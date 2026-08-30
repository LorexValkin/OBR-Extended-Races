# The UE4SS foundation for Extended Races — what was added, what was changed, and where it stands

Written 2026-08-28 after the first install on a second machine. Everything marked
MEASURED was read off a file, a log, an export table, or git; the rest is labelled.

## 1. The short version

- Extended Races ships one C++ UE4SS mod (`OBRPlayableRaces`, the crash fix) and two
  Lua mods (`OBRDremoraHorns`, `OBRFirstPersonSkin`). All three were developed and
  verified on **one specific UE4SS build**: RE-UE4SS `main` at commit
  `68dd45cb5630bd7745310c6630b19c14345176a2` (4 July 2026), which reports itself as
  `UE4SS - v3.0.1 Beta #0 - Git SHA #68dd45cb`. That build is the same one UNBSE is
  pinned to (`tools/unbse/ue4ss/foundation-manifest.json`, `foundationId`
  `ue4ss-3.0.1-beta0-68dd45cb-unbse-patchset-v1-mod-0.9.18`).
- The second machine had the UE4SS every Oblivion Remastered player has — the Nexus
  package "UE4SS for OblivionRemastered", `v3.0.1-394-g437a8ff` (RE-UE4SS `main` at
  `437a8ff`, 9 April 2025). On it: the Lua mods start, `OBRPlayableRaces` refuses to
  load (`error code: 0x7f`, procedure not found), and the Horns mod crashes UE4SS's
  own Lua layer on first use (MEASURED, both from that machine's `UE4SS.log` and a
  crash trace).
- A C++ UE4SS mod is bound to the UE4SS build it was compiled against, and the Lua
  API is not stable across fourteen months of `main` either. So the release has to
  carry its UE4SS. UNBSE already needs the same build, so the two projects ship the
  same foundation. This document records exactly what that foundation is.

## 2. What the foundation is (MEASURED)

| item | value |
| --- | --- |
| upstream | https://github.com/UE4SS-RE/RE-UE4SS.git, commit `68dd45cb…`, 2026-07-04 |
| Unreal submodule | `b2e876da82b17254c04304746341c8fde0ddb37c` |
| PatternSleuth submodule | `da8bfe4c5a464be0ef225c2c9a6ccaa2d9284018` |
| source distribution | upstream's own CI artifact `UE4SS_v3.0.1-1008-g68dd45cb-experimental.zip` (7,181,559 B, SHA-256 `EF0B1AC2…`), kept at `UE5 Oblivion/tools/UE4SS/` |
| proxy | `dwmapi.dll`, 71,680 B, SHA-256 `02822565CF0E4CC607BADB6F17F3F6C4D37A4B6ED05849D98CD18C6C685183B5` |
| runtime, upstream artifact | `ue4ss/UE4SS.dll`, 16,519,168 B, SHA-256 `041975EEEEEC83CB0270558122BA89586C4BDE5D0DB9C5728B16406B4FAAD7B3` |
| runtime, UNBSE rebuild | `ue4ss/UE4SS.dll`, 16,459,264 B, SHA-256 `21A7B6522F3C63F754A9005899A8B32D0058F299664B367298BA75CB6221E5A3` — built from the same commit plus the patchset below, in `out/ue4ss-build`, promoted by `Sync-UNBSEUE4SSPatchedRuntime.ps1` |
| licence | MIT; the upstream `LICENSE` (1,085 B, SHA-256 `99E1D8F8…`) is a required runtime artifact and ships beside the DLL |
| layout | `Binaries/Win64/dwmapi.dll` + `Binaries/Win64/ue4ss/{UE4SS.dll, UE4SS-settings.ini, LICENSE, Mods/}` — the `ue4ss/` subfolder layout, the same one the Nexus package and Vortex's Oblivion Remastered extension use |

Two runtimes exist because UNBSE rebuilds `UE4SS.dll` from the patched source so its
own mod and the runtime come off one compiler configuration. **Every Extended Races
test run to date used the upstream artifact `041975EE…`** — that is what the game on
the development machine had installed (MEASURED: the backup taken today is
16,519,168 B). The rebuild `21A7B652…` has not yet run under these three mods.
Both export the same symbol set (same source), so the C++ mod is expected to load on
either; "expected" is not "tested".

## 3. What we changed in UE4SS: patchset `unbse-ue4ss-v1`

Three patches, all in `tools/unbse/ue4ss/patches/`, pinned by SHA-256 in the
foundation manifest. None of them touches hooking, the Lua layer, or any runtime
behaviour the three mods depend on; two are header-only.

| patch | what | why |
| --- | --- | --- |
| `0001-add-unbse-external-mod.patch` | +11 lines in the root `CMakeLists.txt`: a `UNBSE_MOD_SOURCE_DIR` cache path that `add_subdirectory()`s an external mod after UE4SS's own projects | build a C++ mod *inside* the pinned UE4SS CMake graph, so compiler flags and CRT match the runtime exactly. `OBRPlayableRaces` is built through this same hook, into its own tree (`out/ue4ss-build-playableraces`), so UNBSE's hash-verified build is never reconfigured |
| `0002-fix-msvc-special-invalid-ptr-constexpr.patch` | `LuaMadeSimple/LuaObject.hpp`: `static constexpr auto special_invalid_ptr()` becomes `static auto` | MSVC 14.44 rejects the `reinterpret_cast` in a constexpr function; later upstream has the same change. Compile fix only |
| `0003-add-fscript-map-num-unchecked.patch` | +28 lines across `Map.hpp`, `Set.hpp`, `SparseArray.hpp` in the Unreal submodule: `NumUnchecked()`, `GetMaxIndexUnchecked()`, `GetMaxCapacityUnchecked()` forwarding to the existing assertion-free sparse-storage counts | UNBSE's bounded read-only map probes reject malformed map storage before any indexed access instead of tripping an assert. Additive; nothing existing changes |

What we did **not** change: the hook set, `CppUserModBase`, the Lua property
pushers. The behaviours Extended Races relies on — `RegisterEngineTickPreCallback`
with `FCallbackOptions`, `on_ui_init` / `on_cpp_mods_loaded`, and the Lua layer's
ability to read and write an enum field of a struct parameter (`properties.Type`
in the Horns mod) — are upstream `68dd45cb` behaviour.

## 4. What Extended Races adds on top

### `OBRPlayableRaces` 0.3.0 (C++)

Source: `mod/ue4ss/OBRPlayableRaces/src/dllmain.cpp` here; the identical file sits
at `UE5 Oblivion/tools/unbse/ue4ss/mod/OBRPlayableRaces/src/` and is what the build
compiles. Vendored binary `dlls/main.dll`, 47,616 B. It is its own mod, not part of
UNBSE, because UNBSE's manifest declares `writes: false` and this mod performs two
writes into the running executable (the race-name `TMap` rebuild and the
`GetIsRace` eval-pointer swap).

It imports 21 symbols from `UE4SS.dll` (MEASURED, pefile). The ones that pin it to
`68dd45cb` and do not exist in older builds:
`RegisterEngineTickPreCallback(std::function<…TCallbackIterationData…>, FCallbackOptions)`,
`FCallbackOptions` ctor/dtor, `CppUserModBase::on_ui_init`, `on_cpp_mods_loaded`,
and the four-argument `on_lua_start` / `on_lua_stop` overloads. Seven of the 21 are
absent from the Nexus `437a8ff` build; that is the `0x7f`.

Required `UE4SS-settings.ini` keys (the manifest's `requiredSettings`; the bundled
INI carries them):

```ini
[General]  EnableHotReloadSystem = 0   UseCache = 1   InvalidateCacheIfDLLDiffers = 1
           bUseUObjectArrayCache = false   DefaultExecuteInGameThreadMethod = EngineTick
[Hooks]    HookEngineTick = 1   EngineTickResolveMethod = Scan   HookGameViewportClientTick = 1
           HookUObjectProcessEvent = 1   HookProcessInternal = 1
           HookProcessLocalScriptFunction = 1   HookLoadMap = 1
[ObjectDumper]        LoadAllAssetsBeforeDumpingObjects = 0
[CXXHeaderGenerator]  LoadAllAssetsBeforeGeneratingCXXHeaders = 0
```

`HookEngineTick = 1` is the one this mod cannot live without: its whole poll runs
off the engine-tick hook.

### `OBRDremoraHorns`, `OBRFirstPersonSkin` (Lua)

Unchanged by the foundation work. Relevant only in that both call into UE4SS Lua
APIs whose behaviour differs between builds — see section 5.

## 5. What was tested where (MEASURED)

| UE4SS build | `OBRPlayableRaces` | Horns | First-person skin | verdict |
| --- | --- | --- | --- | --- |
| `68dd45cb`, upstream artifact `041975EE…` (dev machine) | loads; race table rebuilt; alias; AltVoice faction — all confirmed in game | works | works, all four races, both sexes | **the release target** |
| `68dd45cb`, UNBSE rebuild `21A7B652…` | not run | not run | not run | expected identical; needs one pass |
| Nexus `437a8ff` (second machine) | `Failed to load dll … error code: 0x7f` | mod starts; **UE4SS crashes** in `LuaType::push_enumproperty` (`LuaUObject.cpp:1095`, null `enum_ptr`) the moment the row is read | starts (`[FPSkin] loaded`); behaviour unverified | not viable without rewriting the Lua around that build's bugs, and a separate DLL build |
| GitHub release `v3.0.1` (`d935b5b`, Feb 2024) | API too old (`on_ui_init`, the Lua-state overloads and the engine-tick hook do not exist); flat layout, never scans `ue4ss/Mods` | — | — | not viable |

Two rebuilds of `main.dll` were made today while this was being diagnosed — one
against `d935b5b`, one against `437a8ff` (both verified import-for-import against
their targets). They are dead ends now that the decision is to bundle the foundation,
and must not ship. See section 7.

## 6. Review of today's uncommitted UNBSE foundation changes

`UE5 Oblivion` working tree, four files under `tools/unbse/ue4ss/`, modified
2026-08-28 15:40–15:54 (MEASURED):

1. **`scripts/UNBSEUE4SSFoundation.psm1`, `Set-UNBSERequiredIni`** — rewritten as
   two passes: first overwrite every *existing* required key by the parser's
   original line index, then insert the still-missing keys by re-finding each
   `[section]` header in the current line list. The old single pass inserted
   missing keys while later sections' stored indexes were still in use, so an
   insertion shifted them and the next section rewrote the wrong line — which
   duplicated `ObjectDumper` / `CXXHeaderGenerator` keys and dropped `[Hooks]`. The
   fix is correct: nothing is inserted before the index-based writes, and the
   insert pass never reuses a stored index. It throws if a required section
   vanishes mid-update, which is the right failure. Recommend committing.
2. **`scripts/Update-UNBSEUE4SSFoundation.ps1`** — the "fatal" filter over the
   audit's errors was `-match 'process|Game executable|…'`; the bare word
   `process` also matched the *setting name* `HookUObjectProcessEvent` inside a
   settings error, so a missing hook key was mis-classed as "game is running" and
   the update refused. Now anchored to the exact message prefixes. Correct.
3. **`tests/Test-UNBSEUE4SSFoundation.Tests.ps1`** — adds `HookUObjectProcessEvent`
   to the required set and a three-section fixture INI so both bugs above are
   pinned by a test. Run today on this machine (Pester 3.4): fixture tests **PASS**
   (the reparse-point fixture is skipped without symlink privilege). The
   "real installed game" audit reports `ue4ss/UE4SS.dll has an unexpected length`
   — that is not a code failure; it is because the development machine's game
   currently has the GitHub `v3.0.1` DLL installed for today's experiment
   (section 7).
4. **`foundation-manifest.json`** — the `ue4ss/UE4SS.dll` runtime pin moves from the
   upstream artifact (`041975EE…`, 16,519,168 B) to the UNBSE rebuild
   (`21A7B652…`, 16,459,264 B), matching `Sync-UNBSEUE4SSPatchedRuntime.ps1`'s
   pin. The same diff also refreshes four source hashes: the script-proof
   `main.lua`, the supervised proof-profiles config, and two Pale Pass files
   (`Test-OBRPalePassShippingClosureContract.ps1`,
   `OBRPalePassShippingStageCommandlet.cpp`). The last two have nothing to do
   with the foundation; they are in the diff only because the manifest hashes the
   whole source aggregate. Recommend committing the pin bump with a message that
   names the runtime swap, so the history says when the shipped `UE4SS.dll`
   changed identity; the hash refreshes can ride along but should be named.

Nothing in these four changes alters what the runtime does. They are updater and
test fixes plus a pin.

## 7. Where things stand right now, and what has to happen before a release

State of the development machine and this repository at the time of writing
(MEASURED; nothing below is committed or pushed):

- **Game install**: `Binaries/Win64/dwmapi.dll` and `ue4ss/UE4SS.dll` are the GitHub
  `v3.0.1` (`d935b5b`) files, put there for today's experiment;
  `ue4ss/Mods/OBRPlayableRaces/dlls/main.dll` is the `d935b5b`-targeted rebuild.
  The pre-experiment files (`041975EE…` runtime, its proxy and INI) are backed up
  in `.work/ue4ss-backup-unbse/`. UNBSE's own sync script will refuse the current
  DLL as an unrecognised base, so this must be restored before any UNBSE update.
- **Repository working tree**: `mod/ue4ss/OBRPlayableRaces/dlls/main.dll` and
  `src/dllmain.cpp` are the `437a8ff`-targeted rebuild (43,520 B, engine-tick poll,
  version string 0.3.1); `SOURCE.md`, `README.md` and
  `docs/release/nexus-description.bbcode` currently tell users to install the Nexus
  `437a8ff` package. All of that is the wrong story now and must go back to the
  committed 0.3.0 binary and source (`git checkout HEAD -- mod/ue4ss/OBRPlayableRaces`)
  with the text rewritten around the bundled foundation.
- The text changes from earlier today that stay: Sheogorath borrows Imperial's
  combat voice (committed, `b5b8202`), and the stagger-grunt limitation is gone
  (`8f63ee8`).

To release:

1. Restore the game to the `68dd45cb` foundation and decide which runtime ships:
   the tested upstream artifact `041975EE…`, or UNBSE's rebuild `21A7B652…` so that
   both projects place a byte-identical `UE4SS.dll`. The rebuild is the better
   long-term answer (one file, one hash, no overwrite fights between the two
   installs) and needs exactly one test pass of the three mods on it.
2. Add the foundation to the archive: `OblivionRemastered/Binaries/Win64/dwmapi.dll`,
   `…/ue4ss/UE4SS.dll`, `…/ue4ss/UE4SS-settings.ini` (with the required keys),
   `…/ue4ss/LICENSE`. Vortex treats the whole thing as one root mod; a user who
   already has the Nexus UE4SS installed gets a file conflict and must let this
   one win — say so on the page.
3. Say plainly on the Nexus page what bundling means: this replaces the player's
   UE4SS with a July 2026 `main` build; Lua mods written for the Nexus build
   generally keep working, but any *C++* UE4SS mod compiled for `437a8ff` will
   fail to load under it with the same `0x7f` — the mirror image of today's bug.
4. Keep the source of truth honest: `SOURCE.md` should name the foundation commit,
   the patchset, and the fact that a `main.dll` is only valid for that runtime;
   `Build-Release.ps1` should refuse to package if the bundled `UE4SS.dll` hash is
   not one of the two listed above.
