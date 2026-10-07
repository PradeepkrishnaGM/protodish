#!/usr/bin/env bash
# Exports the Godot app and packages it (M8). Run after build-extension.sh for the same platform.
#   packaging/package.sh linux   GODOT VERSION OUTDIR   # -> OUTDIR/Protodish-VERSION-x86_64.AppImage
#   packaging/package.sh windows GODOT VERSION OUTDIR   # -> OUTDIR/Protodish-VERSION-windows-x86_64.zip
# GODOT is the official editor binary with the 4.7.2 export templates installed.
# The Linux build needs appimagetool on PATH (or APPIMAGETOOL set to its path). APPIMAGE_RUNTIME
# may name a downloaded type2 runtime; otherwise appimagetool downloads the latest one.
set -euo pipefail
platform=${1:?usage: package.sh linux|windows GODOT VERSION OUTDIR}
godot=${2:?missing GODOT}
version=${3:?missing VERSION}
outdir=$(mkdir -p "${4:?missing OUTDIR}" && cd "$4" && pwd)
root=$(cd "$(dirname "$0")/.." && pwd)
project="$root/godot"
stage="$root/build-release-$platform/stage"
rm -rf "$stage"
mkdir -p "$stage"

# Licenses shipped with every download: ours, and the notices for Godot and godot-cpp.
copy_licenses() {
  mkdir -p "$1"
  cp "$root/LICENSE" "$root/THIRD_PARTY.md" "$root"/packaging/licenses/* "$1/"
}

# The first import registers the extension; it must finish before the export starts.
"$godot" --headless --path "$project" --import

case "$platform" in
  linux)
    appdir="$stage/Protodish.AppDir"
    mkdir -p "$appdir/usr/bin"
    "$godot" --headless --path "$project" --export-release Linux "$appdir/usr/bin/protodish.x86_64"
    # Smoke test: the exported program loads the extension and runs a few frames.
    log=$("$appdir/usr/bin/protodish.x86_64" --headless --quit-after 120 2>&1) || true
    if grep -E "ERROR|SCRIPT ERROR|Can't open dynamic library" <<<"$log"; then
      echo "smoke test failed" >&2
      exit 1
    fi
    cp "$root/packaging/appimage/AppRun" "$root/packaging/appimage/protodish.desktop" "$appdir/"
    cp "$project/icon.png" "$appdir/protodish.png"
    copy_licenses "$appdir/usr/share/doc/protodish"
    chmod +x "$appdir/AppRun"
    out="$outdir/Protodish-$version-x86_64.AppImage"
    tool_args=(--no-appstream)
    if [[ -n "${APPIMAGE_RUNTIME:-}" ]]; then
      tool_args+=(--runtime-file "$APPIMAGE_RUNTIME")
    fi
    ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGETOOL:-appimagetool}" "${tool_args[@]}" "$appdir" "$out"
    ;;
  windows)
    name="Protodish-$version-windows-x86_64"
    dir="$stage/$name"
    mkdir -p "$dir"
    "$godot" --headless --path "$project" --export-release Windows "$dir/Protodish.exe"
    copy_licenses "$dir/licenses"
    # Only Windows system DLLs may be needed: the C++ runtime is linked statically.
    if command -v x86_64-w64-mingw32-objdump >/dev/null; then
      x86_64-w64-mingw32-objdump -p "$dir"/*.dll | grep "DLL Name" | sort -u
      if x86_64-w64-mingw32-objdump -p "$dir"/*.dll | grep "DLL Name" \
          | grep -iE "libstdc|libgcc|libwinpthread"; then
        echo "the library needs MinGW runtime DLLs" >&2
        exit 1
      fi
    fi
    out="$outdir/$name.zip"
    rm -f "$out"
    (cd "$stage" && zip -r -9 "$out" "$name")
    ;;
  *)
    echo "unknown platform: $platform" >&2
    exit 2
    ;;
esac
ls -l "$out"
