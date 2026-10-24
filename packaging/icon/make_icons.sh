#!/bin/sh
# Regenerates packaging/mac/KeyGuide.icns and packaging/windows/KeyGuide.ico from keyguide.svg.
# Needs macOS 14+ (swift, iconutil) and python3; the outputs are committed, builds do not run this.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
packaging=$(dirname "$here")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

swift "$here/render_icon.swift" "$here/keyguide.svg" "$work" 16 24 32 48 64 128 256 512 1024

iconset="$work/KeyGuide.iconset"
mkdir "$iconset"
for size in 16 32 128 256 512; do
    cp "$work/$size.png" "$iconset/icon_${size}x${size}.png"
    cp "$work/$((size * 2)).png" "$iconset/icon_${size}x${size}@2x.png"
done
iconutil -c icns "$iconset" -o "$packaging/mac/KeyGuide.icns"

# ICO with PNG-compressed entries (supported since Windows Vista).
python3 -I - "$work" "$packaging/windows/KeyGuide.ico" <<'EOF'
import struct, sys, pathlib
work, target = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
sizes = [16, 24, 32, 48, 64, 128, 256]
images = [(work / f"{size}.png").read_bytes() for size in sizes]
header = struct.pack("<HHH", 0, 1, len(sizes))
offset = len(header) + 16 * len(sizes)
entries = b""
for size, data in zip(sizes, images):
    entries += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset)
    offset += len(data)
target.write_bytes(header + entries + b"".join(images))
EOF

cp "$work/256.png" "$here/keyguide_256.png"
echo "Wrote $packaging/mac/KeyGuide.icns, $packaging/windows/KeyGuide.ico and keyguide_256.png"
