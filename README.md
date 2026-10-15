# Banjo & Kong: Jiggy Jungle

A fan project to blend **Banjo-Kazooie** and **Donkey Kong 64** into one game, built on
the two community recompilation projects:

- [Donkey Kong 64: Recompiled](https://github.com/Rainchus/Donkey-Kong-64-Recompiled)
- [BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp)

Both are native ports made with [N64Recomp](https://github.com/N64Recomp/N64Recomp),
running on [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) and
rendering with [RT64](https://github.com/rt64/rt64).

> **This repository contains no game data.** You need your own legally obtained
> US 1.0 ROMs of *both* games. They are read at runtime and never copied into the
> repository. Don't commit ROMs or anything extracted from them.

## Status

Milestone 1 of the [roadmap](docs/ROADMAP.md), the **Jiggy Bridge**, is done: it's the
library that lets the DK64 engine read Banjo-Kazooie's files from the player's ROM.
Milestone 2 is underway: DK64 Recompiled now lives in [`host/`](host) as the engine,
with the bridge wired in. The first crossover is in: Banjo's dialogue portrait, read from
the player's Banjo-Kazooie ROM, greets you when DK Isles loads (not yet tested on real
ROMs). [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains why the
project is shaped this way.

| Piece | State |
| --- | --- |
| ROM checks (byte order, game detection, SHA-1 against the US 1.0 dumps) | Done |
| Banjo-Kazooie asset bank reader (rarezip) | Done |
| DK64 pointer table reader (gzip, shared files) | Done |
| C API for the recompiled game and for Android | Done |
| `jiggy-inspect` command-line tool | Done |
| CI: Linux (GCC + sanitizers, Clang) and Android arm64 cross-build | Done |
| DK64 Recompiled host (`host/`) with the bridge linked in | Done |
| Launcher option to load the Banjo-Kazooie ROM | Done |
| Game-side imports (`jiggy_bk_asset_read` and friends) | Done |
| Auto-updater from GitHub Releases (Windows and Linux install in place) | Done |
| Release workflow publishing Windows and Linux builds | Done (needs ROM secrets, see below) |
| Banjo-Kazooie sprite decoder (all 8 texture formats) | Done |
| First Banjo asset visible in DK Isles (Banjo's portrait) | Done, needs a test on real ROMs |
| CI: game patches build | Done |
| Banjo & Kazooie's 3D model, converted from the player's ROM, beside DK with Banjo-Kazooie's idle, walk, run and jump animations | Done, tested in game |
| Play as Banjo & Kazooie (D-pad left, or J on keyboard): Claw Swipe, Beak Barge, crouch, backflip, Feathery Flap, Rat-a-tat Rap, Beak Buster, Talon Trot (crouch + C-left), Egg Firing (crouch + C-up/C-down), Wonderwing (crouch + C-right), swimming and Shock Spring Jump (A on the pad in the Training Grounds) | Done, tested in game |
| Golden Bananas drawn as Banjo-Kazooie Jiggies, with Banjo-Kazooie's Jiggy jingle on pickup | Done, tested in game (the jingle not yet heard on a real pickup) |
| Music Notes as their own collectible, with Banjo-Kazooie's note counter (in the Training Grounds and Jungle Japes), saved per save file | Done, tested in game |
| Jinjos (all five colors) as their own collectible, with Banjo-Kazooie's five-head counter (Training Grounds and Jungle Japes), saved per save file | Done, tested in game |
| All five Jinjos on a map give a Jiggy: Banjo-Kazooie's fifth-Jinjo fanfare, then a Golden Banana (drawn as a Jiggy) appears where the last Jinjo stood, counted and saved by DK64 (permanent flags 0x31E and 0x31F); given once per save file, and back on the next visit if left behind | Done, tested in game (Jungle Japes) |
| Empty Honeycomb pieces as their own collectible, with Banjo-Kazooie's six-piece ring counter, saved per save file; Honeycombs that give back a slice of health (Training Grounds for now) | Done, tested in game (pieces; health refill not yet) |
| Red and Gold Feathers as their own collectible, with Banjo-Kazooie's feather counters (Training Grounds and Jungle Japes), saved per save file; the Wonderwing spends Gold Feathers | Pickup and counters tested in game; Wonderwing cost needs a test |
| Mumbo Tokens as their own collectible, with Banjo-Kazooie's token counter and pickup sound (Training Grounds and Jungle Japes), saved per save file | Done, tested in game |
| Bottles in Jungle Japes: his molehill by the way in; he pops out with Banjo-Kazooie's animations and sounds when you come near, holds the player in a light cutscene and talks in a text box (his portrait, DK64's dialogue font, A to go on), then ducks back down; the first talk is saved, after it a one-line reminder | Done, tested in game |
| Grublins in Jungle Japes: three of Mumbo's Mountain's hooded enemies patrol the first area with Banjo-Kazooie's animations and sounds, cackle and chase when they see you, hurt and knock you back through DK64's own enemy hit, and fall to punches, rolls, aerial attacks and the Simian Slam (gone until the map is loaded again) | Done, tested in game (DK64 attacks; Rap and Beak Buster need a test) |
| Conga in Jungle Japes: Mumbo's Mountain's ape stands by the bamboo at the east edge of the first area with Banjo-Kazooie's animations and sounds, taunts you in a text box the first time you come near, beats his chest and throws oranges in an arc that hurt you through DK64's own enemy hit; three hits (punches, rolls, aerial attacks, the Simian Slam, or your shots and eggs) beat him, he says a line and a Golden Banana (drawn as a Jiggy) appears beside him; his defeat is saved, and the banana comes back until collected | Done, tested in game |
| Flight (A on the Flight pad in the Training Grounds): flaps on Red Feathers, stick to steer and pitch, Beak Bomb with B, Z to Beak Bust | Takeoff, flying and flaps tested in game; Beak Bomb and landing need a test |
| Banjo-Kazooie's own sounds from the player's ROM: note, Jinjo, Honeycomb and Jiggy jingles, egg, Feathery Flap, Talon Trot, Wonderwing and Shock Spring Jump sounds | Done, checked in the game's audio output |
| Android app: ROM picker and checks, in-app updates | Done, needs a test on a phone |
| Android: engine cross-compiles, game packaged as libmain.so on SDL's activity | Done, untested on a phone |
| Android: on-screen touch controls, controller support | Done, untested on a phone |

## Building

You need CMake 3.20+, Ninja and a C++20 compiler.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For Android (arm64, API 24+):

```bash
cmake -S . -B build-android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
cmake --build build-android
```

## Building the game

The game itself is built from [`host/`](host), a copy of DK64 Recompiled with the Jiggy
changes on top. Like upstream, it needs your own **decompressed DK64 ROM** at build time
to generate the game code, so it can only be built on your machine:

```bash
git clone --recurse-submodules https://github.com/tdprinting/Donkey-Kong-X-Banjo-Kazooie
cd Donkey-Kong-X-Banjo-Kazooie/host
```

Then follow [host/BUILDING.md](host/BUILDING.md) from step 2 (dependencies, decompressing
the ROM, running N64Recomp, then CMake). The executable is still called `DK64Recompiled`.

When the launcher opens, pick **Load DK64 ROM** and **Load Banjo-Kazooie ROM**. Both are
stored in the app's own folder (`JiggyJungle`), separate from DK64 Recompiled's saves.

### Windows test builds

For quick tests on a Windows PC, `tools/windows/build-windows.sh` cross-compiles the game
from Linux with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) and writes
`build-windows/JiggyJungle-windows.zip`. Generate the game code in `host/` first, as above.
Android stays the target, and releases still come from `release.yml`.

## Updates

On launch the game checks this repository's
[latest release](https://github.com/tdprinting/Donkey-Kong-X-Banjo-Kazooie/releases/latest).
When a newer version exists, the launcher shows **Install Update x.y.z**. On Windows and
Linux that downloads the build for your platform, installs it next to the current one
and asks you to restart. If anything fails, the old version is left as it was. Flatpak
and macOS builds open the release page instead, and the Android app downloads the APK
and hands it to Android's installer, which asks you to confirm.

### Publishing a release

[`release.yml`](.github/workflows/release.yml) builds Windows and Linux packages and
publishes them as a GitHub Release that the updater picks up:

1. Put your **decompressed** DK64 ROM (`donkeykong64.decompressed.us.z64`) in a separate
   **private** repository. Never add it to this one.
2. In this repository's settings, add the secrets `JIGGY_ROM_REPO` (that repo's
   `owner/name`) and `JIGGY_ROM_TOKEN` (a fine-grained token that can read it).
3. For the Android APK, make a signing key once and keep it safe: every release must be
   signed with the same key, or phones refuse the update. For example
   `keytool -genkeypair -keystore jiggy.keystore -alias jiggy -keyalg RSA -validity 10000`.
   Add the secrets `JIGGY_ANDROID_KEYSTORE` (the file, base64-encoded:
   `base64 -w0 jiggy.keystore`), `JIGGY_ANDROID_KEYSTORE_PASSWORD`,
   `JIGGY_ANDROID_KEY_ALIAS` and `JIGGY_ANDROID_KEY_PASSWORD`. Without them, releases
   skip the APK.
4. This repository is private (Jiggy Jungle is for personal use), so the updater needs a
   token to see its releases. Make a fine-grained token limited to this repository with
   read-only **Contents** access and add it as `JIGGY_UPDATE_TOKEN`. It's built into the
   release builds, so keep it read-only and limited to this one repository.
5. Bump `JIGGY_VERSION` in `host/include/jiggy_version.h`, then tag and push:
   `git tag v0.1.0 && git push origin v0.1.0`.

This is the same setup upstream DK64 Recompiled uses for its own releases. The packages
contain the compiled game but no ROM data, and players still load their own ROMs.

## Android

[`android/`](android) is the Android app (Android 9 or newer, arm64). The launcher picks
your two ROMs with Android's file picker (each is checked and stored in the app's own
storage) and installs updates from GitHub Releases. Start Game runs the game: DK64
Recompiled with the Jiggy Bridge, built as `libmain.so` and run on SDL's activity.

The game code is generated from a DK64 ROM, so only builds made with one contain the game.
Release APKs do (`release.yml`). The debug APK every CI run uploads
(**JiggyJungle-debug-apk** on the run's page) has the launcher only.

To build the full app yourself, with the Android SDK and NDK installed and the game code
generated in `host/` as for the desktop build:

```bash
android/engine/build-engine.sh DK64Recompiled   # libmain.so, into android/app/engine-libs
cd android && ./gradlew assembleDebug
```

## Checking your ROMs

```bash
./build/jiggy-inspect info banjo.z64 dk64.z64
```

`supported: yes` means the dump matches the one both recomps target:

| Game | SHA-1 of the US 1.0 ROM |
| --- | --- |
| Banjo-Kazooie | `1fe1632098865f639e22c11b9a81ee8f29c75d7a` |
| Donkey Kong 64 | `cf806ff2603640a748fca5026ded28802f1f4a50` |

Banjo-Kazooie US Rev 1 (`ded6ee16…`) is not supported: its asset bank has the same layout
but many assets moved or changed (asset IDs from about 0x539 up differ from 1.0's), so the
names and IDs Jiggy Jungle uses would point at the wrong files.

`.z64`, `.v64` and `.n64` files all work. The tool converts them to the console's byte order in memory.

To browse what the bridge can load:

```bash
./build/jiggy-inspect list banjo.z64        # every Banjo-Kazooie asset, with decomp names
./build/jiggy-inspect list dk64.z64         # DK64's 32 pointer tables
./build/jiggy-inspect list dk64.z64 5       # files in one table (5 = actor models)
./build/jiggy-inspect extract banjo.z64 0x34e banjo-model.bin   # MODEL_BANJOKAZOOIE_HIGH_POLY
./build/jiggy-inspect extract dk64.z64 5:0 model.bin
./build/jiggy-inspect sprite banjo.z64 0x7ef 0 banjo.png    # Banjo's dialogue portrait, frame 0
./build/jiggy-inspect model banjo.z64 0x34e banjo-model/   # Banjo & Kazooie (high poly) as OBJ + PNG textures
./build/jiggy-inspect sounds banjo.z64        # the sound effect bank (add "music" for the music bank)
./build/jiggy-inspect sfx banjo.z64 0x4e flap.wav 1.24 20000   # SFX_4E (Feathery Flap) at BK's pitch and volume
./build/jiggy-inspect jingle banjo.z64 9 note.wav 16000        # COMUSIC_9_NOTE_COLLECTED
```

Extracted files are for your own local research and must stay out of this repository.

## Credits

The ROM layouts come from the CC0-licensed
[Banjo-Kazooie](https://gitlab.com/banjo.decomp/banjo-kazooie) and
[Donkey Kong 64](https://gitlab.com/dk64_decomp/dk64) decompilation projects. All
credit for the recompilations goes to their authors. This is an unofficial fan
project with no affiliation with Nintendo, Rare or Microsoft.
