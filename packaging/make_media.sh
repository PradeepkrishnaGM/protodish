#!/usr/bin/env bash
# Makes the README media in docs/media/ (M8): four screenshots and the GIF, all under Xvfb.
# Needs the local debug build (ninja -C build-godot), xvfb-run, ffmpeg and Godot 4.7.
# Every image is the Stable preset, seed 6, where two lineages split by latitude by year 5.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
media="$root/docs/media"
mkdir -p "$media"
run=(--preset stable_d1a --seed 6)
xvfb=(xvfb-run -a -s "-screen 0 1280x800x24")

# Screenshots at about tick 10,000 (160 frames at 64 ticks per frame).
shot() {
  "${xvfb[@]}" godot --path "$root/godot" -s res://tests/screenshot.gd -- \
    "${run[@]}" --speed 64 "$@" | grep saved
}
shot --view 1 --frames 160 --out "$media/lineage.png"
shot --view 3 --frames 160 --out "$media/feeding.png"
shot --view 5 --frames 160 --out "$media/ground.png"
shot --view 1 --frames 320 --inspect 40 --out "$media/inspect.png"

# GIF: 400 frames at 32 ticks per frame (12,800 ticks, 6.4 years), recorded with Movie Maker.
# At 1280 × 800 the view draws 5 px per site: the strip is at x 277-286, the grid at x 292-931
# and y 80-719. The crop keeps one pixel per site, then scales up 3× with sharp edges.
frames=$(mktemp -d)
trap 'rm -rf "$frames"' EXIT
"${xvfb[@]}" godot --path "$root/godot" --write-movie "$frames/f.png" --fixed-fps 25 \
  --quit-after 400 -- "${run[@]}" --speed 32 --view 1
ffmpeg -loglevel error -y -framerate 25 -i "$frames/f%08d.png" -vf \
  "crop=655:640:277:80,scale=131:128:flags=neighbor,scale=393:384:flags=neighbor,split[a][b];[a]palettegen=max_colors=128:stats_mode=full[p];[b][p]paletteuse=dither=none" \
  -loop 0 "$media/demo.gif"
ls -l "$media"
