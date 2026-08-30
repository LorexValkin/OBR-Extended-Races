# OBRPlayableRaces (Extended Races) — where this binary comes from

`dlls/main.dll` is a build artifact, vendored here so a release can be cut from
this repository alone. It is not written here.

`src\dllmain.cpp`, `src\CMakeLists.txt` and the UNBSE SDK headers under
`src\unbse-sdk\` are the source. The headers are the five files every UNBSE
package ships under `ue4ss\Mods\UNBSE\sdk\`, copied byte-for-byte from
`UNBSE-0.11.0-rc.1.zip`; their SHA-256s are the ones UNBSE's own
`foundation-manifest.json` pins.

## The foundation

A C++ UE4SS mod only loads on the UE4SS build it was compiled against. This
one is compiled against **UNBSE's pinned UE4SS**: RE-UE4SS `main` at commit
`68dd45cb5630bd7745310c6630b19c14345176a2` (4 July 2026, reports itself as
`UE4SS - v3.0.1 Beta #0 - Git SHA #68dd45cb`) plus UNBSE's `unbse-ue4ss-v1`
patchset — the source tree UNBSE keeps at `out/ue4ss-source` and the runtime
UNBSE 0.11.0-rc.1 ships as `ue4ss\UE4SS.dll`
(16,519,168 B, SHA-256 `041975EEEEEC83CB0270558122BA89586C4BDE5D0DB9C5728B16406B4FAAD7B3`).

On any other UE4SS build — the Nexus "UE4SS for OblivionRemastered" package
(`v3.0.1-394-g437a8ff`, April 2025) or the GitHub `v3.0.1` release — UE4SS
refuses the DLL with `Failed to load dll ... error code: 0x7f`: seven of its
24 `UE4SS.dll` imports (`RegisterEngineTickPreCallback` with `FCallbackOptions`,
`RegisterLoadMapPostCallback`, `on_cpp_mods_loaded`, the four-argument Lua
state overloads) do not exist there. That is why Extended Races requires UNBSE
rather than "any UE4SS": UNBSE is where the right runtime comes from.

## Rebuild and re-vendor

The mod is built through the `UNBSE_MOD_SOURCE_DIR` hook UNBSE's patch 0001
adds to the pinned UE4SS `CMakeLists.txt`, exactly as UNBSE builds its own
mod, but into a build tree of its own (`.work\ue4ss-build-playableraces`, not
tracked) so UNBSE's hash-verified build is never reconfigured. From a Visual
Studio 2022 x64 environment with CMake and cargo on the path:

    $src   = 'C:\Users\User\Desktop\Projects\UNBSE\out\ue4ss-source'
    $build = '.work\ue4ss-build-playableraces'
    cmake -S $src -B $build -G 'Visual Studio 17 2022' -A x64 `
          "-DUNBSE_MOD_SOURCE_DIR=$PWD/mod/ue4ss/OBRPlayableRaces/src"
    cmake --build $build --config Game__Shipping__Win64 --target OBRPlayableRaces --parallel 8
    Copy-Item $build\Game__Shipping__Win64\bin\main.dll mod\ue4ss\OBRPlayableRaces\dlls\main.dll

The first build compiles the whole pinned UE4SS tree (about ten minutes); later
ones recompile only `dllmain.cpp`. The pinned tree's two `Cargo.lock` files must
be left as the pins say — cargo 1.95 prunes them during a build — so restore
their bytes afterwards, as UNBSE's `Build-UNBSEUE4SSMod.ps1` does.

`tools/Build-Release.ps1` refuses to package this vendored copy if that build
tree holds a `main.dll` with a different hash, so a stale binary cannot ship
silently.

## Why it is a separate mod from UNBSE

UNBSE's manifest declares `writes: false` across all of its capabilities. This
mod performs writes into the running executable — rebuilding the race-id `TMap`,
swapping the `GetIsRace` eval pointer, setting the player's faction rank and
alt-voice flag — so it cannot live inside UNBSE without breaking that
guarantee. It ships beside UNBSE as an add-on instead, and says so: its
descriptor declares `runtime-read | runtime-write`.

## Why it is mandatory, not optional

Without it, confirming a character of any of the four added races crashes on the
way into the world: the Confirm handler looks the race name up in a ten-entry
table compiled into the executable and dereferences the miss. See
`docs/findings/2026-08-27-race-unlocking-engine-defects.md`.

## What it does (0.5.0)

1. Rebuilds the race-name map (the crash fix).
2. Aliases the four races' `GetIsRace` conditions to Imperial, player only.
3. Keeps a female Dremora player in `AltVoiceFaction`, and out of it
   otherwise, and keeps her alt-voice flag set, so the engine files her combat
   lines under the AltVoice folder they were recorded in. Checked from the
   engine tick every half second, and at once after a map load and the first
   time a race condition is evaluated for the player (which follows Confirm).
   See `docs/findings/2026-08-28-combat-vocal-trigger-chain.md`.

   (0.4.0 tried to hook `VPairedCharacter:SetRace` / `SetSex` /
   `InitializeAppearanceFromForm` through UE4SS's UFunction hooks and check
   only then plus once a minute. Those functions are called natively, never
   through `ProcessEvent`, so the hooks never fired and the voice state lagged
   Confirm by up to a minute. The poll replaces them.)

As a UNBSE add-on it:

- registers as `obr.playable-races` with the UNBSE host from
  `on_cpp_mods_loaded` (C++ mods start in directory order, so this mod starts
  before UNBSE does), declaring `runtime-read | runtime-write` and requiring the
  runtime-info and relocation capabilities;
- verifies the executable's identity through `UNBSERuntimeInfoV1` — PE link
  timestamp `0xF19077A4` and `SizeOfImage` `0x09E1E000`, the values UNBSE pins
  for `OblivionRemastered-Win64-Shipping.exe` 1.512.105 — and **does nothing
  at all on any other build**. Without UNBSE the same two fields are read from
  the PE header;
- resolves every RVA it touches through `UNBSERelocationV1`, which checks
  owner, image bounds and arithmetic; without UNBSE the same bounds check runs
  locally;
- listens to `unbse.core` lifecycle messages and stops writing on
  `runtime-stopping`;
- publishes a read-only status line to UE4SS Lua:
  `UNBSE.Invoke("obr_playable_races", "status")` returns one JSON line with the
  state of the identity check, the map rebuild, the alias, the alt-voice flag
  and the faction membership;
- retires its owner handle in its destructor.

Every structure is still verified before it is written, as before; the UNBSE
checks are in addition to that, not instead of it.
