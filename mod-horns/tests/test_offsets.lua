-- Run with Lua 5.4, or test_offsets.py (Lupa). All Unreal objects are fixtures.
package.path = TEST_SCRIPTS .. "/?.lua;" .. package.path
local P = require("offset_profiles")
local checks = 0
local function check(value, message)
    assert(value, message)
    checks = checks + 1
end
local function equal(a, b, message) check(P.same(a,b), message) end
local zero = {X=0,Y=0,Z=0}
local base = {X=3,Y=4,Z=5}
local offset = {X=0,Y=0,Z=-1}
local state = P.target(nil, base, offset)
for _ = 1, 100 do state = P.target(state, state.applied, offset) end
equal(state.applied, {X=3,Y=4,Z=4}, "offset must not accumulate")
equal(P.target(state, state.applied, zero).applied, base, "reset restores baseline")
equal(P.target(state, base, offset).applied, {X=3,Y=4,Z=4}, "native reset gets one offset")
equal(P.target(state, {X=9,Y=8,Z=7}, offset).applied, {X=9,Y=8,Z=6}, "adopt native movement")
local profiles = { ["Orc|male|Horns"] = offset, ["Dremora|female|HornsSwept"] = {X=1,Y=0,Z=-2} }
equal(assert(P.decode(P.encode(profiles)))["Orc|male|Horns"], offset, "profile round trip")
for _, bad in ipairs({
    "Orc|male|Horns=0,0,nan", "Orc|male|Horns=0,0,inf", "Orc|male|Horns=0,0,11",
    "Orc|other|Horns=0,0,1", "broken", "Orc|male|Horns=0,0,1\nOrc|male|Horns=0,0,2",
}) do check(P.decode(bad) == nil, "reject malformed settings: " .. bad) end
check(P.vector({X=0,Y=0,Z=0/0}) == nil, "reject NaN")
check(P.vector({X=0,Y=0,Z=math.huge}) == nil, "reject infinity")

local file = TEST_TEMP .. "/storage.ini"
check(next(assert(P.load(file))) == nil, "missing file starts empty")
check(P.save(file, profiles), "save initial profiles")
profiles["Orc|male|Horns"] = {X=0,Y=0,Z=-3}
check(P.save(file, profiles), "replace existing file")
equal(assert(P.load(file))["Orc|male|Horns"], profiles["Orc|male|Horns"], "load persisted value")
equal(assert(P.load(file .. ".bak"))["Orc|male|Horns"], offset, "previous settings backed up")
local rename = os.rename
os.rename = function(from, to)
    if from == file .. ".tmp" then return nil, "fixture rename failure" end
    return rename(from, to)
end
check(not P.save(file, {}), "failed promotion reported")
os.rename = rename
equal(assert(P.load(file))["Orc|male|Horns"], profiles["Orc|male|Horns"], "failed promotion restores prior file")

local function object(full, fields)
    fields = fields or {}
    fields.valid = true
    function fields:IsValid() return self.valid end
    function fields:GetFullName() return full end
    function fields:GetAddress() return self.address or full end
    return fields
end
local function asset(name) return object("SkeletalMesh " .. name) end
local hornPath = "/Game/Dev/Phenotypes/Meshes/SK_Dremora_HR_DremoraA_HFA.SK_Dremora_HR_DremoraA_HFA"
local sweptPath = "/Game/Dev/Phenotypes/Meshes/SK_Dremora_HR_Female_HFA.SK_Dremora_HR_Female_HFA"
local actor = object("VPairedCharacter /Game/Test.Player", {
    Race=object("Race /Game/Forms/actors/race/Orc.Orc"), Sex=0,
})
function actor:IsPlayerCharacter() return true end
local comp = object("SkeletalMeshComponent /Game/Test.Player.Horns", {
    RelativeLocation=P.vector(base), SkeletalMeshAsset=asset(hornPath), writes=0,
})
function comp:GetOwner() return actor end
function comp:K2_SetRelativeLocation(v, sweep, hit, teleport)
    check(sweep == false and teleport == true, "cosmetic move uses no sweep and preserves velocity")
    self.writes = self.writes + 1
    if self.fail then return end
    self.RelativeLocation = P.vector(v)
end
local shadow = object("SkeletalMeshComponent /Game/Test.Player.HornShadow", {
    RelativeLocation={X=1,Y=2,Z=3}, writes=0,
})
shadow.GetOwner, shadow.K2_SetRelativeLocation = comp.GetOwner, comp.K2_SetRelativeLocation
local hairstyle = object("SkeletalMeshComponent /Game/Test.Player.Hair", {RelativeLocation=P.vector(base), writes=0})
local pair = {HairMeshComponent=comp, HairMeshShadowProxyComponent=shadow}
actor.HumanoidHeadComponent = object("VHumanoidHeadComponent /Game/Test.Player.Head", {HairComponents={}})
function actor.HumanoidHeadComponent.HairComponents:Find(slot)
    check(slot == 3, "only eyebrows slot accessed")
    return {get=function() return pair end}
end
local npc = object("VPairedCharacter /Game/Test.NPC")
function npc:IsPlayerCharacter() return false end
local actors = {npc, actor}
function FindAllOf(class) check(class == "VPairedCharacter", "no global mesh scan"); return actors end
local queue, bindings, hooks, polls, logs = {}, {}, {}, {}, {}
function ExecuteInGameThread(fn) queue[#queue+1] = fn end
function RegisterKeyBind(key, modifiers, fn) bindings[key] = fn end
function RegisterHook(name, pre, post)
    hooks[name] = hooks[name] or {}
    table.insert(hooks[name], {pre,post})
end
function LoopAsync(interval, fn) polls[#polls+1] = fn end
Key = {UP_ARROW=1,DOWN_ARROW=2,LEFT_ARROW=3,RIGHT_ARROW=4,PAGE_UP=5,PAGE_DOWN=6,BACKSPACE=7,S=8}
ModifierKey = {CONTROL=1,ALT=2}
local function drain()
    while #queue > 0 do local jobs=queue; queue={}; for _, fn in ipairs(jobs) do fn() end end
end
local function press(key) assert(bindings[key], "missing key")( ); drain() end
local function poll() for _, fn in ipairs(polls) do fn() end; drain() end
local function log(fmt, ...) logs[#logs+1] = string.format(fmt, ...) end
require("horn_offsets").start(TEST_TEMP .. "/", log)
drain()
check(comp.writes == 0, "zero default does not move component")
press(Key.DOWN_ARROW)
equal(comp.RelativeLocation, {X=3,Y=4,Z=4.75}, "nudge lowers selected horns")
equal(shadow.RelativeLocation, {X=1,Y=2,Z=2.75}, "shadow moves by same offset")
local writes = comp.writes
for _=1,50 do poll() end
check(comp.writes == writes, "polling performs no redundant writes")
check(hairstyle.writes == 0, "hairstyle untouched")
actors = {}; poll(); actors = {npc,actor}; poll()
equal(comp.RelativeLocation, {X=3,Y=4,Z=4.75}, "transient player absence does not double shift")
comp.RelativeLocation = P.vector(base); poll()
equal(comp.RelativeLocation, {X=3,Y=4,Z=4.75}, "reapply after native refresh")
press(Key.S)
equal(assert(P.load(TEST_TEMP .. "/horn-offsets.ini"))["Orc|male|Horns"], {X=0,Y=0,Z=-0.25}, "save detected identity")
actor.Race = object("Race /Game/Forms/actors/race/Khajiit.Khajiit"); poll()
equal(comp.RelativeLocation, base, "different race has independent zero default")
actor.Race = object("Race /Game/Forms/actors/race/Orc.Orc"); actor.Sex=1; poll()
equal(comp.RelativeLocation, base, "different sex has independent zero default")
actor.Sex=0; poll()
equal(comp.RelativeLocation, {X=3,Y=4,Z=4.75}, "return restores Orc male profile")
comp.SkeletalMeshAsset = asset(sweptPath); poll()
equal(comp.RelativeLocation, base, "different horn style has independent zero default")
press(Key.DOWN_ARROW); press(Key.DOWN_ARROW)
equal(comp.RelativeLocation, {X=3,Y=4,Z=4.5}, "female mesh can be identified without changing sex gates")
comp.SkeletalMeshAsset = asset("/Game/Other.Unknown"); poll()
equal(comp.RelativeLocation, base, "unknown replacement only undoes our old offset")
writes=comp.writes; press(Key.DOWN_ARROW)
check(comp.writes == writes, "unrecognized mesh cannot be adjusted")
comp.SkeletalMeshAsset = asset(hornPath); poll()
comp.address="replacement"; comp.RelativeLocation={X=5,Y=6,Z=7}; poll()
equal(comp.RelativeLocation, {X=5,Y=6,Z=6.75}, "replacement component uses its own baseline")
press(Key.BACKSPACE)
equal(comp.RelativeLocation, {X=5,Y=6,Z=7}, "reset reverses current correction")
for _=1,45 do press(Key.UP_ARROW) end
equal(comp.RelativeLocation, {X=5,Y=6,Z=17}, "adjustments clamped to ten units")
press(Key.BACKSPACE)
comp.fail=true; press(Key.DOWN_ARROW)
equal(comp.RelativeLocation, {X=5,Y=6,Z=7}, "failed write cannot claim moved transform")
comp.fail=false; poll()
equal(comp.RelativeLocation, {X=5,Y=6,Z=6.75}, "failed write retried without accumulating")
press(Key.S)
-- A new Lua session reloads profiles; game provides unmodified new components.
comp.RelativeLocation={X=5,Y=6,Z=7}; shadow.RelativeLocation={X=1,Y=2,Z=3}
polls={}; hooks={}; bindings={}
require("horn_offsets").start(TEST_TEMP .. "/", log); drain()
equal(comp.RelativeLocation, {X=5,Y=6,Z=6.75}, "saved correction survives restart")
check(#logs > 0, "controls provide diagnostic output")
local priorWrites=comp.writes
local realOwner=comp.GetOwner
comp.GetOwner=function() return npc end
press(Key.DOWN_ARROW)
check(comp.writes == priorWrites, "foreign-owned component cannot be moved")
comp.GetOwner=realOwner
-- Exercise the release entrypoint, including the pre/post dispatch restoration.
local printOriginal=print
local startup={}
print=function(message) startup[#startup+1]=message end
assert(loadfile(TEST_SCRIPTS .. "/main.lua"))()
print=printOriginal
check(not table.concat(startup,"\n"):find("FAILED"), "entrypoint loads offset modules")
local dispatch=hooks["/Script/Altar.VRaceSexMenuViewModel:UpdateCustomisationTarget"]
local property={Type=7}
for _, hook in ipairs(dispatch) do hook[1](nil,property) end
check(property.Type == 5, "existing horn selection dispatch still works")
for _, hook in ipairs(dispatch) do hook[2](nil,property) end
check(property.Type == 7, "existing dispatch restores cached type")
print(string.format("PASS: %d assertions (Lua 5.4 fixtures; no game-runtime claim)",checks))
