# Horns for All 0.2.0-test1: adjustable positioning

Status: **UNVERIFIED in game; Lua fixture checks passed** (2026-09-04).

Reported trigger: horns float above the head in both character creation and
gameplay, including Orc, Khajiit, and Argonian. The existing content offers
shared meshes across races without a position correction.

This candidate adds player-only, per-race/sex/style translation controls to
OBRHornsForAll. Hold Ctrl+Alt with arrows or Page Up/Page Down to adjust in
0.25-unit steps, Backspace to reset, and S to save. New profiles start at zero.
The content pak and existing horn selection/sex restrictions are unchanged.

Current source evidence: the configured `TES4R_1_2_Mappings.usmap` exposes
`VPairedCharacter.HumanoidHeadComponent`, `VHumanoidHeadComponent.HairComponents`,
and `CharacterHairPieceBaseConstructResult.HairMeshComponent` plus its shadow
proxy. `EVFacialHairType::Eyebrows` is 3. The hair-piece schema supports mesh
selection by race/sex but exposes no translation property. The implementation
uses the reflected component location setter with no sweep and teleport enabled.
See [Epic's setter contract](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Components/USceneComponent/K2_SetRelativeLocation?application_version=5.5).

Validation: `mod-horns/tests/test_offsets.py` runs the actual Lua modules under
Lupa's Lua 5.4 against mock UE4SS objects. It covers non-accumulating refresh,
reset, component replacement, race/sex/style isolation, unknown mesh exclusion,
player-only selection, shadow movement, bounds, save/reload, malformed settings,
and rollback after a failed settings-file promotion. This is fixture evidence,
not proof of UE4SS marshalling or rendering. Lua language-server diagnostics
were unavailable because `lua-language-server` is not installed; Lua compilation
checks ran through Lua 5.4 instead.

Runtime blocker: Computer Use initialization returned `Computer Use native pipe
is unavailable ... The system cannot find the file specified. (os error 2)`.
No game-runtime or visual approval is claimed.

Next proof: tune one visible floating set, starting with male Orc, then check
creator/gameplay, head rotation and animation, appearance rebuild, save/restart,
and shadow placement. Repeat the fitted values for Argonian, Khajiit, and female
Dremora where reported. Parent-relative translation may not correct all animated
skinning differences; no preset is promoted without that visual check.

Rollback: restore the prior `Scripts/main.lua` and remove the added
`horn_offsets.lua` / `offset_profiles.lua` modules while the game is closed.
The optional `horn-offsets.ini` is independent of game saves and can be retained
for a future reinstall. Restarting without the offset module returns the native
placement; no asset or save-file migration is required.
