-- OBR First Person Skin (Extended Races)
--
-- The added races' skin materials were never put on the game's first-person
-- clipping-fix template, and their first-person bounds can end at the camera.
-- Near a wall the gauntlets and sleeves moved while the arm did not, and
-- pieces were culled out of view. Both are fixed here on the first-person rig
-- only; third person is untouched.
--
-- Skin: each *_Body* slot on the first-person body gets a dynamic instance of
-- the retail first-person skin of the same sex, with every parameter of the
-- race's own material copied across so the race keeps its look.
-- Bounds: every first-person component's BoundsScale is raised so nothing is
-- culled for having a bound that ends at the camera.
--
-- Output goes to ue4ss/UE4SS.log, tagged [FPSkin].

local TAG = "[FPSkin]"

local RACES = { Dremora = true, GoldenSaint = true, DarkSeducer = true, Sheogorath = true }

local DONOR = {
    [0] = "/Game/Art/Character/Imperial/MIC_Imperial_Body_m.MIC_Imperial_Body_m",
    [1] = "/Game/Art/Character/Imperial/MIC_Imperial_Body_F.MIC_Imperial_Body_F",
}

local BOUNDS_SCALE = 6.0

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

local function live(o)
    if o == nil then return false end
    local ok, v = pcall(function() return o:IsValid() end)
    return ok and v == true
end

local function findPlayer()
    local ok, list = pcall(function() return FindAllOf("VPairedCharacter") end)
    if not ok or list == nil then return nil end
    for _, c in ipairs(list) do
        local got, isPlayer = pcall(function() return c:IsPlayerCharacter() end)
        if got and isPlayer then return c end
    end
    return nil
end

-- Every skeletal mesh component on the player's first-person rig: no shadow,
-- owned by the player or by one of its equipment child actors.
local function firstPersonComponents(player)
    local ok, comps = pcall(function() return FindAllOf("SkeletalMeshComponent") end)
    if not ok or comps == nil then return {} end
    local playerName = nameOf(player)
    local out = {}
    for _, comp in ipairs(comps) do
        if live(comp) and get(comp, "CastShadow") == false then
            local owner = call(comp, "GetOwner")
            local mine = owner ~= nil and nameOf(owner) == playerName
            if not mine and owner ~= nil then
                local parent = call(owner, "GetParentActor") or call(owner, "GetAttachParentActor") or call(owner, "GetOwner")
                mine = parent ~= nil and nameOf(parent) == playerName
            end
            if mine then out[#out + 1] = comp end
        end
    end
    return out
end

-- The first-person body is the component on the player itself that carries a
-- body mesh.
local function isBody(comp, playerName)
    local owner = call(comp, "GetOwner")
    if owner == nil or nameOf(owner) ~= playerName then return false end
    local mesh = get(comp, "SkeletalMeshAsset") or get(comp, "SkeletalMesh")
    return mesh ~= nil and short(mesh):find("_Body_") ~= nil
end

-- Skin slots are found by material, not index: the Golden Saint bodies carry
-- underwear in slot 0 and skin in slot 1, the other way round from every other
-- body.
local function isSkin(material)
    local m = material
    for _ = 1, 6 do
        if m == nil then return false end
        local n = short(m)
        if n:find("Underwear") then return false end
        if n:find("_Body") then return true end
        m = get(m, "Parent")
    end
    return false
end

local function alreadyConverted(material)
    return short(material):find("FPSkin") ~= nil
end

local function loadDonor(path)
    local d = StaticFindObject(path)
    if live(d) then return d end
    local loaded, obj = pcall(function() return LoadAsset(path) end)
    if loaded and live(obj) then return obj end
    return nil
end

local announced = {}
local scaled = {}

local function convertSkin(comp, raceName, sex)
    local donor = nil
    for slot = 0, 3 do
        local current = call(comp, "GetMaterial", slot)
        if current == nil then break end
        if not alreadyConverted(current) and isSkin(current) then
            donor = donor or loadDonor(DONOR[sex])
            if donor == nil then return end
            local created = call(comp, "CreateDynamicMaterialInstance", slot, donor, FName("FPSkin"))
            if created ~= nil then
                pcall(function() created:K2_CopyMaterialInstanceParameters(current, false) end)
                pcall(function() created:SetScalarParameterValue(FName("FPSClippingFix_Enable"), 1.0) end)
                local key = raceName .. "/" .. tostring(sex) .. "/" .. slot
                if not announced[key] then
                    announced[key] = true
                    log("%s %s: first-person skin slot %d -> instance of %s", raceName,
                        sex == 1 and "female" or "male", slot, short(donor))
                end
            end
        end
    end
end

local function widenBounds(comp)
    local current = get(comp, "BoundsScale")
    if current == nil or current >= BOUNDS_SCALE then return end
    pcall(function() comp.BoundsScale = BOUNDS_SCALE end)
    pcall(function() comp:UpdateBounds() end)
    local key = nameOf(comp)
    if not scaled[key] then
        scaled[key] = true
        log("bounds x%.0f on %s", BOUNDS_SCALE, short(comp))
    end
end

local function apply()
    local player = findPlayer()
    if player == nil then return end
    local race = get(player, "Race")
    local raceName = race and select(2, pcall(function() return race:GetFName():ToString() end))
    if raceName == nil or not RACES[raceName] then return end
    local sex = get(player, "Sex")
    if DONOR[sex] == nil then return end

    local playerName = nameOf(player)
    for _, comp in ipairs(firstPersonComponents(player)) do
        if isBody(comp, playerName) then convertSkin(comp, raceName, sex) end
        widenBounds(comp)
    end
end

-- The first-person rig is rebuilt on every appearance refresh and on load,
-- which hands it the race material again; the poll catches every rebuild.
local polling = pcall(function()
    LoopAsync(1500, function()
        pcall(function() ExecuteInGameThread(apply) end)
        return false
    end)
end)

for _, target in ipairs({
    "/Script/Altar.VPairedCharacter:InitializeAppearanceFromForm",
    "/Script/Altar.VPairedCharacter:SetRace",
    "/Script/Altar.VPairedCharacter:SetSex",
}) do
    pcall(function() RegisterHook(target, function() end, function() pcall(apply) end) end)
end

log("loaded - poll %s", polling and "on" or "OFF")
