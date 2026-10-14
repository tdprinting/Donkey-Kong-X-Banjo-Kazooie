#!/usr/bin/env bash
# Cross-compiles a Windows test build of the game from Linux with llvm-mingw, and packages
# it as a zip. Android is the target platform; this is for quick tests on a Windows PC.
# Release builds still come from release.yml (MSVC on Windows).
#
#   tools/windows/build-windows.sh [targets...]
#
# With no targets it builds the game and writes build-windows/JiggyJungle-windows.zip. Like
# the desktop and Android builds, the game code must first be generated from your DK64 ROM
# in host/ (host/BUILDING.md); without it, pass targets such as "rt64 librecomp" to only
# check that the engine compiles. The zip has the compiled game but no ROMs: players load
# their own in the launcher.
#
# Needs: cmake, ninja, git, curl, python3, zip, a host C++ compiler, clang and ld.lld (for
# the game patches), and the host submodules checked out (vcpkg with full history). The
# toolchain and libraries below are downloaded once into the build folder.
set -euo pipefail

repo="$(cd "$(dirname "$0")/../.." && pwd)"
host="$repo/host"
build="${JIGGY_WINDOWS_BUILD:-$repo/build-windows}"
deps="$build/deps"
mkdir -p "$deps"

llvm_mingw_version=20250924
llvm_mingw="$deps/llvm-mingw-$llvm_mingw_version-ucrt-ubuntu-22.04-x86_64"
directx_headers_commit=461ef5138bd30922b44fbad77c4e47662812d2d5
sdl2_version=2.30.3
curl_tag=curl-8_10_1
freetype_tag=VER-2-13-3
prefix="$deps/prefix"

# llvm-mingw: clang targeting Windows, with the UCRT runtime.
if [ ! -x "$llvm_mingw/bin/x86_64-w64-mingw32-clang++" ]; then
    curl -sSL "https://github.com/mstorsjo/llvm-mingw/releases/download/$llvm_mingw_version/$(basename "$llvm_mingw").tar.xz" \
        | tar xJ -C "$deps"
fi
export PATH="$llvm_mingw/bin:$PATH"

# DirectX-Headers: RT64 needs newer D3D12 headers than MinGW ships.
if [ ! -d "$deps/DirectX-Headers/include" ]; then
    git init -q "$deps/DirectX-Headers"
    git -C "$deps/DirectX-Headers" fetch -q --depth 1 https://github.com/microsoft/DirectX-Headers.git "$directx_headers_commit"
    git -C "$deps/DirectX-Headers" checkout -q FETCH_HEAD
fi

# MinGW's headers are lowercase, and Linux file names are case-sensitive, so includes such
# as <Windows.h> need a lowercase twin. The shim links every include in host/ that differs
# from MinGW's header only by case.
python3 - "$host" "$llvm_mingw/x86_64-w64-mingw32/include" "$deps/case-shim" <<'EOF'
import os, re, sys
host, mingw, shim = sys.argv[1:]
by_lower = {}
for root, _, files in os.walk(mingw):
    for name in files:
        rel = os.path.relpath(os.path.join(root, name), mingw)
        by_lower.setdefault(rel.lower(), rel)
includes = set()
for root, _, files in os.walk(host):
    if "cmake-build" in root or "/.git" in root:
        continue
    for name in files:
        if name.endswith((".c", ".cpp", ".h", ".hpp", ".inl", ".cc")):
            with open(os.path.join(root, name), errors="ignore") as f:
                includes.update(re.findall(r'#\s*include\s*[<"]([^>"]+)[>"]', f.read()))
for inc in includes:
    real = by_lower.get(inc.lower())
    if real and real != inc and not os.path.exists(os.path.join(mingw, inc)):
        link = os.path.join(shim, inc)
        os.makedirs(os.path.dirname(link), exist_ok=True)
        if not os.path.lexists(link):
            os.symlink(os.path.join(mingw, real), link)
EOF

toolchain="$deps/mingw-toolchain.cmake"
cat > "$toolchain" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER $llvm_mingw/bin/x86_64-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER $llvm_mingw/bin/x86_64-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER $llvm_mingw/bin/x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH $llvm_mingw/x86_64-w64-mingw32 $prefix)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# ultra64.h redefines NULL, so stddef.h has to come first. _declspec is an MSVC spelling
# some of the runtime uses.
set(JIGGY_MINGW_FLAGS "-isystem $deps/DirectX-Headers/include/directx -isystem $deps/DirectX-Headers/include -isystem $deps/DirectX-Headers/include/dxguids -idirafter $deps/case-shim -include stddef.h -D_WIN32_WINNT=0x0A00 -D_declspec=__declspec")
set(CMAKE_C_FLAGS_INIT "\${JIGGY_MINGW_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "\${JIGGY_MINGW_FLAGS}")
EOF

# curl (Windows' own TLS) and FreeType, built static. The prebuilt ones upstream bundles are
# MSVC builds that MinGW can't link.
if [ ! -f "$prefix/lib/libcurl.a" ]; then
    git clone -q --depth 1 --branch "$curl_tag" https://github.com/curl/curl.git "$deps/curl"
    cmake -S "$deps/curl" -B "$deps/curl-build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCURL_USE_SCHANNEL=ON \
        -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF -DCURL_ZLIB=OFF -DUSE_NGHTTP2=OFF \
        -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF -DUSE_LIBIDN2=OFF -DBUILD_CURL_EXE=OFF \
        -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$prefix" >/dev/null
    cmake --build "$deps/curl-build" --target install >/dev/null
fi
if [ ! -f "$prefix/lib/libfreetype.a" ]; then
    git clone -q --depth 1 --branch "$freetype_tag" https://github.com/freetype/freetype.git "$deps/freetype"
    cmake -S "$deps/freetype" -B "$deps/freetype-build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON \
        -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON -DCMAKE_INSTALL_PREFIX="$prefix" >/dev/null
    cmake --build "$deps/freetype-build" --target install >/dev/null
fi

# SDL2's MinGW package, laid out the way host/CMakeLists.txt expects its Visual Studio zip.
sdl2="$deps/sdl2"
if [ ! -f "$sdl2/lib/x64/SDL2.dll" ]; then
    curl -sSL "https://github.com/libsdl-org/SDL/releases/download/release-$sdl2_version/SDL2-devel-$sdl2_version-mingw.tar.gz" \
        | tar xz -C "$deps"
    sdl2_mingw="$deps/SDL2-$sdl2_version/x86_64-w64-mingw32"
    mkdir -p "$sdl2/lib/x64"
    cp -r "$sdl2_mingw/include/SDL2" "$sdl2/include"
    cp "$sdl2_mingw/lib/libSDL2.dll.a" "$sdl2_mingw/bin/SDL2.dll" "$sdl2/lib/x64/"
fi

# Small changes the submodules need for MinGW, on top of the Android ones (which add the
# option to run a build-machine file_to_c). Skipped when already applied.
apply_patch() {
    local dir="$host/$1" patch="$2"
    if git -C "$dir" apply --check "$patch" 2>/dev/null; then
        git -C "$dir" apply "$patch"
    elif ! git -C "$dir" apply --check --reverse "$patch" 2>/dev/null; then
        echo "$(basename "$patch") doesn't apply to $dir; was the submodule updated?" >&2
        exit 1
    fi
}
apply_patch lib/rt64 "$repo/android/engine/patches/rt64.patch"
apply_patch lib/rt64 "$repo/tools/windows/patches/rt64-mingw.patch"
apply_patch lib/rt64/src/contrib/plume "$repo/tools/windows/patches/plume-mingw.patch"
apply_patch lib/RecompFrontend "$repo/tools/windows/patches/recompfrontend-mingw.patch"

# file_to_c turns shaders and assets into C during the build, so it must run here.
mkdir -p "$build/host-tools"
c++ -std=c++17 -O2 -o "$build/host-tools/file_to_c" "$host/lib/rt64/src/tools/file_to_c/file_to_c.cpp"

# N64Recomp turns the game's patches into C during the build.
if [ ! -x "$host/N64Recomp" ]; then
    cmake -S "$host/lib/N64ModernRuntime/N64Recomp" -B "$build/host-tools/N64Recomp" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build/host-tools/N64Recomp" --target N64RecompCLI
    cp "$build/host-tools/N64Recomp/N64Recomp" "$host/N64Recomp"
fi

if [ ! -x "$host/lib/vcpkg/vcpkg" ]; then
    "$host/lib/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
fi

# Every dependency is built above, so vcpkg only provides its toolchain wrapper here.
cmake -S "$host" -B "$build/game" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DVCPKG_MANIFEST_INSTALL=OFF \
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$toolchain" \
    -DCURL_DIR="$prefix/lib/cmake/CURL" \
    -DFREETYPE_MINGW_ROOT="$prefix" \
    -DFETCHCONTENT_SOURCE_DIR_SDL2="$sdl2" \
    -DRT64_HOST_FILE_TO_C="$build/host-tools/file_to_c"

if [ "$#" -gt 0 ]; then
    cmake --build "$build/game" --target "$@" -- -k 0
    exit 0
fi

if [ -z "$(ls "$host/RecompiledFuncs" 2>/dev/null)" ] || [ ! -e "$host/rsp/n_aspMain.cpp" ]; then
    echo "The game code hasn't been generated in host/ yet; see host/BUILDING.md." >&2
    exit 1
fi
cmake --build "$build/game" --target DK64Recompiled

# The zip: the game, its DLLs (llvm-mingw's C++ runtime too) and assets, and no ROM data.
package="$build/package/JiggyJungle"
rm -rf "$build/package"
mkdir -p "$package"
cp "$build/game/DK64Recompiled.exe" "$build/game/SDL2.dll" "$build/game/dxcompiler.dll" \
    "$build/game/dxil.dll" "$llvm_mingw/x86_64-w64-mingw32/bin/libc++.dll" \
    "$llvm_mingw/x86_64-w64-mingw32/bin/libunwind.dll" "$host/recompcontrollerdb.txt" "$package/"
cp -r "$host/assets" "$package/"
sed 's/$/\r/' "$repo/tools/windows/README.txt" > "$package/README.txt"
rm -f "$build/JiggyJungle-windows.zip"
(cd "$build/package" && zip -qr9 "$build/JiggyJungle-windows.zip" JiggyJungle)
echo "Wrote $build/JiggyJungle-windows.zip"
