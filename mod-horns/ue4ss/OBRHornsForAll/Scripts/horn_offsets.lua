-- Player-only translation of the existing Eyebrows slot. No mesh/skeleton edits.
local Profiles = require("offset_profiles")
local M = {}
local ZERO = {X=0, Y=0, Z=0}
local MESHES = {}
local function mesh(path, name, style)
    MESHES[path .. name .. "." .. name] = style
end
for _, item in ipairs({
    {"DremoraA", "Horns"}, {"DremoraB", "HornsCurved"},
    {"Lord", "HornsOfTheKyn"}, {"Female", "HornsSwept"},
}) do
    for _, suffix in ipairs({"_HFA", "_HornsOnly"}) do
        mesh("/Game/Dev/Phenotypes/Meshes/", "SK_Dremora_HR_" .. item[1] .. suffix, item[2])
    end
end
for _, item in ipairs({
    {"StraightHorns", "StraightHorns"}, {"CurvedHorns", "CurvedHorns"},
    {"DualForeheadHorns", "ForeheadSpikes"},
    {"DualForeheadHorns_jeweled", "ForeheadSpikesJeweled"},
}) do
    for _, suffix in ipairs({"", "_f"}) do
        mesh("/Game/Art/Character/Hair/Argonian/", "SK_Argonian_HR_" .. item[1] .. suffix, item[2])
    end
end

local function live(obj)
    if obj == nil then return false end
    local ok, valid = pcall(function() return obj:IsValid() end)
    return ok and valid == true
end
local function get(obj, field)
    local ok, value = pcall(function() return obj[field] end)
    if ok then return value end
end
local function call(obj, method, ...)
    local args = {...}
    local ok, value = pcall(function() return obj[method](obj, table.unpack(args)) end)
    if ok then return value end
end
local function path(obj)
    if not live(obj) then return nil end
    local full = call(obj, "GetFullName")
    return full and full:match("^[^ ]+ (.+)$")
end
local function vector(comp)
    local ok, value = pcall(function() return Profiles.vector(comp.RelativeLocation) end)
    if ok then return value end
end

function M.start(directory, log)
    local profilePath = directory .. "horn-offsets.ini"
    local profiles, loadError = Profiles.load(profilePath)
    -- Never overwrite unreadable/malformed user settings with an empty table.
    local storageReady = profiles ~= nil
    profiles = profiles or {}
    if loadError then log("offset settings not loaded; saving disabled: %s", tostring(loadError)) end
    local states, warned = {}, {}
    local function warn(key, message)
        if not warned[key] then warned[key] = true; log("offsets: %s", message) end
    end
    local function player()
        local ok, actors = pcall(FindAllOf, "VPairedCharacter")
        if not ok or not actors then return nil end
        local found
        for _, actor in ipairs(actors) do
            if live(actor) and call(actor, "IsPlayerCharacter") == true then
                if found then return nil end -- do not guess between worlds/players
                found = actor
            end
        end
        return found
    end
    local function selection()
        local actor = player()
        if not actor then return nil end
        local head = get(actor, "HumanoidHeadComponent")
        if not live(head) then return nil end
        local pair
        local ok, err = pcall(function()
            pair = head.HairComponents:Find(3):get() -- EVFacialHairType::Eyebrows
        end)
        if not ok then
            warn("slot", "cannot read Eyebrows slot: " .. tostring(err))
            return nil
        end
        local comp = get(pair, "HairMeshComponent")
        if not live(comp) then return nil end
        if path(call(comp, "GetOwner")) ~= path(actor) then
            warn("owner", "Eyebrows component owner does not match the player; no adjustment applied")
            return nil
        end
        local asset = get(comp, "SkeletalMeshAsset")
        if not live(asset) then asset = get(comp, "SkeletalMesh") end
        local assetPath = path(asset)
        local style = MESHES[assetPath]
        local racePath = path(get(actor, "Race"))
        local race = racePath and racePath:match("%.([%w_]+)$")
        local sexValue = get(actor, "Sex")
        local sex = sexValue == 0 and "male" or sexValue == 1 and "female" or nil
        local key = style and race and sex and (race .. "|" .. sex .. "|" .. style) or nil
        local components = {comp}
        local proxy = get(pair, "HairMeshShadowProxyComponent")
        if live(proxy) and path(proxy) ~= path(comp) then components[#components+1] = proxy end
        return {key=key, components=components, actorPath=path(actor), mesh=assetPath}
    end
    local function apply(selected)
        -- A transient lookup failure must not forget a still-applied offset.
        -- Only plain numeric/name state is retained, never a UObject reference.
        if not selected then return end
        local nextStates = {}
        if selected then
            for _, comp in ipairs(selected.components) do
                local id = path(comp)
                local ownerPath = path(call(comp, "GetOwner"))
                local current = vector(comp)
                if id and current and ownerPath and ownerPath == selected.actorPath then
                    local previous = states[id]
                    -- A different UObject at a reused name is a new baseline.
                    local address = call(comp, "GetAddress")
                    if previous and (previous.address ~= address or previous.owner ~= ownerPath) then previous = nil end
                    local offset = selected.key and profiles[selected.key] or ZERO
                    -- An unknown replacement mesh may only have OUR prior translation undone.
                    if selected.key or previous then
                        local state = Profiles.target(previous, current, offset or ZERO)
                        if not Profiles.same(current, state.applied) then
                            local ok, err = pcall(function()
                                comp:K2_SetRelativeLocation(state.applied, false, {}, true)
                            end)
                            local observed = vector(comp)
                            if not ok or not Profiles.same(observed, state.applied) then
                                warn("write:" .. id, "location write failed/readback differed: " .. tostring(err or id))
                                -- A partial native write is still ours: preserve the original
                                -- baseline and retry the absolute target, never add again.
                                if observed then state.applied = observed else state = previous end
                            end
                        end
                        if state then
                            state.address, state.owner = address, ownerPath
                            nextStates[id] = state
                        end
                    end
                end
            end
        end
        states = nextStates -- discard destroyed components; never accumulate per-spawn records
    end
    local function tick() apply(selection()) end
    local pending = false
    local function queueTick()
        if pending then return end
        pending = true
        local ok, err = pcall(function()
            ExecuteInGameThread(function()
                pending = false
                local applied, failure = pcall(tick)
                if not applied then warn("tick", tostring(failure)) end
            end)
        end)
        if not ok then pending = false; warn("queue", tostring(err)) end
    end
    local function adjust(axis, delta, reset)
        local selected = selection()
        if not selected or not selected.key then
            log("offsets: select a Horns for All style on the player first")
            return
        end
        local offset = Profiles.vector(profiles[selected.key] or ZERO)
        if reset then offset = Profiles.vector(ZERO)
        else offset[axis] = math.max(-Profiles.LIMIT, math.min(Profiles.LIMIT, offset[axis] + delta)) end
        profiles[selected.key] = offset
        apply(selected)
        log("offset preview %s: X=%.2f Y=%.2f Z=%.2f; Ctrl+Alt+S saves", selected.key, offset.X, offset.Y, offset.Z)
    end
    local function save()
        if not storageReady then log("offsets: repair horn-offsets.ini and restart before saving"); return end
        local ok, err = Profiles.save(profilePath, profiles)
        if ok then log("offsets saved to %s", profilePath)
        else log("offset save FAILED: %s", tostring(err)) end
    end
    local function bind(key, action)
        local ok, err = pcall(function()
            RegisterKeyBind(key, {ModifierKey.CONTROL, ModifierKey.ALT}, function()
                ExecuteInGameThread(function()
                    local done, failure = pcall(action)
                    if not done then log("offset control failed: %s", tostring(failure)) end
                end)
            end)
        end)
        if not ok then log("offset key registration failed: %s", tostring(err)) end
    end
    bind(Key.UP_ARROW, function() adjust("Z", Profiles.STEP) end)
    bind(Key.DOWN_ARROW, function() adjust("Z", -Profiles.STEP) end)
    bind(Key.RIGHT_ARROW, function() adjust("X", Profiles.STEP) end)
    bind(Key.LEFT_ARROW, function() adjust("X", -Profiles.STEP) end)
    bind(Key.PAGE_UP, function() adjust("Y", Profiles.STEP) end)
    bind(Key.PAGE_DOWN, function() adjust("Y", -Profiles.STEP) end)
    bind(Key.BACKSPACE, function() adjust(nil, nil, true) end)
    bind(Key.S, save)
    for _, target in ipairs({
        "/Script/Altar.VPairedCharacter:InitializeAppearanceFromForm",
        "/Script/Altar.VPairedCharacter:SetRace",
        "/Script/Altar.VPairedCharacter:SetSex",
        "/Script/Altar.VRaceSexMenuViewModel:UpdateCustomisationTarget",
    }) do
        local ok, err = pcall(RegisterHook, target, function() end, queueTick)
        if not ok then log("offset refresh hook unavailable (%s); poll will retry: %s", target, tostring(err)) end
    end
    local ok, err = pcall(function()
        LoopAsync(250, function() queueTick(); return false end)
    end)
    if not ok then log("offset polling FAILED: %s", tostring(err)) end
    queueTick()
    log("offset controls loaded: Ctrl+Alt+arrows/PageUp/PageDown, Backspace reset, S save")
end

return M
