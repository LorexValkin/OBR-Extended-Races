# What in Extended Races needs native code, and what doesn't

Written 2026-10-03, in answer to "couldn't this have been a Blueprint-only mod?". Every address below is in the
installed `OblivionRemastered-Win64-Shipping.exe` 1.512.105 (SHA-256 `b7be7e6e…df457`). The evidence is static:
it was read from the decompiled game and its reflection data (what Blueprints can call or override), and checked
independently of the 27 August investigation, which reached the same conclusion by measuring the live game
and its crash dumps.

## Short answer

Almost all of it can be done, and now is done, with content: the ESP, paks and Blueprints. **One step cannot:
confirming a character of a race the game didn't ship as playable.** The Confirm button looks the race up in a table
of ten race names that is compiled into the game's code. An added race isn't in it, and the game crashes. No pak,
ESP or Blueprint can change that table, so a genuinely added race needs a native hook.

The mod itself ships no DLL. The native part is the Unblivion loader's race module, which extends that table and
merges the race-menu rows. The earlier 1.0 edition on Nexus used a UE4SS C++ mod (`OBRPlayableRaces`) for the same
job.

## Capability by capability

| What | Content only? | Evidence |
|---|---|---|
| Make the races playable | **Yes** (ESP) | `ExtendedRaces.esp` sets the playable flag on 4 existing RACE records; `ExtendedRacesSkeleton.esp` adds a fifth. |
| Race tiles, portraits, text, numbers | **Yes** (pak) | They come from the `DT_RaceSex_AllRacesModificationProperties` data table (RaceName, RaceId, Table, Race fields); `GetRaceId` (0x14491e370) reads the row's RaceId. |
| Live preview of the picked race | **Yes** | `UpdateRaceSexArchetype` is Blueprint-callable (0x14493fe10) and passes the row's index to the game's race menu (0x14656fd50 → 0x14678af60). |
| **Confirm with an added race** | **No** | The Confirm handler (0x14490ffa0) is a plain C++ function. It is registered by name (`RaceSex_ClickDone_Handler`) through `RegisterSendDoneButtonHandler` (0x144928aa0), which nothing can override. It looks the race name up in a static C++ map (0x149309240) that a static initializer (0x140b6f790) fills with ten hard-coded names numbered 0–9. On a miss it reads address 0x10 and the game crashes. `Random` (0x1448faed0) has the same unchecked lookup. |
| Vanilla races keep the right number | **No** | The number is a position: the resolver (0x14677ff40) counts only playable races, then sets the player's race. Races that sort ahead of vanilla ones shift those positions, so with only content High Elf (3 in the table) would be confirmed as Dark Seducer. |
| Bodies, faces, phenotypes, Unreal race assets | **Yes** (pak + Blueprint loader) | All data-driven: the race's Unreal asset holds its bodies, phenotype presets and per-race tables. The Skeleton race ships `Race_Skeleton` and `DT_SkeletonSync`. |
| Horns row | **Yes** | The old hook on `UpdateCustomisationTarget` (0x14493d590) is replaced by a cooked Blueprint widget calling `UpdateHair` (Blueprint-callable). |
| Race-gated dialogue (tutorial) | **Yes** (ESP) | The old hook replaced the game's `GetIsRace` check (0x1468c9d90). The ESP now carries 25 INFO records with OR conditions. |
| Combat voices | **Yes** (content) | Previously patched in code; the mod now ships 342 loose voice files and a voice pak. |
| Several race mods sharing one menu | Not from Blueprint | `AddDataTableRow` is editor-only in UE 5.3, and the race menu's row array can't be written from Blueprint. One mod, or a compatibility patch, can ship the whole table instead. |
| Change a race any other way | **No** | The game has no script command that sets a race. `AVPairedCharacter::SetRace` (0x144866fc0) changes only the look. |

### Why a Blueprint can't reach the table

In the game's reflection data, `UVRaceSexMenuViewModel` has no `BlueprintNativeEvent` or `BlueprintImplementableEvent`,
so nothing about Confirm can be overridden from a Blueprint. `RegisterSendDoneButtonHandler` is reflected, but it
only registers the native handler by name. The handler itself isn't reflected, and the ten-race table is a global
variable, not a property of any class. The view model's only map property, `RaceRowDataMap`, holds the per-race
menu rows (a different thing) and is a plain `UPROPERTY()` with no Blueprint access. A Blueprint can call only the
functions the game exposes, and none of them commits a race.

## The no-hook alternative, and its limits

A content-only mod could keep **at most ten** playable races and swap rather than add: rename existing races
(`FullName` is Blueprint-writable) so the table's numbers still line up with the sorted list, or apply a cosmetic
"race" on top of a vanilla one. Neither has been tested, and neither gives a real eleventh race with its own stats
and identity.

## Caveats

- **Saves:** a tester observed that a race is restored by its position, so a save needs the same plugin set with or
  without native code. The save code itself was not verified.
- **Static evidence:** the in-game proof is the 2 October test of the Unblivion edition, in which the Skeleton race
  is still marked as a test.
