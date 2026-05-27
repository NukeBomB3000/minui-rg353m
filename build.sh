#!/bin/bash
# Build MinUI für den Anbernic RG353M (dArkOS / KMS / RK3566)
#
# Voraussetzungen:
#   - Docker mit rgb30-toolchain Image (vom GKD Pixel 2 Build)
#   - minui/MinUI.zip (MinUI-Release-Paket)
#   - .minui-src/ (MinUI Quellcode — wird automatisch geklont)
#
# Verwendung:
#   ./build.sh              — baut alle Binaries nach firmware/
#   ./build.sh --install    — baut und installiert auf SD-Karte

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLATFORM="rg353m"
MINUI_SRC="$SCRIPT_DIR/.minui-src"
FIRMWARE="$SCRIPT_DIR/firmware"
WORKSPACE="$MINUI_SRC/workspace"

echo "=== MinUI Build für $PLATFORM ==="

# ─── Schritt 1: MinUI Quellcode ───────────────────────────────────────────────

if [ ! -d "$MINUI_SRC" ]; then
    echo "=== Klone MinUI Quellcode ==="
    git clone --depth=1 https://github.com/shauninman/MinUI "$MINUI_SRC"
fi

# ─── Schritt 2: Platform-Files in MinUI-Workspace synchronisieren ─────────────

echo "=== Synchronisiere Platform-Files ==="
mkdir -p "$WORKSPACE/$PLATFORM/platform" \
         "$WORKSPACE/$PLATFORM/keymon" \
         "$WORKSPACE/$PLATFORM/libmsettings"

cp "$SCRIPT_DIR/platform/platform.h"    "$WORKSPACE/$PLATFORM/platform/"
cp "$SCRIPT_DIR/platform/platform.c"    "$WORKSPACE/$PLATFORM/platform/"
cp "$SCRIPT_DIR/platform/makefile.env"  "$WORKSPACE/$PLATFORM/platform/"
cp "$SCRIPT_DIR/platform/makefile.copy" "$WORKSPACE/$PLATFORM/platform/"
cp "$SCRIPT_DIR/keymon/keymon.c"        "$WORKSPACE/$PLATFORM/keymon/"
cp "$SCRIPT_DIR/libmsettings/msettings.c" "$WORKSPACE/$PLATFORM/libmsettings/"
cp "$SCRIPT_DIR/libmsettings/msettings.h" "$WORKSPACE/$PLATFORM/libmsettings/"
cp "$SCRIPT_DIR/libmsettings/makefile"    "$WORKSPACE/$PLATFORM/libmsettings/"

# workspace/rg353m/makefile braucht MinUI (kopiere von gkdpixel als Vorlage)
[ -f "$WORKSPACE/$PLATFORM/makefile" ] || \
    cp "$WORKSPACE/gkdpixel/makefile" "$WORKSPACE/$PLATFORM/makefile" 2>/dev/null || \
    echo "all:" > "$WORKSPACE/$PLATFORM/makefile"

# ─── Schritt 3: Bauen im Docker (rgb30-toolchain) ─────────────────────────────

echo "=== Baue in Docker (rgb30-toolchain) ==="

docker run --rm \
    -v "$WORKSPACE":/root/workspace \
    rgb30-toolchain /bin/bash -c "
set -e
export PLATFORM=$PLATFORM
export UNION_PLATFORM=$PLATFORM
cd /root/workspace

echo '--- libmsettings ---'
cd $PLATFORM/libmsettings
make
cd /root/workspace

echo '--- minui ---'
cd all/minui
make PLATFORM=$PLATFORM
cd /root/workspace

echo '--- minarch ---'
cd all/minarch
make PLATFORM=$PLATFORM
cd /root/workspace

echo '--- say ---'
cd all/say
make PLATFORM=$PLATFORM
cd /root/workspace

echo '--- syncsettings ---'
cd all/syncsettings
make PLATFORM=$PLATFORM
cd /root/workspace

echo '--- clock ---'
cd all/clock
make PLATFORM=$PLATFORM
cd /root/workspace

echo '--- minput ---'
cd all/minput
make PLATFORM=$PLATFORM
cd /root/workspace

echo '--- keymon ---'
cd $PLATFORM/keymon
\${CROSS_COMPILE}gcc -O2 \
    -I../libmsettings \
    -I../../../workspace/all/common \
    -L../libmsettings \
    -o keymon.elf keymon.c \
    -lmsettings -lpthread
cd /root/workspace

echo '--- Build abgeschlossen ---'
"

# ─── Schritt 4: Binaries einsammeln ──────────────────────────────────────────

echo "=== Sammle Binaries nach firmware/ ==="
mkdir -p "$FIRMWARE"

cp "$WORKSPACE/all/minui/build/$PLATFORM/minui.elf"               "$FIRMWARE/"
cp "$WORKSPACE/all/minarch/build/$PLATFORM/minarch.elf"           "$FIRMWARE/"
cp "$WORKSPACE/all/say/build/$PLATFORM/say.elf"                   "$FIRMWARE/"
cp "$WORKSPACE/all/syncsettings/build/$PLATFORM/syncsettings.elf" "$FIRMWARE/"
cp "$WORKSPACE/$PLATFORM/libmsettings/libmsettings.so"            "$FIRMWARE/"
cp "$WORKSPACE/all/clock/build/$PLATFORM/clock.elf"               "$FIRMWARE/"
cp "$WORKSPACE/all/minput/build/$PLATFORM/minput.elf"             "$FIRMWARE/"

# Docker baut als root — keymon.elf via Docker herauskopieren
docker run --rm \
    -v "$WORKSPACE":/root/workspace \
    -v "$FIRMWARE":/out \
    rgb30-toolchain cp /root/workspace/$PLATFORM/keymon/keymon.elf /out/

echo ""
echo "=== Fertig ==="
ls -lh "$FIRMWARE/"

# ─── Optional: Auf SD installieren ───────────────────────────────────────────

if [ "$1" = "--install" ]; then
    "$SCRIPT_DIR/install_to_sd.sh"
fi
