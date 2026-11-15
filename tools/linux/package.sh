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
PACKAGE_NAME="IndustrialFrontier-Stage10-Part1-Linux-x64"
STAGING="$DIST_DIR/$PACKAGE_NAME"
EXECUTABLE="$BUILD_DIR/IndustrialFrontier"

if [[ ! -x "$EXECUTABLE" ]]; then
    echo "ERROR: executable not found: $EXECUTABLE" >&2
    exit 1
fi

if [[ ! -d "$ROOT/assets" ]]; then
    echo "ERROR: assets directory not found." >&2
    exit 1
fi

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "ERROR: Linux x86_64 required." >&2
    exit 1
fi

if ! command -v readelf >/dev/null 2>&1; then
    echo "ERROR: readelf is required." >&2
    exit 1
fi

mkdir -p "$DIST_DIR"
rm -rf "$STAGING"
mkdir -p "$STAGING/lib"

cp "$EXECUTABLE" "$STAGING/IndustrialFrontier"
cp -a "$ROOT/assets" "$STAGING/assets"

while IFS= read -r needed; do
    [[ -n "$needed" ]] || continue

    found="$(find "$BUILD_DIR" -type f -name "${needed}*" -print -quit)"

    if [[ -z "$found" ]]; then
        echo "ERROR: missing runtime $needed" >&2
        exit 1
    fi

    cp -L "$found" "$STAGING/lib/$needed"
done < <(readelf -d "$EXECUTABLE" | sed -n -E 's/.*\(NEEDED\).*\[(libSDL3(_ttf)?\.so[^]]*)\].*/\1/p')

cat > "$STAGING/run.sh" <<'LAUNCHER'
#!/usr/bin/env bash
set -Eeuo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

cd "$HERE"

exec ./IndustrialFrontier "$@"
LAUNCHER

chmod +x "$STAGING/run.sh"

cat > "$STAGING/README-LINUX.txt" <<'NOTES'
Industrial Frontier - Stage 10 Part One

Platform: Linux x86_64
Required: OpenGL 3.3 compatible graphics driver
Desktop environment: X11 or Wayland

Launch:
./run.sh

Ubuntu 24.04 is the primary build target.
This is an in-development showcase release.
NOTES

tar -C "$DIST_DIR" -czf "$DIST_DIR/$PACKAGE_NAME.tar.gz" "$PACKAGE_NAME"

echo "Linux package created successfully."
