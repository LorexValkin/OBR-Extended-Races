# OBRPlayableRaces (Extended Races) — where this binary comes from

`dlls/main.dll` is a build artifact, vendored here so a release can be cut from
this repository alone. It is not written here.

`src\dllmain.cpp` and `src\CMakeLists.txt` here are the source. The build tree
lives in the UNBSE project, because it shares UNBSE's pinned UE4SS build; the
same two files sit there and are what the build compiles:

    tools/unbse/ue4ss/mod/OBRPlayableRaces/src/dllmain.cpp
    tools/unbse/out/ue4ss-build-playableraces/

Rebuild and re-vendor:

    cmake --build tools/unbse/out/ue4ss-build-playableraces `
          --config Game__Shipping__Win64 --target OBRPlayableRaces
    # then copy Game__Shipping__Win64/bin/main.dll over dlls/main.dll

`tools/Build-Release.ps1` refuses to package this copy if the sibling build tree
is present and holds a newer DLL, so a stale vendored binary cannot ship
silently.

## Why it is a separate mod from UNBSE

UNBSE's `foundation-manifest.json` declares `writes: false` across all 20 of its
capabilities, and `tests/test_source_contract.py` asserts it. This mod performs
two writes into the running executable — rebuilding the race-id `TMap` and
swapping the `GetIsRace` eval pointer — so it cannot live inside UNBSE without
breaking that guarantee. It ships beside UNBSE instead.

## Why it is mandatory, not optional

Without it, confirming a character of any of the four added races crashes on the
way into the world: the Confirm handler looks the race name up in a ten-entry
table compiled into the executable and dereferences the miss. See
`docs/findings/2026-08-27-race-unlocking-engine-defects.md`.

## What it does (0.3.0)

1. Rebuilds the race-name map (the crash fix).
2. Aliases the four races' `GetIsRace` conditions to Imperial, player only.
3. Keeps a female Dremora player in `AltVoiceFaction`, and out of it
   otherwise, so the engine files her combat lines under the AltVoice folder
   they were recorded in. See
   `docs/findings/2026-08-28-combat-vocal-trigger-chain.md`.
