# Roadmap

The goal is the *Banjo & Kong: Jiggy Jungle* concept: four tag-team duos, eight fused
worlds and collectibles from both games side by side, running natively on PC and Android. The
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
- [x] Run on real US 1.0 dumps: all 5574 Banjo-Kazooie assets load and all 32 DK64 tables read,
      Banjo's portrait and models decode correctly. (US Rev 1 shares the bank's layout but
      not its contents, so it stays unsupported.)

## 2. DK64 Recompiled as the host

*Size: weeks.*

- [x] Bring DK64 Recompiled into `host/` (vendored at upstream 3b0c36d so it can be changed
      freely) and build it from this repo. The player still generates the recompiled C from
      their own ROM, as upstream requires.
- [x] Ask for the Banjo ROM in the launcher next to the DK64 one, and load it at startup.
- [x] Expose the bridge to patches and mods as recomp imports (`jiggy_bk_available`,
      `jiggy_bk_asset_size`, `jiggy_bk_asset_read`, `jiggy_bk_sprite_frame_count`,
      `jiggy_bk_sprite_frame_rgba16` at `0x8F0004xx` in `patches/syms.ld`).
- [x] Own app id (`JiggyJungle`), so saves and settings stay separate from DK64 Recompiled.
- [x] **First visible crossover:** Banjo's own dialogue portrait (`ASSET_7EF_SPRITE_BANJO`)
  is decoded from the player's ROM (`bridge/src/bk_sprite.cpp`, all eight BK sprite
  formats) and fades in, talking, at the bottom left of the HUD each time DK Isles loads
  (`host/patches/jiggy_cameo.c`). Needs a check on real ROMs: `jiggy-inspect sprite` dumps
  the same frames to PNG.

## Updates and releases (done)

- [x] Updater core (`updater/`): reads GitHub's latest-release JSON, compares versions,
      picks the download for the platform, and unpacks .zip/.tar.gz releases into the
      install folder with rollback on failure.
- [x] Launcher entry in the host: checks on launch, then installs in place on Windows and
      Linux, or opens the release page on Flatpak and macOS.
- [x] `release.yml`: builds Windows and Linux packages and publishes the release. It needs
      the `JIGGY_ROM_REPO` / `JIGGY_ROM_TOKEN` secrets, since the game code is generated
      from a decompressed ROM, as upstream does.
- [x] Android: the app downloads the release APK and hands it to the system installer,
      and `release.yml` adds a signed APK when the `JIGGY_ANDROID_*` key secrets are set.

## 3. Android

*Size: weeks to a few months, depending on device issues.*

- [x] Gradle project (`android/`, arm64-v8a, Android 9 / API 28+) with the bridge and updater built in
      through JNI. CI builds a debug APK on every push.
- [x] ROM selection with Android's file picker. Each ROM is checked by the same C++ as the
      desktop launcher (`jiggy/rom_import.hpp`) and stored big-endian in app storage.
- [x] In-app updates: download the release APK and install it with Android's package installer.
- [x] Cross-compile the engine for Android (`android/engine/build-engine.sh`): the runtime,
      RT64 on Vulkan through SDL's window, RecompFrontend and the game's own sources, with
      SDL2, FreeType and curl from vcpkg. Small patches to RT64, plume and zstd live in
      `android/engine/patches/`. CI builds all of it except the final link, which needs the
      code generated from a ROM.
- [x] The game as `libmain.so` on SDL's activity (`GameActivity`), with ROMs, saves, config
      and unpacked assets in app storage. `release.yml` builds it from the ROM and puts it in
      the APK; development APKs from CI have the launcher only.
- [ ] First run on a real phone. Nothing has been tested on a device yet, so expect fixes.
- [x] On-screen touch controls: an N64 layout drawn over the game that drives an SDL virtual
      controller, so the game's own input mapping handles it. Hidden while a physical
      controller is connected, which SDL supports directly. The desktop file dialog is a stub
      on Android, since the launcher picks ROMs.
- [x] Save data and config in app storage (`APP_FOLDER_PATH`).

## 4. Banjo content in the DK64 engine

*Size: months.*

- [x] Model reader (`bridge/src/bk_model.cpp`): header, vertex list, textures (CI4, CI8,
  RGBA16, RGBA32) and an F3DEX display list walk that produces triangles with UVs. On the
  real ROM it finds exactly the triangle count each model's header gives (Banjo high poly:
  4470), and `jiggy-inspect model` exports OBJ + PNG for checking.
- [x] Model converter (`bridge/src/bk_gfx.cpp`): re-encodes a model's F3DEX display lists as
  F3DEX2 for the DK64 engine, with its vertices, textures and render modes in one block.
  Banjo & Kazooie's model now stands beside the player in the Training Grounds and DK Isles
  (`host/patches/jiggy_banjo3d.c`), tested in game on real ROMs.
- [x] Bones: apply Banjo-Kazooie's skeleton (the geo list's bone and skinning commands) so
  models can pose and animate (bridge/src/bk_anim.cpp, bk_pose.cpp; jiggy_bk_model_pose).
- [x] Play Banjo-Kazooie's own skeletal animations on its models (no retargeting needed:
  the host evaluates BK's animation assets directly).
- [ ] Golden Bananas look like Jiggies: Banjo-Kazooie's Jiggy model is drawn in place of
  every Golden Banana, placed (props) or rewarded (actors), keeping its spin and size
  (`host/patches/jiggy_collectibles.c`). Tested in game; still drawn flat gold, without
  Banjo-Kazooie's shine.
- [ ] Animated textures (segments 11-15) for the models that use them. Banjo's doesn't: his
  eyes are geometry picked by selectors, so blinking already works.
- Texture and palette conversion where formats differ.
- [x] Banjo-Kazooie sounds: the host renders sound effects (sfx bank, VADPCM, with BK's
  chains, envelopes and pitch) and pickup jingles (compact MIDI on the music bank) from the
  player's ROM and mixes them into DK64's audio (`bridge/src/bk_sound.cpp`,
  `host/src/game/jiggy_audio.cpp`; imports `jiggy_bk_sfx_play`, `jiggy_bk_jingle_play`).
  Played for note and Jinjo pickups, eggs, the Feathery Flap and Talon Trot steps, with
  DK64's pickup sound as the fallback without a Banjo-Kazooie ROM. Still to hook up: the
  Jiggy jingle (DK64's own Golden Banana fanfare needs replacing at the same moment).
- Music: Banjo sequences and instruments into DK64's sound bank format, or a second
  sound bank loaded alongside DK64's. (The jingle renderer above plays whole sequences, but
  only pre-rendered, so it doesn't follow DK64's music system.)

## 5. Playable Banjo & Kazooie

*Size: many months.*

- [x] First step: D-pad left swaps the player to Banjo & Kazooie (their model and
  animations over DK's moves and physics) in the Training Grounds and DK Isles, and back.
- [x] Banjo's animations for DK's moves (Claw Swipe, Beak Barge, crouch, backflip) and
  Banjo-Kazooie's air moves: Feathery Flap, Rat-a-tat Rap and Beak Buster
  (`host/patches/jiggy_banjo3d.c`), tested in game.
- [x] Talon Trot (`jiggy_banjo_trot.c`) and Egg Firing (`jiggy_banjo_eggs.c`, eggs are DK's
  coconut shots drawn as Banjo-Kazooie's eggs), tested in game.
- Rebuild the rest of the moveset (Wonderwing, Shock Spring Jump, Flight, swimming) as DK64
  patches, using the BK decompilation as the behaviour reference.
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

- Collectibles from both games side by side. DK64's colored bananas stay, alongside
  Banjo-Kazooie's Music Notes, Jiggies (in place of Golden Bananas), Jinjos, Honeycomb
  pieces, Mumbo Tokens, eggs and feathers, each with its own counter and pickup.
- Fused worlds (for example, Fungi Forest with Click Clock Wood's seasons): each one blends
  both games' map elements into one world, fun to play and completable from start to finish,
  with its own cutscenes and NPCs. Built from the two games' own maps, models and characters,
  modified where needed (edits applied at runtime to what the player's ROMs supply), with
  wholly new assets only as a last resort.
- A hub rework, Mumbo and Funky's shops, and the tag-team final boss against Gruntilda and
  K. Rool.

## Ground rules

- Reuse the two games' own assets wherever possible: models, animations, textures,
  sounds, music and text come from the player's ROMs. Only create something new when no
  existing asset from either game can do the job, and note why when that happens.
- No ROMs, extracted assets or other copyrighted game data in this repository, ever.
- Format knowledge comes from the CC0 decompilations and is credited where it's used.
- Both recomps are GPL-3.0. Code taken from them keeps that license, so this project
  will be GPL-3.0 once milestone 2 brings their code in.
