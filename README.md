# Extended Races — Dark Seducer, Golden Saint, Dremora and Sheogorath

Four races the game already models but never lets you pick, added to character
creation. Dremora get a **Horns** row of their own.

Built against **Oblivion Remastered 1.512.105**.

---

## What you get

| race | sexes | face sliders |
| --- | --- | --- |
| Dark Seducer | male + female | yes |
| Golden Saint | male + female | yes |
| Dremora | male + female | male yes, female fixed face |
| Sheogorath | male | fixed face |

Dremora also get four horn sets in a **Horns** row above Hair Style, plus a
"None". Horns sit in a head slot of their own, so they cost you nothing — you
keep your hairstyle, beard and moustache.

**They fight with their own voices.** Dremora, Dark Seducer and Golden Saint
each have real, recorded hit and power-attack lines that vanilla never lets you
hear — the Isles races because theirs are locked behind a faction no player can
join, and female Dremora because hers are filed under a voice folder only
faction membership selects. This plugin points each race at its own recordings,
keeps the generic playable-race grunts (which have no audio for these races)
out of their selection pool, and the UE4SS mod keeps a female Dremora on her
alt-voice recordings. It does not put anybody on a borrowed voice: the audio is
keyed by race, so a race with nothing recorded (Sheogorath) stays silent rather
than sounding like someone else.

## Install

Drop the three folders onto `OblivionRemastered\`, keeping the structure:

```
OblivionRemastered\
  ObvData\Data\ExtendedRaces.esp
  Content\Paks\~mods\zz_ExtendedRaces_P.pak   (+ .ucas, .utoc)
  Binaries\Win64\ue4ss\Mods\OBRPlayableRaces\
  Binaries\Win64\ue4ss\Mods\OBRDremoraHorns\
  Binaries\Win64\ue4ss\Mods\OBRFirstPersonSkin\
```

Then add the plugin to your load order. In
`OblivionRemastered\Content\Dev\ObvData\Data\Plugins.txt`, put

```
ExtendedRaces.esp
```

**after `AltarESPMain.esp`.** That is not optional: `AltarESPMain.esp` overrides
14 of the 15 race records itself, so a plugin loading before it silently loses.

### Requirements

**UE4SS — required. Not optional, and not only for cosmetics.** The pak and
plugin make the four races *selectable*, but the shipping executable cannot
carry a character of any of them into the world: the character-creation Confirm
handler looks the race name up in a ten-entry table compiled into the binary and
dereferences the miss. Confirming a Dark Seducer, Golden Saint, Dremora or
Sheogorath **crashes the game** with an access violation reading `0x10`. No pak
or plugin can fix that; the table is in the executable.

`OBRPlayableRaces` fixes it at runtime, and also fixes a second problem the
first one hides — `RaceId` is an ordinal position in the alphabetical playable
list, so adding four races renumbers seven of the ten vanilla ones and applies
the wrong race's attributes, skills and body. It additionally makes the four
races satisfy the race-gated dialogue checks Imperial satisfies, without which
Valen Dreth never taunts you and the tutorial cannot be completed.

`OBRDremoraHorns` is the cosmetic one: without it the Horns row appears but
clicking it does nothing.

`OBRFirstPersonSkin` fixes first person. The added races' skin materials were
never set up for the game's first-person clipping fix, so near a wall your
gauntlets and sleeves would shift while the arm underneath did not — pieces
floating off the hand, the hand vanishing into the wall. The mod moves the
first-person arms onto the retail first-person skin material with the race's
own textures and tint copied across.

Get it from [RE-UE4SS releases](https://github.com/UE4SS-RE/RE-UE4SS/releases).
Tested against **v3.0.1**. Install it into `OblivionRemastered\Binaries\Win64\`
so you end up with:

```
Binaries\Win64\
  dwmapi.dll            <- the UE4SS proxy that loads it
  ue4ss\
    UE4SS.dll
    UE4SS-settings.ini
    Mods\OBRPlayableRaces\     <- required; without it the game crashes
    Mods\OBRDremoraHorns\      <- the Dremora Horns row
    Mods\OBRFirstPersonSkin\   <- first-person arms near walls
```

All three ship an `enabled.txt`, so UE4SS starts them automatically — you do
**not** need to edit `mods.txt`.

UE4SS must be able to hook Blueprint-called functions, which is its default. If
you have edited `UE4SS-settings.ini`, these must stay on under `[Hooks]`:

```ini
HookProcessInternal = 1
HookProcessLocalScriptFunction = 1
HookUObjectProcessEvent = 1
```

**Nothing else is needed.** No script extender and no OBSE. `OBRPlayableRaces`
is built alongside UNBSE and shares its build tree, but it does not need UNBSE
installed — it is its own UE4SS mod, kept separate precisely because UNBSE
guarantees it never writes to the running game and this one must. If you already
run UE4SS for other mods, these sit alongside them.

All three container files (`.pak`, `.ucas`, `.utoc`) must be present. A mod
missing any one of them will not mount, silently.

### Checking it worked

`Binaries\Win64\ue4ss\UE4SS.log` will contain:

```
[OBRPlayableRaces] rebuilt with 14 entries, hashSize=32; all re-verified
[OBRPlayableRaces] race conditions aliased to Imperial for the four added races (player only)
[DremoraHorns] loaded - Horns row enabled for Dremora
[FPSkin] loaded - poll on
```

and, the first time you click a horn set:

```
[DremoraHorns] Horns row live: toggle type 7 -> 5 (written, reads back 5)
```

`OBRPlayableRaces` verifies the engine structures it is about to touch and
refuses to write if anything fails to check out, so on a build it does not
recognise you get an error line and an unmodified game rather than a corrupted
one. If you see `model verified` but not `rebuilt`, that is what happened.

Once a character exists, it also logs its race once, and a female Dremora
her voice faction:

```
[OBRPlayableRaces] player race resolved: formType=9 formId=0x00038010 - aliased to Imperial
[OBRPlayableRaces] AltVoiceFaction found: formId 0x0B000802
[OBRPlayableRaces] female Dremora player: added to AltVoiceFaction
```

and, in first person on any of the four:

```
[FPSkin] Dremora female: first-person skin slot 0 -> instance of MIC_Imperial_Body_F
```

If the first `[OBRPlayableRaces]` line is missing entirely, UE4SS is not loading
the mod — do not start a character, it will crash. If `[DremoraHorns] loaded`
never appears when you click, that mod is loaded but the menu hook did not
attach.

## Uninstall

Delete the items above and remove the `Plugins.txt` line.

**Safe to add to an existing save. Not safe to remove from one, if your
character is one of the four races.** The plugin only overrides records that
already exist and creates no new forms, and the horn selection lives in save
fields the game already had, so *adding* it is safe, and so is removing it from
a character of a vanilla race.

But `RaceId` is an ordinal in the playable-race list, so a save made as one of
the four resolves correctly only while this mod is installed. Load such a
character without it and the game will resolve them to some other race
entirely. Change race before uninstalling, or keep a save from before.

## Known limitations

- **Dremora female has no face sliders.** `SK_Dremora_Head_f` ships without the
  face-morph curve set — 1 morph axis against the 113 on the male head — so the
  game itself cannot pose it. Her face is fixed. Nothing short of new head art
  changes that.
- **Sheogorath is male only and has a fixed face**, for the same reason.
- **Dremora female gets one horn set.** Only `SK_Dremora_HR_Female` is skinned
  to the female head; the other three sit wrong on it and are gated to male.
- **Nobody reacts to you as a Dremora.** The four races answer to Imperial's
  race-gated dialogue, because their own does not exist — 1,221 conditions in
  the game name Imperial and none names these four for the player. It is what
  makes the tutorial completable at all. Your own race's lines still work where
  any exist; Imperial's are added, not substituted.
- **Sheogorath has no combat voice.** Hit and power-attack grunts come from
  race-keyed dialogue records, and no recording of Sheogorath making those
  sounds exists anywhere in the game. He fights in silence. Dark Seducer,
  Golden Saint and Dremora use their own recordings.
- **Stagger grunts are silent for all four.** The Wwise bank declares the switch
  value for each of them and then leaves the branch empty, which is a decision
  in the shipped audio rather than a missing key. Breathing, fatigue and
  footsteps are unaffected and work normally.
- **Another mod that makes further races playable will break the numbering.**
  `RaceId` counts positions in the alphabetical playable list, so anything that
  inserts more races shifts everything after them. That shows up as the wrong
  race being applied, not as a crash.
- **Horn menu tiles borrow Argonian icons.** The shipped Dremora horn art has no
  menu portraits, and the tile view draws nothing for an option without one.

## Building from source

Everything in the release is generated; nothing is hand-edited binary. The
toolchain, none of which is redistributed here:

| tool | used for |
| --- | --- |
| [retoc](https://github.com/trumank/retoc) | IoStore ⇄ legacy package conversion, and packing the `_P` container |
| [UAssetGUI / UAssetAPI](https://github.com/atenfyr/UAssetGUI) | legacy package ⇄ JSON, at `VER_UE5_3` |
| `TES4R_1_2_Mappings.usmap` | build-matched mappings; unversioned properties cannot be decoded without it |
| Python 3.12 | the authoring scripts under `tools/` |

Then:

```
tools\Build-Pak.ps1              # authors every package and packs the container
python tools\build_esp.py        # the plugin
python tools\verify_esp.py mod\esp\ExtendedRaces.esp
tools\Build-Release.ps1 -Version 1.0.0
```

`verify_esp.py` is worth running every time. It walks the plugin's group
structure and fails unless the walk lands on the last byte exactly, then diffs
every record against the copy it was built from and reports anything that
changed which was not meant to - including a repeated subrecord being dropped,
which a plain type-level comparison misses.

`OBRPlayableRaces` is C++ and is not built by these scripts; its source and
build tree live in the UNBSE project. See
`mod\ue4ss\OBRPlayableRaces\SOURCE.md`. `Build-Release.ps1` refuses to package
the vendored DLL if that build tree is present and holds a newer one.

Paths to retoc, UAssetGUI, the usmap and Python are parameters on the scripts —
override them if yours differ from the defaults.

**The usmap must match the game build.** Reflected property layouts move between
patches, and a stale usmap decodes silently wrong rather than failing.

## Notes for other mod authors

The Horns row uses the **Eyebrows** head slot, which every head in the game
instantiates and no shipped content fills. The character-creation menu has no
handler for its `EyebrowsStyle` toggle type, so the row draws and dispatches but
nothing happens — that gap is what the UE4SS mod closes, in about ten lines. If
you want horns without the UE4SS dependency, point the row at `BeardStyle`
instead and Dremora trade beards for horns.

The four horn meshes are shipped as clones with their fused hairstyle blanked
(`MIC_Dremora_HairOff`, a hair material with `Opacity_Multiply` at 0). The
shipped meshes are untouched, so Dremora NPCs keep their hair exactly as it was.
