#!/usr/bin/env bash
# Cross-compiles the DK64 Recompiled engine (N64ModernRuntime, RT64, RecompFrontend) for
# Android arm64 with the NDK, plus SDL2, FreeType and curl from vcpkg.
#
#   android/engine/build-engine.sh [targets...]
#
# With no targets it builds the engine libraries and compiles the game's own sources, which
# is what CI checks: linking the game also needs the code N64Recomp generates from a DK64
# ROM (host/RecompiledFuncs), which can't exist in CI. Pass DK64Recompiled once that code is
# in place to build libmain.so; android/app picks it up from android/app/engine-libs.
#
# Needs: ANDROID_NDK_HOME (or ANDROID_NDK_LATEST_HOME), ninja, a host C++ compiler, and the
# host submodules checked out (vcpkg with full history, for its baseline).
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
host="$repo/host"
build="${JIGGY_ANDROID_ENGINE_BUILD:-$repo/build-android-engine}"
ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_LATEST_HOME:-}}"
if [ -z "$ndk" ]; then
    echo "Set ANDROID_NDK_HOME to the Android NDK." >&2
    exit 1
fi
export ANDROID_NDK_HOME="$ndk" # vcpkg's Android triplets read this.

# Small changes RT64, plume and zstd need for Android (cross-compiling, no desktop file
# dialogs, rendering to SDL's window, no qsort_r). They apply to the pinned submodules and are skipped when
# already applied.
apply_patch() {
    local dir="$host/$1" patch="$repo/android/engine/patches/$2"
    if git -C "$dir" apply --check "$patch" 2>/dev/null; then
        git -C "$dir" apply "$patch"
    elif ! git -C "$dir" apply --check --reverse "$patch" 2>/dev/null; then
        echo "$2 doesn't apply to $dir; was the submodule updated?" >&2
        exit 1
    fi
}
apply_patch lib/rt64 rt64.patch
apply_patch lib/rt64/src/contrib/plume plume.patch
apply_patch lib/rt64/src/contrib/zstd zstd.patch

# file_to_c turns shaders and assets into C during the build, so it must run here.
mkdir -p "$build/host-tools"
c++ -std=c++17 -O2 -o "$build/host-tools/file_to_c" "$host/lib/rt64/src/tools/file_to_c/file_to_c.cpp"

# N64Recomp turns the game's patches into C during the build. Release builds bring the one
# they generated the game code with; otherwise build the runtime's copy for this machine.
if [ ! -x "$host/N64Recomp" ]; then
    cmake -S "$host/lib/N64ModernRuntime/N64Recomp" -B "$build/host-tools/N64Recomp" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build/host-tools/N64Recomp" --target N64RecompCLI
    cp "$build/host-tools/N64Recomp/N64Recomp" "$host/N64Recomp"
fi

if [ ! -x "$host/lib/vcpkg/vcpkg" ]; then
    "$host/lib/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
fi

# The generated game code (and the audio microcode from RSPRecomp) only exists after
# running the recompilers on the player's ROM. Placeholders let CMake configure without it.
if [ -z "$(ls "$host/RecompiledFuncs" 2>/dev/null)" ]; then
    mkdir -p "$host/RecompiledFuncs"
    echo "/* Placeholder: N64Recomp writes the game code here. */" > "$host/RecompiledFuncs/placeholder.c"
fi
if [ ! -e "$host/rsp/n_aspMain.cpp" ]; then
    mkdir -p "$host/rsp"
    echo "// Placeholder: RSPRecomp writes the audio microcode here." > "$host/rsp/n_aspMain.cpp"
fi

cmake -S "$host" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DVCPKG_TARGET_TRIPLET=arm64-android \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-28 \
    -DANDROID_STL=c++_static \
    -DRT64_HOST_FILE_TO_C="$build/host-tools/file_to_c"

if [ "$#" -eq 0 ]; then
    # The engine libraries, plus every game source file that doesn't include the code
    # generated from the ROM, compiled but not linked.
    host_objects=$(ninja -C "$build" -t targets all \
        | sed -n 's|^\(CMakeFiles/DK64Recompiled\.dir/src/[^:]*\.o\):.*|\1|p' \
        | grep -v -e register_overlays -e register_patches)
    set -- rt64 ultramodern librecomp recompui recompinput jiggy_bridge jiggy_updater $host_objects
fi
# -k 0 keeps going after an error, so one run reports every file that fails.
cmake --build "$build" --target "$@" -- -k 0

# Hand a freshly linked game to the Android app build.
for target in "$@"; do
    if [ "$target" = DK64Recompiled ]; then
        mkdir -p "$repo/android/app/engine-libs/arm64-v8a"
        cp "$build/libmain.so" "$repo/android/app/engine-libs/arm64-v8a/"
        echo "Copied libmain.so to android/app/engine-libs/arm64-v8a/"
    fi
done
