#!/usr/bin/env bash
# Builds the release GDExtension into godot/bin/ (M8). Used by CI and for local release builds.
#   packaging/build-extension.sh linux     # also builds and runs the fast core tests
#   packaging/build-extension.sh windows   # cross-compiles with MinGW-w64
# Release builds link libstdc++ and libgcc statically (and winpthread on Windows), so the
# library needs nothing beyond the system's C library.
set -euo pipefail
platform=${1:?usage: build-extension.sh linux|windows}
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/build-release-$platform"

args=(-S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DEVO_BUILD_GODOT=ON
      -DGODOTCPP_TARGET=template_release -DGODOTCPP_USE_STATIC_CPP=ON)
if command -v ccache >/dev/null; then
  args+=(-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache)
fi

case "$platform" in
  linux)
    cmake "${args[@]}"
    ninja -C "$build" protodish evolve evo_tests
    (cd "$build" && ctest -L fast --output-on-failure)
    ;;
  windows)
    cmake "${args[@]}" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchain-mingw-w64-x86_64.cmake"
    ninja -C "$build" protodish
    ;;
  *)
    echo "unknown platform: $platform" >&2
    exit 2
    ;;
esac
ls -l "$root"/godot/bin/*template_release*
