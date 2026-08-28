-- OBR Dremora Horns (Extended Races)
--
-- Makes the character creator's Horns row apply. The content pak adds the row
-- to DT_RaceSex_Dremora with toggle type EyebrowsStyle, the one head slot no
-- shipped content uses, and the shipping dispatcher does nothing with that
-- type. Rewriting it to BeardStyle for the length of the dispatch call runs
-- the native path exactly as it does for a beard; the pieces themselves are
-- VCharacterHairPiece_Eyebrows and land in the eyebrows slot, so hair, beard
-- and moustache are untouched. The rewrite is undone in the post hook: the
-- property is the menu's cached row, and leaving it changed makes the commit
-- step index the Horns row as the Beard row and crash.
--
-- Output goes to ue4ss/UE4SS.log, tagged [DremoraHorns].

local TAG = "[DremoraHorns]"

-- ELegacyRaceSexMenuToggleType
local EYEBROWS_STYLE = 7
local STANDIN_STYLE = 5

local function log(fmt, ...)
    local ok, line = pcall(string.format, fmt, ...)
    print(TAG .. " " .. (ok and line or tostring(fmt)) .. "\n")
end

local function unwrap(param)
    if param == nil then return nil end
    local ok, value = pcall(function() return param:get() end)
    if ok then return value end
    return param
end

local announced = false
local swappedThisCall = false

local function beforeDispatch(self, Property)
    swappedThisCall = false
    local properties = unwrap(Property)
    local ok, kind = pcall(function() return properties.Type end)
    if not ok or kind ~= EYEBROWS_STYLE then return end

    swappedThisCall = pcall(function() properties.Type = STANDIN_STYLE end)
    if not announced then
        announced = true
        local readback = select(2, pcall(function() return properties.Type end))
        log("Horns row live: toggle type %d -> %d (%s, reads back %s)",
            EYEBROWS_STYLE, STANDIN_STYLE, swappedThisCall and "written" or "FAILED",
            tostring(readback))
    end
end

local function afterDispatch(self, Property)
    if not swappedThisCall then return end
    swappedThisCall = false
    local properties = unwrap(Property)
    pcall(function() properties.Type = EYEBROWS_STYLE end)
end

local registered = pcall(function()
    RegisterHook("/Script/Altar.VRaceSexMenuViewModel:UpdateCustomisationTarget",
        beforeDispatch, afterDispatch)
end)

if registered then
    log("loaded - Horns row enabled for Dremora")
else
    log("FAILED to hook UpdateCustomisationTarget; the Horns row will not apply")
end
