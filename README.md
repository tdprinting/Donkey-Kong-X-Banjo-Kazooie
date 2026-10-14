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
with the bridge wired in. Nothing new is visible in-game yet. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains why the
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
| First Banjo asset visible in DK Isles | Next |
| Android app | Planned |

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

## Checking your ROMs

```bash
./build/jiggy-inspect info banjo.z64 dk64.z64
```

`supported: yes` means the dump matches the one both recomps target:

| Game | SHA-1 of the US 1.0 ROM |
| --- | --- |
| Banjo-Kazooie | `1fe1632098865f639e22c11b9a81ee8f29c75d7a` |
| Donkey Kong 64 | `cf806ff2603640a748fca5026ded28802f1f4a50` |

`.z64`, `.v64` and `.n64` files all work. The tool converts them to the console's byte order in memory.

To browse what the bridge can load:

```bash
./build/jiggy-inspect list banjo.z64        # every Banjo-Kazooie asset, with decomp names
./build/jiggy-inspect list dk64.z64         # DK64's 32 pointer tables
./build/jiggy-inspect list dk64.z64 5       # files in one table (5 = actor models)
./build/jiggy-inspect extract banjo.z64 0x34e banjo-model.bin   # MODEL_BANJOKAZOOIE_HIGH_POLY
./build/jiggy-inspect extract dk64.z64 5:0 model.bin
```

Extracted files are for your own local research and must stay out of this repository.

## Credits

The ROM layouts come from the CC0-licensed
[Banjo-Kazooie](https://gitlab.com/banjo.decomp/banjo-kazooie) and
[Donkey Kong 64](https://gitlab.com/dk64_decomp/dk64) decompilation projects. All
credit for the recompilations goes to their authors. This is an unofficial fan
project with no affiliation with Nintendo, Rare or Microsoft.
