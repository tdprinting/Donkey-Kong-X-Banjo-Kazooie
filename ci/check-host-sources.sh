#!/usr/bin/env bash
# Compiles the host files Jiggy Jungle changes against the real runtime, RecompFrontend
# and RT64 headers. The full game needs the player's ROM to build, so CI can't link it;
# this catches API mistakes in our changes without one.
#
# Needs: clang++, SDL2 and libcurl development headers, and these submodules:
#   git submodule update --init --recursive --depth 1 host/lib/N64ModernRuntime host/lib/RecompFrontend
#   git -C host/lib/rt64 submodule update --init --depth 1 \
#       src/contrib/hlslpp src/contrib/plume src/contrib/nativefiledialog-extended src/contrib/stb
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
host="$root/host"
stubs="$(mktemp -d)"
trap 'rm -rf "$stubs"' EXIT

# Files CMake would generate.
echo 'inline const char icon_bytes[4] = {};' > "$stubs/icon_bytes.h"
echo '#define MINIZ_EXPORT' > "$stubs/miniz_export.h"

includes=(
    "$host/include"
    "$host/lib/N64ModernRuntime/N64Recomp/include"
    "$host/lib/N64ModernRuntime/ultramodern/include"
    "$host/lib/N64ModernRuntime/librecomp/include"
    "$host/lib/N64ModernRuntime/thirdparty"
    "$host/lib/N64ModernRuntime/thirdparty/concurrentqueue"
    "$host/lib/N64ModernRuntime/thirdparty/o1heap/o1heap"
    "$host/lib/N64ModernRuntime/thirdparty/miniz"
    "$host/lib/RecompFrontend/recompinput/include"
    "$host/lib/RecompFrontend/recompui/include"
    "$host/lib/RecompFrontend/recompui/src"
    "$host/lib/RecompFrontend/recompui/lib/RmlUi/Include"
    "$host/lib/RecompFrontend/lib/SlotMap"
    "$host/lib/RecompFrontend/lib/GamepadMotionHelpers"
    "$host/lib/SlotMap"
    "$host/lib/rt64/src"
    "$host/lib/rt64/src/contrib"
    "$host/lib/rt64/src/contrib/hlslpp/include"
    "$host/lib/rt64/src/rhi"
    "$host/lib/rt64/src/render"
    "$host/lib/rt64/src/contrib/nativefiledialog-extended/src/include"
    "$root/bridge/include"
    "$root/updater/include"
    /usr/include/SDL2
    "$stubs"
)
flags=(-std=c++20 -fsyntax-only -fms-extensions -DHLSL_CPU -Wall -Wextra -Wno-unused-parameter)
for dir in "${includes[@]}"; do
    flags+=(-I "$dir")
done

status=0
for file in src/main/main.cpp src/game/jiggy_api.cpp src/game/jiggy_updates.cpp; do
    echo "checking host/$file"
    # Only fail on our own files' warnings; upstream code has some we don't own.
    if ! output="$(cd "$host" && clang++ "${flags[@]}" "$file" 2>&1)"; then
        echo "$output"
        status=1
    elif grep -E "jiggy[a-z_]*\.(cpp|h|hpp):[0-9]+:[0-9]+: warning" <<<"$output"; then
        status=1
    fi
done
exit $status
