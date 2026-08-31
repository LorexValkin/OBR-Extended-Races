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
  Dremora sets) seating can drift; nothing here re-fits geometry per race.
- Removing the mod mid-save leaves the eyebrows-slot index pointing at a row
  that no longer exists; the game falls back to no horns.
