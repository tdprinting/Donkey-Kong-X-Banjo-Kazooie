# Architecture

This page records what we learned from reading both recomp codebases and why Jiggy
Jungle is built the way it is. Paths refer to the upstream repositories:
[DK64 Recompiled](https://github.com/Rainchus/Donkey-Kong-64-Recompiled) (`dk64/`),
[BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp) (`banjo/`), and the CC0
decompilations they build on.

## How each recomp works

The two projects come from the same template and are laid out almost identically:

- **Build time.** The player's ROM is decompressed (`src/game/rom_decompression.cpp`)
  and [N64Recomp](https://github.com/N64Recomp/N64Recomp) turns the game's MIPS code into
  C (`us.toml` / `banjo.us.rev0.toml`), with `RSPRecomp` doing the same for the audio
  microcode. The generated C is compiled into the executable. This is why neither project
  can be built without a ROM, and why CI here builds only ROM-independent code.
- **Patches.** `patches/*.c` is C compiled for MIPS, then recompiled like game code. It
  replaces game functions and calls native code through absolute symbols in
  `patches/syms.ld` (e.g. `recomp_get_bgm_volume = 0x8F000030`).
- **Runtime.** [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime)
  provides libultra (threads, the scheduler, audio, controllers, saves) and the mod
  loader. [RT64](https://github.com/rt64/rt64) renders the display lists.
  [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) draws the launcher and
  menus.
- **Game registration.** `src/main/main.cpp` registers a `recomp::GameEntry` with the
  ROM's hash, its save type and the recompiled entrypoint. At runtime the game reads data
  from the player's original ROM through emulated PI DMA.
- **Mods.** Both ship the N64Recomp mod loader (`.nrm` files). Native helpers are exposed
  to mods with `REGISTER_FUNC(...)` in `src/game/recomp_api.cpp`.

| | Banjo-Kazooie | Donkey Kong 64 |
| --- | --- | --- |
| ROM (US 1.0) SHA-1 | `1fe1632098865f639e22c11b9a81ee8f29c75d7a` | `cf806ff2603640a748fca5026ded28802f1f4a50` |
| ROM size | 16 MiB | 32 MiB |
| RAM | 4 MiB | 8 MiB (Expansion Pak) |
| Graphics microcode | F3DEX | F3DEX2 |
| Asset storage | One asset bank, rarezip (DEFLATE) | 32 pointer tables, gzip |
| Runtime fork pinned | upstream N64ModernRuntime | `Killklli/N64ModernRuntime` (patch-1) |

## Why one engine, not two

The first idea, running both recompiled games inside one app, doesn't hold up:

1. **They want the same memory.** Both games are linked at the same N64 addresses
   (`0x80000000` onward) and assume they own all of RDRAM.
2. **The runtime is a singleton.** N64ModernRuntime holds one set of emulated threads,
   one scheduler, one VI, one audio interface and one section table
   (`librecomp/src/overlays.cpp`). It was built to run one game per process.
3. **The generated code collides.** Both recompilations emit functions named after their
   addresses (`func_80xxxxxx`), `recomp_entrypoint` and other globals, so they can't be
   linked into one binary without reworking the generator.

Even if those three were solved, the result would be two games taking turns, not one
blended game.

So Jiggy Jungle uses **DK64 Recompiled as the engine** and treats Banjo-Kazooie as a
**content source**:

- The concept is set on DK Isles with K. Rool, so DK64's hub, its Kong-swapping system
  and its 8 MiB memory budget are the natural base.
- Banjo-Kazooie's models, animations, sprites, text and music are read from the player's
  own Banjo ROM at runtime through the **Jiggy Bridge**, then converted to the formats
  the DK64 engine expects.
- Banjo & Kazooie's moves are rebuilt as DK64 patches, using the BK decompilation as the
  reference for how they behave.

## The Jiggy Bridge

`bridge/` is a small C++20 library with no dependencies beyond miniz, the DEFLATE
library N64ModernRuntime already bundles.

```
player's ROMs ──► Rom (byte order fixed, SHA-1 checked)
                   ├─► BkAssetTable       Banjo asset bank at 0x5E90, rarezip
                   └─► Dk64PointerTables  DK64 pointer tables at 0x101C50, gzip
                         │
                         ▼
                   bridge.h (C API) ──► host: recomp imports for DK64 patches
                                   └──► Android: ROM bytes from the system file picker
```

The layouts come from the decompilations:

- **Banjo-Kazooie**: `include/assets.h` and `src/core2/code_B3A80.c`
  (`assetCache_init`, `assetcache_get`); the bank's ROM offset is in
  `decompressed.us.v10.yaml`. Rarezip framing is in `src/core1/code_660.c`. Asset names
  come from `enum asset_e` in `include/enums.h`, extracted by
  `tools/codegen/gen_bk_asset_names.py`.
- **Donkey Kong 64**: `src/global_asm/code_6AF80.c` (`func_global_asm_8066ADA0`,
  `8066B5F4`, `8066B4D4`, `getPointerTableFile`) and `romAssetBin` in
  `symbol_addrs.us.txt`. Table names come from `enum pointertable_e`.

The bridge refuses any dump other than the two US 1.0 ROMs, because those are the only
images the offsets are known to be right for.

## Android

Neither recomp ships an Android build today, but the pieces are close:

- RT64 already sets `ANDROID_PLATFORM android-24` / `ANDROID_ABI arm64-v8a` and skips
  SDL linking on Android. Its Vulkan backend is the renderer phones would use.
- N64ModernRuntime's `renderer_context.hpp` already has an `ANativeWindow` path.
- DK64 Recompiled already has Linux ARM64 CI runners, so the generated code builds for
  ARM64. The AVX requirement only applies to x86 builds.

What's missing is the app around it: an SDL2 Android activity, Vulkan 1.2 device checks,
touch controls, and ROM selection through Android's file picker, since apps can't read
arbitrary paths. That's why the bridge has `jiggy_bridge_open_memory`. The launcher's
native file dialog (nfd/GTK) has to be replaced on Android.
