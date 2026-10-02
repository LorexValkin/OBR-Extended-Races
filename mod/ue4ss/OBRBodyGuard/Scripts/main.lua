-- OBR Body Guard (Extended Races)
--
-- Third-party body mods rebuild character bodies through their own rigs and
-- assets. For the ten vanilla races that is their business, but none of them
-- know the four extended races' bodies. Measured with NaturalBodyMorph: its
-- rig has no male Dremora / Dark Seducer / Golden Saint / Sheogorath body,
-- and its rebuild leaves the body's skin slot holding a dynamic material
-- instance whose object has been garbage-collected - the body renders
-- untextured while the mesh itself is untouched.
--
-- This mod does not touch the other mod. Once a second it looks at every
-- character of the four races and repairs what a body mod broke:
--
--   Materials: a body slot holding a dead material or an engine substitute
--   (WorldGridMaterial and kin) is restored - from the material recorded
--   while the body was healthy, or from the race's own skin material, whose
--   asset paths are read out of the game's pak manifest below. A healthy
--   body is snapshotted as the baseline for later repairs.
--
--   Meshes: should a body mod swap the mesh itself, the recorded mesh is put
--   back - but only when the snapshot provably belonged to this race (the
--   mesh name carries the race's token), only after the foreign mesh has
--   survived two consecutive polls (one sighting may be the game's own
--   rebuild mid-flight), and at most MAX_MESH_RESTORES times, after which
--   only materials are repaired. Two mods can never fight at full speed.
--
-- All writes happen from the poll on the game thread, never from an engine
-- hook, and the poll stands down until a player character exists - walking
-- the world during a loading screen touches objects the async loader is
-- still building. Vanilla races are never touched.
--
-- Output goes to ue4ss/UE4SS.log, tagged [BodyGuard].

local TAG = "[BodyGuard]"
local DEBUG = false          -- true: log each guarded character's component
                             -- inventory once (meshes and materials per slot)

-- Race name -> token its body meshes carry (SK_Dremora_Body_m,
-- SK_GoldenSaint_Body_f, ..._Seducer_Body..., ..._Sheogorath_Body...).
local RACE_TOKEN = {
    Dremora = "dremora",
    GoldenSaint = "saint",
    DarkSeducer = "seducer",
    Sheogorath = "sheogorath",
}

local REPAIR_SLOTS = 2       -- body meshes carry skin/underwear in slots 0..1;
                             -- higher slots read back as junk wrappers on
                             -- every component, healthy or not
local POLL_MS = 1000
local MAX_MESH_RESTORES = 3  -- per component; afterwards materials only

-- The races' own body skin materials, by race and sex (0 male, 1 female).
-- Full paths read out of the game's own pak manifest (retoc manifest on
-- OblivionRemastered-Windows.utoc). The Dremora folder really is spelled
-- "Dreamora" - that typo ships in the game.
local SKIN_MIC = {
    Dremora     = { [0] = "/Game/Art/Character/Dreamora/MIC_Dremora_Body.MIC_Dremora_Body",
                    [1] = "/Game/Art/Character/Dreamora/MIC_Dremora_Body_f.MIC_Dremora_Body_f" },
    DarkSeducer = { [0] = "/Game/Art/Character/DarkSeducer/MIC_DarkSeducer_Body_m.MIC_DarkSeducer_Body_m",
                    [1] = "/Game/Art/Character/DarkSeducer/MIC_DarkSeducer_Body_F.MIC_DarkSeducer_Body_F" },
    GoldenSaint = { [0] = "/Game/Art/Character/GoldenSaint/MIC_GoldenSaint_Body_M.MIC_GoldenSaint_Body_M",
                    [1] = "/Game/Art/Character/GoldenSaint/MIC_GoldenSaint_Body_F.MIC_GoldenSaint_Body_F" },
    Sheogorath  = { [0] = "/Game/Art/Character/Sheogorath/MIC_Sheogorath_Body.MIC_Sheogorath_Body" },
}

local function log(fmt, ...)
    local ok, line = pcall(string.format, fmt, ...)
    print(TAG .. " " .. (ok and line or tostring(fmt)) .. "\n")
end

local function nameOf(o)
    if o == nil then return "nil" end
    local ok, valid = pcall(function() return o:IsValid() end)
    if not ok or not valid then return "invalid" end
    local got, n = pcall(function() return o:GetFullName() end)
    return (got and n) or "?"
end

-- "Class /Path/Package.Object" -> "/Path/Package.Object"
local function pathOf(o)
    local full = nameOf(o)
    return full:match("%S+%s+(.+)$")
end

local function short(o)
    local n = nameOf(o)
    return n:match("([^%s/]+)$") or n
end

local function get(o, prop)
    local ok, v = pcall(function() return o[prop] end)
    if ok then return v end
    return nil
end

local function call(o, fn, ...)
    local args = { ... }
    local ok, v = pcall(function() return o[fn](o, table.unpack(args)) end)
    if ok then return v end
    return nil
end

-- Like call, but reports whether the call itself succeeded; needed for
-- functions that return nothing.
local function tryCall(o, fn, ...)
    local args = { ... }
    return (pcall(function() return o[fn](o, table.unpack(args)) end))
end

local function live(o)
    if o == nil then return false end
    local ok, v = pcall(function() return o:IsValid() end)
    return ok and v == true
end

-- The engine substitutes these when a slot has no usable material.
local function isDefaultMaterial(material)
    if material == nil then return false end
    local n = short(material)
    return n:find("WorldGridMaterial") ~= nil
        or n:find("DefaultMaterial") ~= nil
        or n:find("BasicShapeMaterial") ~= nil
end

-- Breakage comes in two shapes: the engine's substitute material, and a slot
-- still holding a dynamic instance whose object has been garbage-collected.
-- A nil slot is fine - the component falls back to the mesh's own material.
local function isBrokenMaterial(material)
    if material == nil then return false end
    if not live(material) then return true end
    return isDefaultMaterial(material)
end

-- A dynamic instance (our FPSkin one, or another mod's) is transient and
-- cannot be found again later; record the first ancestor that lives in a
-- package instead.
local function persistentPathOf(material)
    local m = material
    for _ = 1, 6 do
        if m == nil then return nil end
        local p = pathOf(m)
        if p and p:find("^/") and not p:find("^/Engine/Transient") then return p end
        m = get(m, "Parent")
    end
    return nil
end

local function resolve(path)
    if path == nil then return nil end
    local found = StaticFindObject(path)
    if live(found) then return found end
    local loaded, obj = pcall(function() return LoadAsset(path) end)
    if loaded and live(obj) then return obj end
    return nil
end

local function raceNameOf(character)
    local race = get(character, "Race")
    if race == nil then return nil end
    local ok, n = pcall(function() return race:GetFName():ToString() end)
    if ok then return n end
    return nil
end

local function bodyMeshOf(comp)
    local mesh = get(comp, "SkeletalMeshAsset") or get(comp, "SkeletalMesh")
    if mesh ~= nil and short(mesh):find("_Body") ~= nil then return mesh end
    return nil
end

local function meshMatchesRace(mesh, raceName)
    local token = RACE_TOKEN[raceName]
    return token ~= nil and short(mesh):lower():find(token, 1, true) ~= nil
end

local SkelCompClass = nil
local function skelClass()
    if live(SkelCompClass) then return SkelCompClass end
    SkelCompClass = StaticFindObject("/Script/Engine.SkeletalMeshComponent")
    return SkelCompClass
end

-- Array elements and hook params arrive as parameter wrappers that need an
-- extra :get() before they behave as objects.
local function unwrap(v)
    if v == nil or live(v) then return v end
    local ok, inner = pcall(function() return v:get() end)
    if ok and inner ~= nil and live(inner) then return inner end
    return v
end

-- Only the character's own skeletal mesh components; sweeping the world's
-- full component list touches objects the async loader is still building.
local function componentsOf(character)
    local out = {}
    local cls = skelClass()
    for _, fname in ipairs({ "K2_GetComponentsByClass", "GetComponentsByClass" }) do
        if cls ~= nil then
            local ok, arr = pcall(function() return character[fname](character, cls) end)
            if ok and arr ~= nil then
                if type(arr) == "table" then
                    for _, c in ipairs(arr) do
                        c = unwrap(c)
                        if live(c) then out[#out + 1] = c end
                    end
                else
                    pcall(function()
                        arr:ForEach(function(_, elem)
                            local c = unwrap(elem)
                            if live(c) then out[#out + 1] = c end
                        end)
                    end)
                end
                if #out > 0 then return out, fname end
            end
        end
    end
    local mesh = unwrap(get(character, "Mesh"))
    if mesh ~= nil then
        out[1] = mesh
        return out, "Mesh property"
    end
    return out, "nothing"
end

-- One inventory block per character when DEBUG is on: which components
-- discovery reaches, and what mesh and materials each carries. This is what
-- tells us, from a log alone, what a body mod actually did.
local inspected = {}
local function logInventory(owner, raceName, comps, how)
    if not DEBUG then return end
    local ownerKey = nameOf(owner)
    if inspected[ownerKey] then return end
    inspected[ownerKey] = true
    log("%s %s: %d component(s) via %s", raceName, short(owner), #comps, how)
    for i, comp in ipairs(comps) do
        if i > 12 then log("  ... and %d more", #comps - 12) break end
        local mesh = get(comp, "SkeletalMeshAsset") or get(comp, "SkeletalMesh")
        local mats = {}
        for slot = 0, 3 do
            local m = call(comp, "GetMaterial", slot)
            if m == nil then break end
            mats[#mats + 1] = short(m)
        end
        log("  [%d] %s mesh=%s mats={%s}", i, short(comp),
            mesh ~= nil and short(mesh) or "none", table.concat(mats, ", "))
    end
end

-- key -> { mesh = path, meshShort = name, mats = { [slot] = path } }
local snapshots = {}
local guarded = {}
local mismatchSeen = {}
local meshRestores = {}
local gaveUp = {}

local function keyFor(owner, comp)
    return nameOf(owner) .. "|" .. short(comp)
end

local function raceSkinFor(raceName, sex)
    local bySex = SKIN_MIC[raceName]
    local path = bySex and bySex[sex]
    if path == nil then return nil end
    return resolve(path)
end

local function healthy(comp)
    for slot = 0, REPAIR_SLOTS - 1 do
        local m = call(comp, "GetMaterial", slot)
        if m == nil then break end
        if isBrokenMaterial(m) then return false end
    end
    return true
end

local function snapshot(key, comp, mesh, raceName)
    local entry = { mesh = pathOf(mesh), meshShort = short(mesh), mats = {} }
    if entry.mesh == nil then return end
    for slot = 0, REPAIR_SLOTS - 1 do
        local m = call(comp, "GetMaterial", slot)
        if m == nil then break end
        entry.mats[slot] = persistentPathOf(m)
    end
    snapshots[key] = entry
    if not guarded[key] then
        guarded[key] = true
        log("guarding %s body %s", raceName, entry.meshShort)
    end
end

-- The two body slots are skin and underwear; a broken slot is the skin slot
-- when its neighbour is live underwear (Golden Saint has them the other way
-- round from everyone else, so this is checked, not assumed).
local function looksLikeSkinSlot(comp, slot)
    local other = call(comp, "GetMaterial", 1 - slot)
    if other ~= nil and live(other) and short(other):find("Underwear") ~= nil then
        return true
    end
    return slot == 0
end

local noSource = {}
local function restoreMaterials(comp, mesh, entry, raceName, key, sex)
    for slot = 0, REPAIR_SLOTS - 1 do
        local current = call(comp, "GetMaterial", slot)
        if current == nil then break end
        if isBrokenMaterial(current) then
            local material = resolve(entry and entry.mats[slot] or nil)
            if material == nil and looksLikeSkinSlot(comp, slot) then
                material = raceSkinFor(raceName, sex)
            end
            if material ~= nil then
                call(comp, "SetMaterial", slot, material)
                local now = call(comp, "GetMaterial", slot)
                log("%s: restored material slot %d of %s to %s (reads back %s)",
                    raceName, slot, short(mesh), short(material),
                    now ~= nil and short(now) or "none")
            else
                local missKey = tostring(key) .. "/" .. slot
                if not noSource[missKey] then
                    noSource[missKey] = true
                    log("%s: material slot %d of %s is broken and no restore source was found",
                        raceName, slot, short(mesh))
                end
            end
        end
    end
end

local function restoreMesh(key, comp, entry, raceName, sex)
    local mesh = resolve(entry.mesh)
    if mesh == nil then
        log("%s: mesh was swapped but %s cannot be loaded", raceName, tostring(entry.mesh))
        return
    end
    if not tryCall(comp, "SetSkeletalMeshAsset", mesh) then
        tryCall(comp, "SetSkeletalMesh", mesh, false)
    end
    -- The foreign rig's morph state means nothing on the mesh being put back.
    tryCall(comp, "ClearMorphTargets")
    meshRestores[key] = (meshRestores[key] or 0) + 1
    log("%s: put %s back (a body mod had swapped in a foreign mesh; restore %d of %d)",
        raceName, entry.meshShort, meshRestores[key], MAX_MESH_RESTORES)
    restoreMaterials(comp, mesh, entry, raceName, key, sex)
end

local busy = false
local function sweep()
    if busy then return end
    busy = true
    local ok, chars = pcall(function() return FindAllOf("VPairedCharacter") end)
    if not ok or chars == nil then busy = false return end
    -- No player means menus or a loading screen; the world is not a safe
    -- place to walk yet.
    local worldUp = false
    for _, c in ipairs(chars) do
        local got, isPlayer = pcall(function() return c:IsPlayerCharacter() end)
        if got and isPlayer then worldUp = true break end
    end
    if not worldUp then busy = false return end
    for _, owner in ipairs(chars) do
        local raceName = live(owner) and raceNameOf(owner) or nil
        if raceName ~= nil and RACE_TOKEN[raceName] ~= nil then
            local sex = get(owner, "Sex")
            if type(sex) ~= "number" then sex = 0 end
            local comps, how = componentsOf(owner)
            logInventory(owner, raceName, comps, how)
            for _, comp in ipairs(comps) do
                local mesh = live(comp) and bodyMeshOf(comp) or nil
                if mesh ~= nil then
                    local key = keyFor(owner, comp)
                    if meshMatchesRace(mesh, raceName) then
                        mismatchSeen[key] = nil
                        if snapshots[key] == nil then
                            if healthy(comp) then
                                snapshot(key, comp, mesh, raceName)
                            else
                                -- Broken before a baseline was ever seen: the
                                -- race's own materials are still the truth.
                                restoreMaterials(comp, mesh, nil, raceName, key, sex)
                            end
                        else
                            restoreMaterials(comp, mesh, snapshots[key], raceName, key, sex)
                        end
                    elseif snapshots[key] ~= nil then
                        -- A foreign mesh must survive two consecutive polls before
                        -- it is replaced; one sighting may be the game mid-rebuild.
                        if not mismatchSeen[key] then
                            mismatchSeen[key] = true
                        elseif (meshRestores[key] or 0) < MAX_MESH_RESTORES then
                            mismatchSeen[key] = nil
                            restoreMesh(key, comp, snapshots[key], raceName, sex)
                        else
                            if not gaveUp[key] then
                                gaveUp[key] = true
                                log("%s: a body mod keeps swapping this mesh; leaving the mesh and fixing materials only",
                                    raceName)
                            end
                            restoreMaterials(comp, mesh, nil, raceName, key, sex)
                        end
                    else
                        -- No proven baseline: never swap meshes on guesswork, but a
                        -- broken material is still worth repairing.
                        restoreMaterials(comp, mesh, nil, raceName, key, sex)
                    end
                end
            end
        end
    end
    busy = false
end

local polling = pcall(function()
    LoopAsync(POLL_MS, function()
        pcall(function() ExecuteInGameThread(sweep) end)
        return false
    end)
end)

log("loaded - poll %s", polling and "on" or "OFF")
