-- OBR Horns for All
--
-- Makes the character creator's Horns row apply, for every race. The content
-- pak adds the row to each race's customisation table with toggle type
-- EyebrowsStyle, the one head slot no shipped content uses, and the shipping
-- dispatcher does nothing with that type. Rewriting it to BeardStyle for the
-- length of the dispatch call runs the native path exactly as it does for a
-- beard; the pieces themselves are VCharacterHairPiece_Eyebrows and land in
-- the eyebrows slot, so hair, beard and moustache are untouched. The rewrite
-- is undone in the post hook: the property is the menu's cached row, and
-- leaving it changed makes the commit step index the Horns row as the Beard
-- row and crash.
--
-- Safe alongside Extended Races' OBRDremoraHorns: whichever pre-hook runs
-- first does the rewrite, the other sees a type it does not handle and
-- returns, and only the one that swapped restores.
--
-- Output goes to ue4ss/UE4SS.log, tagged [HornsForAll].

local TAG = "[HornsForAll]"

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
    log("loaded - Horns row enabled for all races")
else
    log("FAILED to hook UpdateCustomisationTarget; the Horns row will not apply")
end

-- Keep positioning optional to the selection hook: an offset-module error
-- must not prevent the existing Horns row from working.
local offsetsLoaded, offsetsError = pcall(function()
    local source = debug.getinfo(1, "S").source
    local directory = source:match("^@(.+[/\\])")
    assert(directory, "cannot resolve the mod's Scripts directory")
    require("horn_offsets").start(directory, log)
end)
if not offsetsLoaded then log("offset system FAILED to start: %s", tostring(offsetsError)) end
