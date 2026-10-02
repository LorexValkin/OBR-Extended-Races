# Extended Races with the current Unblivion Mod Loader

## Goal and current boundary

Keep the four playable races, character creation and Horns menus, voices, dialogue,
first-person skin and body compatibility while moving supported runtime behavior
to a loader owned Blueprint actor. `ExtendedRaces.esp` and the cooked content
remain data inputs. The current GitHub `main` is
`ecb96cd86c824aaf4109a4af7678eb40509ef4cb` (2026-09-22). Its
`modloader-rewrite` scans `/Game/Mods` for an **instance** of `PDA_ModInfo`,
reads `ModInfo` (`S_ModInfo`), and spawns `LogicModInfo.CustomBPLogicModActor`
when supplied. `ModName` and `ModAuthor` are identity fields on that struct.
The previous H11 `BP_SyncMod` entry and `LoadMod` API are obsolete for this
target. The rewrite's own runtime README calls it work in progress and says it
has not yet been verified in game.

The owner has now specified **no UE4SS dependency** and wants Blueprint to own
the mod behavior, with additional engine code allowed where Blueprint lacks an
API. This changes the required runtime deliverable: any added engine API must be
present in the *installed retail game*, then called by the cooked Blueprint.
Editor-only native code does not meet that condition.

### Altar mapping check (2026-09-24)

`OBR_Alter_Map`'s `AltarMap01` maps `UVRaceSexMenuViewModel::GetRaceId` to
`0x14491e370` and `UpdateCustomisationTarget` to `0x14493d590`. Both are
`Final Native` functions in the shipping reflection map, although callable
from Blueprint. The mapping lists `AVPairedCharacter::GetRace`, `GetSex`,
`SetVoiceType`, and the `OnAppearanceRefreshedEnd` delegate as Blueprint
surfaces useful for the cosmetic actor. The race selection table is compiled
into the shipping executable and has only ten names. The retail `GetRaceId`
reads an entry in the view model; character creation Confirm uses the separate
compiled lookup. The `GetIsRace` evaluator and player-only voice path likewise
need their specific native behavior changed or bypassed at a verified caller.

The canonical `D:/Unblivion Editor/Editor` project contains the mapped Altar
declarations, but its `UnblivionEditor.Target.cs` is **Editor only**. Its
`UVRaceSexMenuViewModel::GetRaceId` is currently a reconstruction stub returning
zero. Compiling an engine or Altar change there cannot alter the installed
`OblivionRemastered-Win64-Shipping.exe`. The public loader's current `main`
ships cooked Blueprint assets and has no native runtime module. A retail-loadable
engine extension or a demonstrated Blueprint/ESP bypass for every native call is
the remaining prerequisite for a UE4SS-free complete conversion. No such
complete set of extensions or bypasses has been built or installed yet.

The owner has rejected a separate new proxy DLL and directed use of the loader's
Blueprint runtime and existing native in-game functions. On September 24 the
owner also authorized extending the existing first-party `version.dll`, which
already hosts the ESL work, for native gaps and planned OBSE64-style script
extensions. In the 3,538-function
AltarMap01 function list, `SetRace` is callable, but there is no reflected
`RegisterRace`, `SetRaceId`, `SetRaceAlias`, or `GetIsRace` evaluator setter.
The current retail `GetRaceId` implementation reads the view model's selected
race entry; the Confirm callback separately resolves the race name through its
compiled map at `0x149309240`. Changing the actor's race with `SetRace` does
not change the native evaluator's exact race-form test. The targeted dialogue
conditions now have an ESP route described below. A metadata-only asset was
cooked and validated offline, but installing it now would advertise a
conversion that is not present; no Extended Races loader payload was installed.

### Retail dump MCP follow-up (2026-09-24)

`D:/Dump Experiment/FullDump` is exposed through the read-only
`obr-dump-cache` MCP. Its captured executable SHA-256
`B7BE7E6EBE9424F6FDF274F5E7A59372DE103E043AB5B66C60E021C0C89DF457`
matches the installed shipping EXE. The full capture is reported verified with
dispositions; the searchable index is paused, so a name-search miss alone is not
proof of absence. The following exact-address bodies are available:

- Confirm handler `0x14490FFA0` looks up the selected race name in the static
  map at `0x149309240` and passes the resulting integer to
  `RaceSex_ClickDone_Handler`. The caller `0x144928AA0` registers this handler.
  Initializer `0x140B6F790` inserts exactly ten hard-coded vanilla names and
  race IDs into the map, confirming that extra ESP races do not populate it.
- `AVPairedCharacter::SetRace` at `0x144866FC0` stores the `UTESRace*` in the
  paired character and broadcasts its race-change delegate. This is a real
  Blueprint-callable native setter, but it does not add a key to Confirm's map.
- `GetIsRace` evaluator `0x1468C9D90` writes 1.0 only if the subject NPC's
  race pointer at `+0x1B0` equals the supplied race pointer. Its CommandInfo
  slot at `0x148FBBA70` contains the pointer to that evaluator. A Blueprint
  `SetRace` call does not make custom races satisfy Imperial's conditions.
- Sheogorath's player voice fix currently redirects the call at `0x14698CCCB`
  within `0x14698CB00`. That caller obtains the speaker's race through
  `0x1465B2620` before choosing a voice path. The reflected
  `AVPairedCharacter::SetVoiceType` at `0x144867340` writes a different byte at
  `+0xE99`; no evidence yet shows it changes this race-based voice lookup.

The Horns selection has a narrower native route. The shipping dispatcher
`UpdateCustomisationTarget` at `0x14493D590` treats toggle type 7 as a no-op,
but the reflected `UVRaceSexMenuViewModel::UpdateHair` at `0x14493DE20`
directly calls `UVPhenotypeCustomizationSession::SetHairPiece` at
`0x144811920`. It asks the selected `UVCharacterHairPieceBase` for its own
facial-hair type. Horn pieces are `UVCharacterHairPiece_Eyebrows` and report
`EVFacialHairType::Eyebrows` (3), which the setter writes to the eyebrows slot.
The row's option struct exposes `HairPiece` as a Blueprint-visible soft object
reference. A Blueprint-owned Horns click handler can resolve that selected
piece and call `UpdateHair(piece, index, refresh)`, while leaving the row's
toggle type 7 for the normal commit path. This is a source-backed candidate;
the widget graph, cook, and retail selection test remain to be done.

### Original hook trace and September 24 failure

The old runtime did not route every feature through Lua. The observed entry
points and effects are:

| Feature | Original entry point | Effect | Loader replacement |
| --- | --- | --- | --- |
| Horns selection | `OBRDremoraHorns/Scripts/main.lua` and the separate `OBRHornsForAll/Scripts/main.lua` register pre/post hooks on `/Script/Altar.VRaceSexMenuViewModel:UpdateCustomisationTarget` | Temporarily change `FLegacyRaceSexMenuToggleProperties.Type` from `EyebrowsStyle` (7) to `BeardStyle` (5) during the native call, then restore 7 before commit. The selected `UVCharacterHairPiece_Eyebrows` chooses the eyebrows slot itself. | The existing first-party DLL now applies this same native call boundary; its startup log confirms hook installation. The clicked Horns result, beard and hair preservation, and later Blueprint menu graph remain unverified. |
| Optional Horns positioning | The separate `OBRHornsForAll/Scripts/horn_offsets.lua` hooks `VPairedCharacter::{InitializeAppearanceFromForm,SetRace,SetSex}` and `VRaceSexMenuViewModel::UpdateCustomisationTarget`, polls every 250 ms, and binds Ctrl+Alt keys | Reapplies the selected horn mesh offset after appearance changes; this hook does not make a Horns click select a piece. | A loader actor must track the actual component and rebuild lifecycle if this separate feature is included. |
| Custom-race Confirm | `OBRPlayableRaces/src/dllmain.cpp::ExtendMap` on an engine-tick callback | Rebuild the executable's static `TMap<FString,int32>` from ten to fourteen race names using game-allocator memory. This is C++, not Lua or a Blueprint call. | The first-party DLL now rebuilds and verifies the map after `ExtendedRaces.esp` loads. A fresh game log confirms four names registered; completing Confirm as each new race is still unverified. |
| Imperial dialogue fallback | `OBRPlayableRaces/src/dllmain.cpp::GetIsRaceAlias`, installed into the `GetIsRace` `CommandInfo` eval slot | Calls the original evaluator first; for a failed Imperial test whose subject is the player, returns a match when the player's actual race is one of the four additions. It does not change the player's race. This is C++, not Lua. | The installed `ExtendedRaces.esp` adds four OR race tests to each of the ten identified player-targeted Imperial INFOs. Offline verified; tutorial and other dialogue still need retail proof. |
| Sheogorath player voice | `OBRPlayableRaces/src/dllmain.cpp::InstallPlayerVoiceRace` redirects one native voice-path call | Supplies the Imperial race only for the player at that call site; NPC Sheogorath voices stay intact. | No proven Blueprint/native replacement yet. `SetVoiceType` writes a different actor field. |
| Female Dremora voice | `OBRPlayableRaces/src/dllmain.cpp::ApplyVoiceFaction` on tick/load | Updates the player NPC's alternate-voice faction and a player actor flag that the normal NPC pairing routine does not derive for the player. | Reflected faction operations and flag behavior need a retail proof before this writer can be retired. |
| First-person skin | `OBRFirstPersonSkin/Scripts/main.lua` hooks `VPairedCharacter::{InitializeAppearanceFromForm,SetRace,SetSex}` and polls | Reapplies material and bounds after appearance rebuilds. | Loader actor can subscribe to appearance lifecycle and update owned components after the native rebuild. |
| Body compatibility | `OBRBodyGuard/Scripts/main.lua` uses a timed game-thread sweep | Repairs eligible body components. It does not register the Horns or dialogue hooks. | Loader actor needs equivalent target/ownership checks before touching player or inventory-doll components. |

The September 24 11:20 crash context reports an access violation reading
`0x10`. Its top shipping-game RVA is `0x04910040`, the instruction
`mov r15d, dword [rax+0x10]` in Confirm. On a failed race-name lookup the
preceding path leaves `rax=0`, so this instruction reads address `0x10`.
Merely making the selected name look like `Imperial` for this lookup would send
Imperial's ordinal race ID to `RaceSex_ClickDone_Handler`; it would not preserve
the custom race in the legacy character without another proven runtime step.
The same crash context's module list contains the installed `version.dll` but
no separately named UE4SS, UNBSE, or `OBRPlayableRaces` module. The installed
`ue4ss/UE4SS.log` was last modified September 12. Together with the inert
Horns row, this supports the old hook stack being inactive during the
September 24 run; a proxy may contain code internally, so module names alone
are not conclusive. The crash is in Confirm before the new dialogue INFOs run.
Do not attribute it to the ESP OR conditions.

The native call evidence is pinned to the installed retail EXE SHA-256
`B7BE7E6EBE9424F6FDF274F5E7A59372DE103E043AB5B66C60E021C0C89DF457`.
`UVRaceSexMenuViewModel::UpdateCustomisationTarget` (`0x14493D590`) treats type
7 as a no-op. `UpdateHair` (`0x14493DE20`) calls
`UVPhenotypeCustomizationSession::SetHairPiece` (`0x144811920`), which has an
eyebrows branch for facial-hair type 3. Both methods are reflected as
`BlueprintCallable` in the Altar mapping; this establishes a Horns graph route,
not proof that such a graph has been cooked or run. The GitHub loader `main`
still resolves to `ecb96cd86c824aaf4109a4af7678eb40509ef4cb` on this date.

### Player dialogue ESP candidate

The current `Oblivion.esm` and `AltarESPMain.esp` contain only ten distinct
Imperial `GetIsRace` INFO conditions whose CTDA flags run the test on the
dialogue target. `AltarESPMain.esp` supplies all ten current overrides: five
under HELLO, two under Attack, and three under CharGenTaunt2. The other
Imperial `GetIsRace` INFOs test the speaker; two QUST conditions also remain
speaker-side. `ExtendedRaces.esp` now overrides those ten INFOs and inserts
four target-preserving OR alternatives before each original Imperial test.
The build asserts each source INFO's parent DIAL and condition shape. The
verifier checks the original condition order and each added race/form/OR flag.
The ESP header author is `LorexValkin`, matching the loader metadata.
This replaces the old player-only evaluator alias for the identified dialogue
conditions at the data layer, subject to a retail tutorial/dialogue test.
It does not change the evaluator itself.

The current loader does not expose hooks for executable code or the legacy TES
condition evaluator. It cannot replace `OBRPlayableRaces` merely by discovering
a Blueprint actor. The following shipping-game behavior currently needs a native
bridge or a separately proven replacement route:

| Behavior | Current owner | Why an H11 actor alone is insufficient |
| --- | --- | --- |
| Resolve 14 distinct race names at character creation Confirm | First-party `version.dll` candidate | Confirm dereferences a miss in a ten-entry `TMap` compiled into the executable. Startup registration is field-confirmed; the menu action remains unverified. |
| Make player-targeted Imperial dialogue accept the four races | `OBRPlayableRaces` | Ten INFO overrides now carry native `GetIsRace` OR alternatives in the ESP; retail dialogue proof is pending. |
| Preserve Sheogorath NPC voice while routing the player's combat voice | `OBRPlayableRaces` | The player-only voice-path call is native. A shared ESP voice override also changes NPCs. |
| Apply the Horns row while keeping beard and hair | First-party `version.dll` candidate | The native hook installs; selection and the surrounding cosmetic state need an in-game check. |

`OBRPlayableRaces` also maintains the female Dremora alternate-voice faction and
flag. Those actor-state writes may be movable to Blueprint if the exact reflected
faction and flag API works in the shipping game. Retire that native responsibility
only after an independent in-game check.

## Blueprint candidates

`OBRFirstPersonSkin` and `OBRBodyGuard` act on Unreal actors, skeletal mesh
components and materials. They are candidates for a feature actor using the
reflected `VPairedCharacter` race/sex properties, `IsPlayerCharacter`,
`OnAppearanceRefreshedEnd`, `GetBodyMesh`, component material functions and
world lifecycle. Preserve the existing player-versus-inventory-doll distinction,
component ownership checks, material baselines, bounded mesh restore policy and
EndPlay cleanup. Exact graph compilation and retail behavior must be verified
before removing either Lua mod.

The `PDA_ModInfo.ModInfo` metadata is **ModName: Extended Races** and
**ModAuthor: LorexValkin**. An optional logic actor can call the current hub's
`RegisterMod` and `ReportModStatus` through its Owner. It should report only an
observed state. A listed mod means the metadata asset was discovered; it does not
prove the native race patch, ESP load order, Horns selection or rendered body.
Avoid reporting `Ready` until those dependencies can be checked. The new struct
also has `UIandSettingsModInfo` for a Settings and quick access widget; those
fields should stay empty until real widgets exist.

## Build and acceptance path

1. Author `/Game/Mods/ExtendedRaces/DA_ExtendedRacesModInfo` as a data asset
   instance of the rewrite's `PDA_ModInfo` in an isolated UE 5.3.2 workspace.
   Set the identity fields above and only the capability fields actually used.
   Package only Extended Races assets and a cooked discovery header; keep the
   loader's own structs, Blueprints and GameMode out of the mod archive.
2. Verify the cooked data asset is discovered and the mod appears in the Mods menu.
   Check Back/reopen and normal title controls.
3. Move one cosmetic behavior at a time. For each, compare the emitted graph,
   installed file hashes and observed game behavior before disabling its Lua
   implementation. Never run both writers on the same component in a release.
4. Test all four races through character creation Confirm, save/reload, dialogue,
   combat voice and travel. Exercise Horns with hair, beard and moustache, plus
   first-person arms near walls and third-party body rebuilds. Verify player and
   inventory doll separately.

Current local evidence: `python tools/verify_esp.py mod/esp/ExtendedRaces.esp`
passes the 22,309-byte ESP structure, intended-record diff, and ten player
dialogue OR-chain checks. Its SHA-256 is
`B29C66D9EC1BA6093967D01AA5A97A84BAAD1F9C518B618AFB8411F25FC62BD5`.
This is a data
check, not Blueprint compilation, manager-menu proof or a retail game test.
The ESP alone was installed while the game was closed; its post-copy hash
matches that source hash. The previous installed ESP is backed up at
`.work/extended-races-installed-esp-before-dialogue-20260924.esp` (SHA-256
`11FB0FDF964DBDB17629DC3E6F42C898E78EF67D238664BE5D491100835360CA`).
The installed game still uses the old UE4SS runtime components; this ESP update
does not constitute the Blueprint conversion or prove the new dialogue in game.

The metadata `.uasset` was resaved with the installed UE 5.3.2 editor after the
source-built editor's copy triggered an empty-engine-version warning. The
resaved source asset is `blueprint/assets/DA_ExtendedRacesModInfo.uasset`
(SHA-256 `F7D0EF1F57259A871FFD776FCC666901BBA31EE4F1CEF4CB78AF3D89ECAEFFCB`).
The installed editor reloaded it, confirmed `Extended Races` / `LorexValkin`,
and cooked its one requested package with zero errors and zero warnings.
The isolated metadata-only container and discovery pak at
`.work/extended-races-modinfo-candidate-ue532-20260924` passed the author
kit's offline `verify_mod.py` checks (4/4 packaged files match SHA-256).
That candidate intentionally has no ESP, logic actor, or settings widget and
has **not** been installed or observed in the retail Mods menu.

The existing first-party `version.dll` source now has a separate
`feat/extended-races-native-20260924` candidate at `c77f1ac` in the
`OBR Mod Limit Fix` worktree under `.work/limit-fix-extended-races-native`.
The game install contains candidate SHA-256
`2A94253DF7CB1F57394C31FEDE8938030F1BCC383C5EDF295F070A8542932FE8`;
the previous DLL and unchanged developer INI are backed up under
`.work/extended-races-native-install-20260924`, with `receipt.json`. The
September 24 fresh run (PID 30408) confirms the Horns hook installed and four
race names registered after the ESP loaded. Confirm, clicked Horns, voice,
cosmetics, and the loader menu still need their own retail proof.

The September 19 author kit's `Package-Discovery.ps1` mishandles a single
header because its filtered `$candidates` value becomes a string and
`$candidates[0]` becomes `C`. An isolated copy in `.work/extended-races-cooker`
wraps that filter in an array. The kit also leaves its discovery build record
inside `~mods`; this candidate moved that record under `_build` and
regenerated the manifest so only four actual pak/container files ship.

Sources: `docs/findings/2026-08-27-race-unlocking-engine-defects.md`,
`mod/ue4ss/OBRPlayableRaces/SOURCE.md`, the current cosmetic scripts,
GitHub `LorexValkin/UnblivionModLoader` at the pinned main commit above
(`unreal-project-dropins/modloader-rewrite/README.md`,
`runtime/modloader-rewrite/README.md`), and the locally staged 2026-09-19
mod-author kit for cooking/discovery mechanics. The kit predates the nested
sub-struct schema and cannot be copied unchanged into this build.
