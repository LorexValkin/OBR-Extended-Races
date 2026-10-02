# Extended Races: UNBX registration and loader update

The Extended Races port was missing its native Confirm registration in the current
installation: `version.dll` is absent, while the installed UNBX runtime only
registered ObScript commands. The historical LimitFix success log cannot prove
the current runtime has that capability.

This candidate puts the retained generic race-map adapter into UNBX, behind the
existing pinned-image and competing-extender checks. It uses a mod-owned
`Content/Dev/ObvData/Data/UML/RaceRegistry.txt`. The per-file native loader runs
unchanged first; successful required-plugin callbacks admit map construction with
the game's allocator and string hash. The existing Confirm/name/sex/paired-engine
continuation is preserved. No Blueprint call is made on the legacy loading thread.

The launcher and runtime share manifest validation. The launcher rejects absent
or disabled required plugins. UNBX's receipt distinguishes an armed hook from
completed registration; `unbx-races-PID.log` under `%LOCALAPPDATA%/Unblivion/UNBX`
records registration or refusal. If injection arrives after the required plugin
has loaded, this adapter does not backfill it: an armed receipt alone is not success.
Unknown or missing race registration is not a safe fallback for custom Confirm.

The update also includes the retained September 24 UML rewrite from Mod Manager's
`public/runtime/modloader-rewrite`, replacing the different September 22 installed
containers. Its 11 packages are the owned loader assets plus the intentional
`BP_AltarGameMode` override. Its published status is work in progress; compatibility
of this combined candidate still needs a fresh in-game run.

## Checks

- 31 local Rust tests pass, including native trampoline execution with synthetic
  arguments, manifest bounds/collisions, a 15-name Skeleton fixture and missing or
  disabled plugin preflight. These are not game-runtime tests.
- Release launcher and runtime DLL build successfully. Two existing test-helper
  lifetime warnings remain in untouched upstream code.
- All five native signatures match once in shipping EXE SHA-256
  `b7be7e6ebe9424f6fdf274f5e7a59372de103e043ab5b66c60e021c0c89df457`.
- The installed winning playable RACE records match the fourteen menu ordinals.
  Legacy FULL values use `LOC_FN_*` keys; the first literal-name comparison failed
  and is retained. Explicit known-key mapping corrected the offline comparison.
  This is not native ordinal readback, localization qualification or saved-race proof.
- UML container verification, membership and six-file ZIP hash readback pass.

Evidence is retained in `.work/extended-races-bp-authoring/`:
`extended-races-unbx-v1/{tests.log,build.log,preflight.json}`, the source candidate
`unbx-races-candidate-v1/`, and `extended-races-loader-update-v1/`.
UNBX source baseline is `02cb8ab9ad2ce6d8319975eacc14028444b45fed`; promotion checks
each preimage hash and preserves unrelated work. No publication or Git push.

## Skeleton addition remains incomplete

`skeleton-race-record-v1/ExtendedRacesSkeleton.esp` contains a separately owned
record, local ID `0x800`, derived from Altar's Imperial record with all 69 untouched
donor subrecords verified. It is deliberately non-playable, staged only, and is
not in the install ZIP. The existing follower preview remains installed.

Required work: owned Unreal race/phenotype assets and UML sync mapping; reversible
player/head/arms and inventory-doll appearance handling; equipment/appearance
refresh; native selected-race readback; and rendered animation/save/reload proof.
The newly researched race and appearance functions identify continuation and
ownership constraints; addresses alone do not establish a usable adapter for
those remaining responsibilities. Body Guard compatibility remains unported.

Next acceptance: launch through `unbx-loader.exe`, verify the new per-process race
log says fourteen names registered, then test Dremora Confirm and horns, followed
by the other races, sex/name, voice, first-person arms, save/reload and travel.
Also inspect the four Skeleton followers and first-person arms while moving,
attacking, blocking and casting. Do not call the Skeleton race playable yet.

Tool observations: scoped source reads and retained native evidence located the
missing capability; Rust tests exercised the copied hook ABI; retoc verified
containers. No local semantic/symbol connector was available, so exact `rg`
searches and narrow source reads were used. System PATH lacked Python; the installed
Python 3.12 executable worked. Rider's unavailable Python project formatting was
not treated as Python validation. Augment was not used.
