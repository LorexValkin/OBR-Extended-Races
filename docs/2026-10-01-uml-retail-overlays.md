# Extended Races UML retail-overlay candidate, October 1

Historical build record. This candidate was subsequently installed, then
superseded by the required-Dremora-horns correction and Skeleton test preview.
See [the current installed status](2026-10-01-skeleton-prototype-and-required-horns.md).

The owner permits child assets and mod-pack overrides of retail behavior. The
Horns side mod is out of scope. The UML loader is unchanged. All builds and
packaging outputs below are isolated; nothing from this candidate was installed
in the game or saved into canonical editor Content.

## Current candidate

`.work/extended-races-bp-authoring/extended-races-uml-overlay-candidate-20261001-v2/ExtendedRaces-UML-Overlay-Candidate-20261001.zip`

SHA-256: `4c87152405bee751f7aa8cc1cc3ea5a4748eb515a61e86fd3930002f6cddab88`.
The archive contains 353 hash-verified payload files plus its manifest and README.
It contains no loader, UE4SS, UNBSE, proxy DLL or Horns side-mod assets.

- Race grid: fourteen rows with matching alphabetical legacy ordinals. The ten
  original row payloads are preserved except for their required RaceId splices.
  Each added race uses the retail Imperial customization table; no Horns table
  or horn-mesh package is included.
- First-person actor: the corrected material-copy graph and expanded rig bounds,
  bound to UML metadata. Identity is Extended Races / LorexValkin; the metadata
  declares ExtendedRaces.esp and its twelve actual masters.
- Voice overlay: 132 retail response packages gain audio and animation keys.
  Female Dremora LEGACY keys reuse her existing ALTVOICE recordings. Sheogorath
  keys reuse Imperial LEGACY generic combat recordings. Original response fields
  and every original audio/animation map entry pass independent preservation
  verification. The overlay changes no shared race VNAM or NPC plugin record.
- Voice files: 342 MP3/LIP files extracted from the two retail voice BSAs supply
  the corresponding missing loose legacy paths. These belong under
  Content/Dev/ObvData/Data/Sound/Voice, rather than solely inside Unreal IoStore:
  the classic audio loader checks the legacy path before Unreal posts Wwise audio.

The skin graph authored three functions and two events with zero Blueprint
errors and warnings. Its fresh isolated v3 cook exited 0, with zero errors and
thirteen reconstruction startup/configuration warnings. Both containers and
discovery pak passed offline verification. The new voice container contains
exactly its 132 expected response packages and passes retoc verification. The
combined ZIP passes CRC and readback hashes for all 353 payload files.

## Native boundary and remaining work

Character creation Confirm is executable code at `0x14490FFA0`, not a cooked
Blueprint graph. It dereferences a missing entry in the ten-race C++ map at
`0x149309240`. RegisterSendDoneButtonHandler at `0x144928AA0` is a final native
function; an ordinary child or asset replacement does not replace its body.
The retained installed UnblivionLimitFix.log reports four custom names
registered on September 24. The candidate therefore still requires that
existing first-party native fourteen-race registration. This is separate from
UML loading the mod, and it is not a claim that UML patches the executable.

The pack route replaces the candidate's voice-writer responsibilities, subject
to in-game proof. It does not establish retail load or audible behavior. Check
all four added races and the ten vanilla Confirm selections; both sexes;
hit/power attack vocals; original Sheogorath NPC dialogue; bare arms and gloves
near walls; appearance rebuild; inventory doll; save/reload and travel. Keep a
single skin writer active during rendering acceptance.

The separate Body Guard third-party body compatibility logic has not been
ported. This is an offline candidate, not a completed compatibility release.

## Source and evidence index

`blueprint/native_authoring/retail_overlay/` retains the exact Python authoring,
packaging and preservation tools plus OverlayWriter's C# source. Sources use
the existing local FullDump cache, original cooked corpus, UAssetAPI mapping,
mod package tools and retoc. They refuse to overwrite an existing run directory;
choose a new output tag before reproducing a run. The local workspace's
OverlayWriter DLL was compiled from the retained C# source with .NET 8.

The authoring workspace is `.work/extended-races-bp-authoring/`. Current receipts:

| Evidence | Path relative to workspace |
| --- | --- |
| Race-only container and 14-row readback | `extended-races-core-20261001-v3/` |
| Voice member lineage and 132 package hashes | `extended-races-voice-overlay-20261001-v3/voice-overlay-manifest.json` |
| Independent preservation of original responses | `extended-races-voice-overlay-20261001-v3/retail-preservation-verification.json` |
| Final container manifest and verification logs | `extended-races-uml-overlay-candidate-20261001-v2/` |
| Final per-file hashes and scope | `extended-races-uml-overlay-candidate-20261001-v2/candidate/candidate-manifest.json` |
| Previous source assets and promotion hashes | `source-assets-before-promotion-20261001/` |
| Authenticated retail menu/race projections | `retail-overlay-evidence/` |

Skin authoring/binding logs are in
`D:/Unblivion Editor/Manifests/ExtendedRaces20261001Mapped/`. The clean cook is
in `.work/extended-races-skin-candidate-20261001-v3/_build/`. Earlier failed
packaging and voice attempts remain at their original paths. The first voice
attempt incorrectly expected KeyType metadata on an unversioned map; inspection
of the authenticated response corrected that assumption. v2 verified audio
keys; v3 also includes the corresponding animation keys.

The dump MCP resolved retail objects and authenticated native bodies. Its
search index remains paused, so search misses were not treated as absence.
The source-bound cache reader supplied complete exact-record projections.
Rider applied the authoring script edits; UAssetAPI supplied typed write/readback;
retoc supplied actual container verification. No live Binary Ninja analysis was
needed because the required native bodies were already captured.

Next action: retail acceptance of this candidate with the existing native
Confirm registration, followed by the remaining Body Guard compatibility port.
