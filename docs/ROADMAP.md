# Roadmap

The goal is the *Banjo & Kong: Jiggy Jungle* concept: four tag-team duos, eight fused
worlds and a shared Note and Jiggy economy, running natively on PC and Android. The
player supplies both ROMs.

Milestones 1 to 3 are well understood engineering. Milestone 4 onward is open-ended
reverse-engineering and game-design work on the scale of a long-running fan project.
The estimates below are rough sizes, not promises.

## 1. Jiggy Bridge (this PR)

The foundation every later step needs: reading both games' files from the player's ROMs.

- [x] ROM loading in any byte order, game detection, SHA-1 checks against the US 1.0 dumps
- [x] Banjo-Kazooie asset bank reader with rarezip decompression and decomp asset names
- [x] DK64 pointer table reader with gzip decompression and shared-file redirects
- [x] C API for the host and for Android
- [x] `jiggy-inspect` for checking ROMs and browsing assets locally
- [x] Tests on synthetic ROMs, CI on Linux and an Android arm64 cross-build
- [ ] Run `jiggy-inspect list` on real US 1.0 dumps and confirm every entry decodes.
      This needs a maintainer with both ROMs, since CI can't have them.

## 2. DK64 Recompiled as the host

*Size: weeks.*

- Bring DK64 Recompiled in as a submodule and build it from this repo (the player still
  generates the recompiled C from their own ROM, as upstream requires).
- Ask for the Banjo ROM in the launcher next to the DK64 one, and open the bridge at startup.
- Expose the bridge to patches as recomp imports (`jiggy_bk_asset_read` and similar, at
  free `0x8F0000xx` addresses in `patches/syms.ld`).
- **First visible crossover:** one Banjo-Kazooie asset loaded from the player's ROM
  and shown in DK Isles. A dialogue string or HUD sprite is the simplest place to start
  before tackling 3D models.

## 3. Android

*Size: weeks to a few months, depending on device issues.*

- Gradle project with an SDL2 activity hosting the DK64 host build (arm64-v8a, API 24+,
  Vulkan 1.2).
- ROM selection with Android's file picker, passed to `jiggy_bridge_open_memory`.
- On-screen touch controls and controller support; replace the desktop file dialog.
- Save data and config in app storage.

## 4. Banjo content in the DK64 engine

*Size: months.*

- Model converter: Banjo-Kazooie's model format and F3DEX display lists to DK64's actor
  model format and F3DEX2.
- Texture and palette conversion where formats differ.
- Animation retargeting from BK's bone animation format to DK64's.
- Music: Banjo sequences and instruments into DK64's sound bank format, or a second
  sound bank loaded alongside DK64's.

## 5. Playable Banjo & Kazooie

*Size: many months.*

- Rebuild the core moveset (Feathery Flap, Rat-a-tat Rap, Talon Trot, Beak Barge, Egg
  Firing) as DK64 patches, using the BK decompilation as the behaviour reference.
- Add the duo to DK64's character swap, then a quick-swap wheel in place of Tag Barrels.

## 6. Two-player split-screen co-op

*Size: months. Engine work that doesn't depend on milestones 4 and 5, so a prototype with
two Kongs can start right after milestone 2.*

Each duo becomes two players: player 1 controls one partner and player 2 the other, and
both play through every mission together.

- Starting point: DK64 already has 2 to 4 player split-screen, but only in its battle
  arenas (the `multiplayer` overlay in the decompilation). DK64 Recompiled ships it behind
  a "work in progress, non-functional" setting, so it needs fixing first.
- Bring split-screen cameras and viewports into story-mode maps, with a second player actor
  driven by controller 2.
- Rework what assumes one player: cutscenes, warps, loading zones, collectible pickups,
  Kong-only switches, and the HUD (one per half of the screen).
- Tie it into the duo system: both players share the duo's Notes and Jiggies, and duo-only
  Jiggies need both partners. On Android, player 2 uses a controller.
- Reuse DK64's existing split-screen HUD and camera code rather than writing new versions.

## 7. Jiggy Jungle proper

*Size: long-term.*

- Shared Notes and Jiggies replacing per-Kong colored bananas.
- Fused worlds (for example, Fungi Forest with Click Clock Wood's seasons), a hub
  rework, Mumbo and Funky's shops, and the tag-team final boss against Gruntilda and K. Rool.

## Ground rules

- Reuse the two games' own assets wherever possible: models, animations, textures,
  sounds, music and text come from the player's ROMs. Only create something new when no
  existing asset from either game can do the job, and note why when that happens.
- No ROMs, extracted assets or other copyrighted game data in this repository, ever.
- Format knowledge comes from the CC0 decompilations and is credited where it's used.
- Both recomps are GPL-3.0. Code taken from them keeps that license, so this project
  will be GPL-3.0 once milestone 2 brings their code in.
