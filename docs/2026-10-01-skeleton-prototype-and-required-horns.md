# Skeleton prototype and required Dremora horns

Status at 2026-10-01 22:49 UTC: combined test candidate installed and hash-verified. Retail animation and horn acceptance remain pending. This is a UML Blueprint follower preview, not a selectable Skeleton race.

## Required Dremora horns

The corrected core restores the Dremora customization table and four gender-gated horn sets. Its cooked character-creation toggle Blueprint calls native `UpdateHair` for the eyebrow-style horn option. The option stays type 7 during confirmation; no UE4SS selection hook is used. Other widget functions are preserved. The Argonian race-grid row is byte-identical to retail and its customization assets are not overridden. Optional Horns for Everyone is excluded.

Required-horns archive SHA256: `f39ff7ba91c980870a36e587329baa3a8cfd2814b469ec1b49af4eb73af805b0`. Source: `blueprint/native_authoring/retail_overlay/build_required_horns.py` and `HornOverlayWriter/`. Original installation receipt: `.work/extended-races-bp-authoring/install-backup-20261001T215459Z/receipt.json`.

## Skeleton preview

The creature mesh was rebound through 90 explicit anatomical mappings onto the retained 379-bone Imperial male reference rig. Weighted bind-pose transfer converts its T-pose to the humanoid A-pose. The full body preserves 40,409 vertices and 60,598 triangles. The open-shoulder arms subset has 8,640 vertices and 13,412 triangles.

Bare, Blades Helmet, Dark Brotherhood Boots/Cuirass and Full Armor are real equipment variants found in the retained assets. Guardian, Hero and Champion names do not establish distinct bone-shape presets. Jaw and toe merges and joint warping are authored approximations requiring rendered review.

Nine meshes imported, rebuilt and reopened in an isolated Unreal project. The independent comparison passed geometry, oriented triangle multiplicities, UVs, named skin weights and local reference poses. Exact retail material references were assigned to owned mesh slots. The imported skeletons remain owned; the preview uses leader-pose bone-name mapping rather than replacing the player's skeleton or animation class.

`SkeletonPreview.inl` authors an editable Blueprint with four collision-free third-person followers, spaced 140 cm apart along the player mesh's local X axis, plus a no-shadow owner-visible arms follower offset 8 cm along local Y. The actor obtains the exact native main and first-person components, sets its owner to the player and refreshes attachment every 1.5 seconds. It does not replace meshes, hide the native head, modify equipment or change race records.

The actor compiled with zero Blueprint errors/warnings. The cook exited 0 and packed exactly 20 owned packages: nine meshes, their nine imported skeletons, the preview actor and UML metadata. Retail material and loader declarations are dependencies, not shipped overrides. A separate empty `ExtendedRacesSkeletonPrototype.esp` marker enables UML discovery; it has no gameplay records.

## Installed candidate and checks

Archive: `.work/extended-races-bp-authoring/extended-races-skeleton-test-20261001-v1/ExtendedRaces-Skeleton-Prototype-With-Dremora-Fix.zip`.

SHA256: `a9268378e5b770727d13d2733d8f9a1ce22eb53c3e4868a9539e46a90c039c44`.

The combined candidate contains all 353 required-horns payload files and five additional prototype files. Container membership, retoc verification, ZIP CRCs and payload hashes passed. Installer verified all 358 installed files with the game closed; no duplicate containers were removed. Receipt: `.work/extended-races-bp-authoring/install-backup-20261001T224949Z/receipt.json`. The original 14-race native confirmation capability is still an existing external requirement.

Isolated Unreal authoring/evidence: `D:/Unblivion Editor/Manifests/ExtendedRacesSkeleton20261001V5`. Mesh reopen proof: `.work/extended-races-bp-authoring/skeleton-mesh-prototype/unreal-reopen-v1/comparison.json`. Retained scripts: `blueprint/native_authoring/skeleton_prototype/`; output paths are local to this workstation and intentionally refuse overwriting retained runs. Start with a fresh project/output identity for another build. The first four import attempts were historical failures; V5 corrected the `_Source.glb` filename and is the successful import/build lineage.

## Retail acceptance

Restart the game and load a test character. In third person, inspect all four Skeleton followers while walking, sprinting, jumping, attacking, blocking and casting. In first person inspect the additional offset skeletal arms and finger motion; ordinary player arms remain visible for comparison. Test gloves, weapons, camera clipping, travel and reload. This establishes animation alignment before integrating a Skeleton selection into the race menu.

Separately test male/female Dremora horn options, hair/beard changes, confirmation, reload and travel. Confirm Argonian horn options still work. Offline preservation checks do not establish runtime appearance success.

To remove only the Skeleton preview, close the game, disable `ExtendedRacesSkeletonPrototype.esp` in Plugins.txt and remove `ExtendedRacesSkeletonPrototype_P.pak`, `.ucas`, `.utoc` and `ExtendedRacesSkeletonPrototypeDiscovery_P.pak` from `Content/Paks/~mods`. Retain the `zz_ExtendedRaces` containers for the Dremora correction. Do not remove the original Extended Races plugin.

The next full-race work is a real Skeleton race record, native confirmation-map registration, character-creation cosmetic controls, reversible exact-component replacement, native appearance-refresh handling and equipment/head visibility integration. None is claimed by this preview.
