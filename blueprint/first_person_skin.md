# Extended Races first-person skin candidate

## October 1 corrected Blueprint and cooked candidate

The authoring source now creates an unattached dynamic material through
`KismetMaterialLibrary`, copies the original component material's parameters,
enables `FPSClippingFix_Enable`, and only then calls `SetMaterial`. The previous
component-level creation node installed the new material first. Its pure
`GetMaterial` source node could therefore read that new material when the copy
executed, losing the race's original parameters.

`author_mod_info.py` now declares `ExtendedRaces.esp` and reads its twelve
required masters from the actual 20-byte TES4 header. The local preflight
passed the real plugin and four invalid-header rejection cases (5/5), and the
existing complete ESP verifier passed with zero structural or unexpected
changes. Receipt: `.work/extended-races-bp-authoring/source-preflight-20261001.json`.

`tools/package_uml_candidate.py` assembles the logic/discovery containers,
existing race-content trio and ESP into a fresh candidate output. It verifies
input hashes, containers, plugin contents and copied bytes. It includes neither
UE4SS nor the loader. It rejected the historical September 24 candidate's
nonzero cook result before creating output. This packager does not turn the
skin-only Blueprint into a complete runtime replacement.

The corrected source compiled and linked in isolation. A fresh authoring run
saved three functions and two events with zero Blueprint errors and warnings.
Metadata binding persisted the logic actor and the ESP's twelve masters. The
isolated v3 cook exited 0 with zero errors and thirteen startup/configuration
warnings (online subsystems, device CVars, collision channels and a Python enum
name collision). Only the two owned packages were cooked. The previous failed
cook and packaging runs remain retained under their original candidate paths.

Current source assets in `blueprint/assets/` match the generated October 1
assets. The race-only content pack excludes the Horns side mod. The combined
candidate and retail voice-overlays are documented in
`docs/2026-10-01-uml-retail-overlays.md`. Retail acceptance and the separate
Body Guard compatibility port remain unfinished. Native Confirm still needs
the existing first-party fourteen-race registration; UML remains the mod
loader and this candidate includes no UE4SS or UNBSE.

The source in `native_authoring/ExtendedRacesSkinCommandlet.{h,cpp}` authors
`/Game/Mods/ExtendedRaces/BP_ExtendedRacesFirstPersonSkin` in the mapped
UnblivionEditor project. `attach_skin_logic.py` assigns that class to the
loader's `LogicModInfo.CustomBPLogicModActor` and preserves **Extended Races** /
**LorexValkin** in the metadata asset. The cooking config is
`.work/extended-races-cooker/skin-cook.json`.

The historical Lua fix first appeared in commit `31b6e27`. Its two operations
are independent: the first-person body skin gets a same-sex Imperial retail
first-person material with race parameters copied and
`FPSClippingFix_Enable=1`; skeletal components on the player's first-person
rig get bounds scale 6. Golden Saint has skin in slot 1; the other three races
use slot 0. The Lua selects only non-shadow components. The installed Lua has a
later scoped component search and guarded callback, but its material and
bounds operations are unchanged from the tracked script.

The Blueprint candidate performs those operations through reflected Unreal
nodes. It uses the local player character and direct attached actors, filters
skeletal components by `CastShadow=false`, and checks for a `_Body_` mesh
before replacing its skin slot. `SetBoundsScale` calls `UpdateBounds` itself in
UE 5.3.2. BeginPlay and a 1.5 second Tick catch body and equipment rebuilds.
The graph does not modify the shadow-casting third-person body. A retail visual
check must still establish the shadow symptom separately from wall clipping.

## Current evidence and blocker

As of 2026-09-24, the editor-only commandlet compiled, authored the Blueprint,
and reported three functions, two events, zero Blueprint errors, and zero
Blueprint warnings in `.work/extended-races-cooker/skin-author.log`. The
Blueprint asset and bound metadata asset are in `blueprint/assets/`.
`skin-bind.log` confirms `CustomBPLogicModActor` persisted and the metadata
still says **Extended Races** / **LorexValkin**.

The two packages cooked into
`.work/extended-races-skin-candidate-20260924`. The cook processed exactly
two packages; its editor exit code was 1 because the reconstruction project
reports unrelated startup/content errors. `retoc verify` passed. The loader
author kit's offline `verify_mod.py` passed layout, container, discovery,
both package entries, and 4/4 release hashes. The build record is under
`_build/`; only the four packaged files are in the release layout. This is an
offline packaging result, **not** retail load or visual proof.

The installed `UnblivionModLoader_P.ucas` SHA-256 is
`1CCDD88E818CD40CF8E99993AA2AB756E29EDB5E87DDBDB5291177AF7EAD3924`;
the public `main` rewrite container is
`DA62CBB7DCDDF7DD4238AC15C70171483CF4AB954377AD145E29D28086A3D197`.
GitHub `main` was checked at `ecb96cd86c824aaf4109a4af7678eb40509ef4cb`.
The rewrite itself says it is unverified in game. With the game closed, the
rewrite and the four-file skin candidate were installed on 2026-09-24. The
post-copy SHA-256 hashes matched the sources. After a 15:11 crash, the previous
loader trio was restored and the four skin files moved out of the game folder
while leaving Snow untouched. The install receipt, rollback receipt, previous
loader files and quarantined candidate are under
`.work/extended-races-skin-install-20260924-150508/`. The installed loader
is again the earlier `1CCDD88E...` build. The 15:11 crash's first two game
stack RVAs (`0xFEF93D`, `0xDDC039`) also occurred in crashes on 2026-09-22,
before this candidate was installed. This signature alone does not identify
which mod caused it. The old Lua skin writer remains installed. Actor spawning
and the wall/shadow behavior remain unverified.

## Completion checks

1. Build the mapped editor commandlet, author the actor, and require zero
   Blueprint compile errors and warnings. Save the resulting mod-owned asset.
2. Bind the class to loader metadata, verify the persisted class and identity,
   then cook only those two mod-owned packages and verify every packaged file.
3. With the game closed, retain rollback copies and hashes before any install.
   Do not change saves or the developer INI. Confirm the loader spawns the actor
   before disabling the old Lua writer if it remains active.
4. In game, test a custom race near a wall with bare arms and with gloves and
   sleeves; test in direct light and shadow separately. Check both Golden
   Saint sexes, the other added races, appearance rebuild, and an inventory
   doll. A cook or log line alone does not prove the rendering result.
