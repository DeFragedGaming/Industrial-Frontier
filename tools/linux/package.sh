#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

resolve_path() {
    if [[ "$1" = /* ]]; then
        printf '%s\n' "$1"
    else
        printf '%s\n' "$ROOT/$1"
    fi
}

BUILD_DIR="$(resolve_path "${1:-build-linux}")"
DIST_DIR="$(resolve_path "${2:-dist}")"
PACKAGE_NAME="IndustrialFrontier-Linux-x64"
STAGING="$DIST_DIR/$PACKAGE_NAME"
EXECUTABLE="$BUILD_DIR/IndustrialFrontier"
ARCHIVE="$DIST_DIR/$PACKAGE_NAME.tar.gz"

[[ "$(uname -m)" == "x86_64" ]] || { echo "Linux x86_64 is required" >&2; exit 1; }
[[ -x "$EXECUTABLE" ]] || { echo "Missing executable: $EXECUTABLE" >&2; exit 1; }
[[ -d "$ROOT/assets" ]] || { echo "Missing assets directory" >&2; exit 1; }
command -v readelf >/dev/null || { echo "Missing readelf (binutils)" >&2; exit 1; }
command -v sha256sum >/dev/null || { echo "Missing sha256sum" >&2; exit 1; }

mkdir -p "$DIST_DIR"
rm -rf "$STAGING"
mkdir -p "$STAGING/lib"
cp "$EXECUTABLE" "$STAGING/IndustrialFrontier"
cp -a "$ROOT/assets" "$STAGING/assets"

mapfile -t required_libraries < <(
    readelf -d "$EXECUTABLE" | sed -n -E 's/.*\(NEEDED\).*\[(libSDL3(_ttf)?\.so[^]]*)\].*/\1/p'
)

for soname in "${required_libraries[@]}"; do
    [[ -n "$soname" ]] || continue
    library="$(find "$BUILD_DIR" -type f \( -name "$soname" -o -name "$soname.*" \) -print -quit)"
    [[ -n "$library" ]] || { echo "Missing bundled runtime: $soname" >&2; exit 1; }
    cp -L "$library" "$STAGING/lib/$soname"
done

cat > "$STAGING/run.sh" <<'LAUNCHER'
#!/usr/bin/env bash
set -Eeuo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$HERE"
exec ./IndustrialFrontier "$@"
LAUNCHER

chmod +x "$STAGING/run.sh" "$STAGING/IndustrialFrontier"

if command -v git >/dev/null && git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$ROOT" rev-parse HEAD > "$STAGING/BUILD_COMMIT.txt"
fi

cat > "$STAGING/README-LINUX.txt" <<'NOTES'
Industrial Frontier - Linux x64

Built on Ubuntu 24.04 using C++20, SDL3 and OpenGL.
Requires an OpenGL 3.3-capable graphics driver and desktop session.
Extract the archive and run ./run.sh from the extracted folder.
System OpenGL, graphics drivers, X11/Wayland and glibc are not bundled.
This is a development build. Other Linux distributions are not certified.
NOTES

rm -f "$ARCHIVE" "$ARCHIVE.sha256"
tar -C "$DIST_DIR" -czf "$ARCHIVE" "$PACKAGE_NAME"
[[ -s "$ARCHIVE" ]] || { echo "Archive creation failed" >&2; exit 1; }
(cd "$DIST_DIR" && sha256sum "$PACKAGE_NAME.tar.gz" > "$PACKAGE_NAME.tar.gz.sha256")
printf 'Linux package: %s\n' "$ARCHIVE"
