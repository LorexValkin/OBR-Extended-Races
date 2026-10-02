# Extended Races for Unblivion: the verified mod folder

`Extended Races/` is the mod as installed and verified in game on 2 October 2026, except for its loose voice
files (see the end of this page). Players copy the complete folder, from the release archive, into
`Unblivion\Mods\`.

- **Verified:** Unblivion placed every file and registered 15 races for character creation (run pid 39172).
  The owner confirmed the races in game.
- **Needs:** Unblivion (its `dsound.dll` and `Unblivion` folder) and the Unblivion Mod Loader paks. No UE4SS,
  no UNBSE.
- **`Data/UML/RaceRegistry.txt`:** only Unblivion builds before 2 October 2026 read it. From commit c086b3b
  on, Unblivion works the race list out from the plugins itself, numbered the way the game numbers them, so
  the file is ignored. It is kept here because it was part of the verified set.
- **Rebuilding:** the files come from the authoring pipelines in `.work/extended-races-bp-authoring/`
  (local, not tracked). Most are byte-identical to the output of `playable-skeleton-v1`; the table names the
  exceptions. SHA-256 is abbreviated to 16 hex digits.

| File | Bytes | SHA-256 | Built in |
| --- | ---: | --- | --- |
| `Data/UML/RaceRegistry.txt` | 333 | `DC8BB4D61400F708` | `playable-skeleton-v1` |
| `ExtendedRaces.esp` | 22,309 | `B29C66D9EC1BA609` | `playable-skeleton-v1` |
| `ExtendedRacesDiscovery_P.pak` | 1,602 | `8E1A5F5C4175FA5B` | `playable-startup-crash-v1` |
| `ExtendedRacesSkeleton.esp` | 10,640 | `4E548C21C0C23B65` | `playable-skeleton-v1` |
| `ExtendedRacesSkeletonDiscovery_P.pak` | 1,678 | `2EDC3DCD60E77A0B` | `playable-startup-crash-v1` |
| `ExtendedRacesSkeleton_P.pak` | 347 | `75E7144577253917` | the standard IoStore stub, the same in every build |
| `ExtendedRacesSkeleton_P.ucas` | 2,832,585 | `A786A4B4468FD2AC` | `skeleton-fit-v3` |
| `ExtendedRacesSkeleton_P.utoc` | 1,844 | `02FB63B1FC8667EC` | `skeleton-fit-v3` |
| `ExtendedRaces_P.pak` | 347 | `75E7144577253917` | the standard IoStore stub, the same in every build |
| `ExtendedRaces_P.ucas` | 12,764 | `D7938505E207E4F9` | `playable-startup-crash-v1` |
| `ExtendedRaces_P.utoc` | 615 | `0A63C53B2A9766DC` | `playable-startup-crash-v1` |
| `zz_ExtendedRacesVoices_P.pak` | 347 | `75E7144577253917` | the standard IoStore stub, the same in every build |
| `zz_ExtendedRacesVoices_P.ucas` | 1,041,723 | `8F67588B8BDAB2B2` | `playable-skeleton-v1` |
| `zz_ExtendedRacesVoices_P.utoc` | 14,603 | `B2FC463474FBEA77` | `playable-skeleton-v1` |
| `zz_ExtendedRaces_P.pak` | 347 | `75E7144577253917` | the standard IoStore stub, the same in every build |
| `zz_ExtendedRaces_P.ucas` | 22,088,087 | `E6811DC5231DA1B5` | `playable-skeleton-v1` |
| `zz_ExtendedRaces_P.utoc` | 6,378 | `F62950FF1303535A` | `playable-skeleton-v1` |

**The 342 loose voice files are not in this repository.** They live under
`Data/sound/voice/oblivion.esm/`: female Dremora, and male and female Sheogorath. They are copies of the game's
own recordings, so they ship only in the mod's release archive, never on public GitHub. They are
byte-identical to the output of the same build, and `.gitignore` keeps them out.
