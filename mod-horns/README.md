# Horns for All

Companion mod to Extended Races. Every playable race gets the **Horns** row in
character creation — the four Dremora horn sets and the four true-horn Argonian
styles — including the Extended Races additions (Dark Seducer, Golden Saint,
Dremora, Sheogorath).

Built against **Oblivion Remastered 1.512.105**.

## What you get

A **Horns** row above Hair Style for every race, with a "None" and:

| set | source | sexes |
| --- | --- | --- |
| Horns | Dremora A | male |
| Horns Curved | Dremora B | male |
| Horns of the Kyn | Dremora Lord | male |
| Horns Swept | Dremora female | female |
| Straight Horns | Argonian | male + female |
| Curved Horns | Argonian | male + female |
| Forehead Spikes | Argonian | male + female |
| Forehead Spikes Jeweled | Argonian | male + female |

Horns sit in a head slot of their own (the unused Eyebrows slot), so they cost
you nothing — hairstyle, beard and moustache all stay. The Dremora sets are
gender-locked because each mesh is skinned to the body it was authored for;
the Argonian styles ship separate male and female meshes, so both get them.

The Dremora horn meshes are this mod's own clones with the fused hairstyle
blanked (see Extended Races); the Argonian horn meshes are referenced straight
from the shipped game — they carry no fused hair and use the shared head rig.

## How the races are covered

- The ten vanilla customisation tables are overridden with the row spliced in;
  existing rows keep their exact bytes.
- Dark Seducer, Golden Saint and Sheogorath use the Imperial table, so the
  vanilla overrides cover them for free.
- Dremora has its own table in Extended Races; this pak ships a combined copy
  (Extended Races' four sets in their original option order, Argonian styles
  appended) and is named `zzz_` so it mounts later and wins. Saves made with
  Extended Races' own Horns row keep their selection.

## Install

Works with or without Extended Races. Requires UE4SS for the small Lua mod
that makes the Horns row apply (the UNBSE bundle Extended Races uses provides
it; any UE4SS install with Lua mods works).

**Vortex:** install the archive as it is; it deploys as a root mod.
**Manual:** extract into the game's install folder — the one that contains
`OblivionRemastered\` and `Engine\` — so you end up with:

```
OblivionRemastered\
  Content\Paks\~mods\zzz_HornsForAll_P.pak   (+ .ucas, .utoc)
  Binaries\Win64\ue4ss\Mods\OBRHornsForAll\
```

No .esp and no plugin-list entry — this mod is a content pak plus a Lua hook.

If Extended Races is also installed, its OBRDremoraHorns Lua mod and this one
coexist: whichever hooks first does the work, the other stands down.

## Known limits

- The horn meshes bind to the shared humanoid head rig. On heads far from the
  ones they were authored for (Khajiit and Argonian especially, for the
  Dremora sets) seating can drift. The experimental offset controls below can
  adjust placement; they do not reshape or re-skin the meshes.
- Removing the mod mid-save leaves the eyebrows-slot index pointing at a row
  that no longer exists; the game falls back to no horns.

## Experimental horn positioning

This build adds adjustable player horn placement. It has passed Lua fixture
tests; visual fit in the shipping game is **not yet verified**. It does not
include guessed offsets or change which horn styles each sex can select.

Select a horn style and hold **Ctrl+Alt** with one of these keys:

| Key | Action |
| --- | --- |
| Up / Down | Increase / decrease Z (height on the usual upright attachment) |
| Right / Left | Increase / decrease X |
| Page Up / Page Down | Increase / decrease Y |
| Backspace | Reset the current race/sex/style to its original placement |
| S | Save all current corrections |

Each press moves by **0.25 Unreal units**, with each axis limited to +/-10.
X/Y/Z are relative to the horn component's parent; check their visible direction
on your character. Start with **Ctrl+Alt+Down** for floating horns. Corrections
are separate for each race, sex, and horn style and apply only to the player's
Eyebrows slot and its associated shadow proxy. Hair and beard slots are untouched.

Adjustments are previews until you press **Ctrl+Alt+S**. A reset must also be
saved if you want it to persist. Saved corrections live next to `main.lua` in
`Scripts/horn-offsets.ini`, outside the game save, and are loaded at startup.
The previous settings file is retained as `.bak` after a successful replacement.
Preserve this file when updating the mod. Malformed or unreadable settings disable
saving for that session and report the problem in `UE4SS.log`.

The system reapplies saved corrections after appearance changes, with a 250 ms
fallback check. It remembers the unadjusted component position to avoid adding
the offset repeatedly. A brief unadjusted frame during rebuilding is still
possible. This is a translation adjustment, not a geometry or skinning repair;
animation, extreme head shapes, and certain horn combinations may still need
fitted meshes. Profiles are shared by characters with the same race/sex/style.

Before treating a correction as verified, inspect it from the front and side
in the creator and gameplay, turn/look up and down, change hairstyle and horn
style, then save the correction and reload the game. Check that hair, beard,
NPCs, and the horn shadow remain correct. `[HornsForAll] offset preview` and
`offsets saved` in `UE4SS.log` identify the selected profile and saved values;
startup alone does not prove visible placement.
