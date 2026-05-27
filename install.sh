#!/bin/bash
# MinUI Install-Script für den Anbernic RG353M
#
# Voraussetzungen:
#   - dArkOS einmal gebootet (expandtoexfat hat EASYROMS expandiert)
#   - SD-Karte im PC eingesteckt, EASYROMS und ROOTFS gemountet
#   - firmware/ mit gebauten Binaries (./build.sh ausführen)
#   - minui/MinUI.zip
#   - minui/Emus/rgb30/ und minui/Tools/rgb30/
#
# Verwendung:
#   ./install.sh [EASYROMS_PFAD] [ROOTFS_PFAD]
#   Standard: /run/media/$USER/EASYROMS  /run/media/$USER/ROOTFS

set -eo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

PLATFORM="rg353m"
FIRMWARE="$SCRIPT_DIR/firmware"
MINUI_ZIP="$SCRIPT_DIR/minui/MinUI.zip"
EXTRAS_DIR="$SCRIPT_DIR/minui"

EASYROMS="${1:-/run/media/$USER/EASYROMS}"
ROOTFS="${2:-/run/media/$USER/ROOTFS}"

# Script-Output in Logdatei spiegeln
INSTALL_LOG="$SCRIPT_DIR/install.log"
exec > >(tee -a "$INSTALL_LOG") 2>&1
echo ""
echo "=== RG353M MinUI Install $(date) ==="
echo "  EASYROMS: $EASYROMS"
echo "  ROOTFS:   $ROOTFS"
echo "  Log:      $INSTALL_LOG"

# ─── Voraussetzungen ──────────────────────────────────────────────────────────

for f in "$MINUI_ZIP" \
          "$FIRMWARE/minui.elf" "$FIRMWARE/minarch.elf" \
          "$FIRMWARE/keymon.elf" "$FIRMWARE/libmsettings.so"; do
    [ -f "$f" ] || { echo "FEHLER: fehlt: $f"; exit 1; }
done

[ -d "$EXTRAS_DIR/Emus/rgb30" ] && [ -d "$EXTRAS_DIR/Tools/rgb30" ] || {
    echo "FEHLER: Extras fehlen unter $EXTRAS_DIR/Emus/rgb30 und $EXTRAS_DIR/Tools/rgb30"
    exit 1
}

mountpoint -q "$EASYROMS" || { echo "FEHLER: $EASYROMS ist nicht gemountet"; exit 1; }
mountpoint -q "$ROOTFS"   || { echo "FEHLER: $ROOTFS ist nicht gemountet"; exit 1; }

# ─── Schritt 1: MinUI nach EASYROMS kopieren ─────────────────────────────────

echo ""
echo "=== Schritt 1: MinUI nach EASYROMS kopieren ==="

ROMS="$EASYROMS"

# dArkOS-Verzeichnisse entfernen — alles außer unseren MinUI-Dirs
# Roms/, Bios/, Saves/ werden NICHT gelöscht um bestehende Roms zu erhalten
echo "  Bereinige dArkOS-Verzeichnisse..."
find "$ROMS" -maxdepth 1 -mindepth 1 ! -name '.system' ! -name '.tmp_update' \
    ! -name '.userdata' ! -name 'Bios' ! -name 'Saves' ! -name 'Roms' \
    -exec rm -rf {} + 2>/dev/null || true

mkdir -p \
    "$ROMS/.system/$PLATFORM/bin" \
    "$ROMS/.system/$PLATFORM/lib" \
    "$ROMS/.system/$PLATFORM/cores" \
    "$ROMS/.system/$PLATFORM/paks/MinUI.pak" \
    "$ROMS/.system/$PLATFORM/paks/Emus" \
    "$ROMS/Tools/$PLATFORM" \
    "$ROMS/.system/res" \
    "$ROMS/.tmp_update" \
    "$ROMS/.userdata/$PLATFORM/logs" \
    "$ROMS/.userdata/shared/.minui" \
    "$ROMS/Saves" \
    "$ROMS/Bios" \
    "$ROMS/Roms/Game Boy (GB)" \
    "$ROMS/Roms/Game Boy Color (GBC)" \
    "$ROMS/Roms/Game Boy Advance (GBA)" \
    "$ROMS/Roms/Game Boy Advance (MGBA)" \
    "$ROMS/Roms/Nintendo Entertainment System (FC)" \
    "$ROMS/Roms/Super Nintendo Entertainment System (SFC)" \
    "$ROMS/Roms/Sega Genesis (MD)" \
    "$ROMS/Roms/Sega Master System (SMS)" \
    "$ROMS/Roms/Sega Game Gear (GG)" \
    "$ROMS/Roms/Sony PlayStation (PS)" \
    "$ROMS/Roms/TurboGrafx-16 (PCE)" \
    "$ROMS/Roms/Neo Geo Pocket Color (NGPC)" \
    "$ROMS/Roms/Super Game Boy (SGB)" \
    "$ROMS/Roms/Super Nintendo Entertainment System (SUPA)" \
    "$ROMS/Roms/Virtual Boy (VB)" \
    "$ROMS/Roms/Pokémon mini (PKM)" \
    "$ROMS/Roms/Pico-8 (P8)" \
    "$ROMS/Roms/Native Games (PAK)"

echo "  Verzeichnisse erstellt"

# Binaries: eigene Builds (RG353M-spezifisches SDL_WINDOW_SHOWN + Input-Events)
# libmsettings.so aus rgb30 (hat SetVolume/GetVolume korrekt implementiert)
for bin in minui minarch say syncsettings; do
    cp "$FIRMWARE/$bin.elf" "$ROMS/.system/$PLATFORM/bin/"
done
cp "$FIRMWARE/keymon.elf" "$ROMS/.system/$PLATFORM/bin/"
[ -f "$FIRMWARE/libSDL_ttf-2.0.so.0" ] && \
    cp "$FIRMWARE/libSDL_ttf-2.0.so.0" "$ROMS/.system/$PLATFORM/lib/"
python3 -c "
import zipfile, os
with zipfile.ZipFile('$MINUI_ZIP') as z:
    data = z.read('.system/rgb30/lib/libmsettings.so')
    open('$ROMS/.system/$PLATFORM/lib/libmsettings.so','wb').write(data)
    print('    libmsettings.so (rgb30)')
"
echo "  Binaries kopiert"

# Cores
echo "  Extrahiere Cores..."
python3 -c "
import zipfile, os
with zipfile.ZipFile('$MINUI_ZIP') as z:
    for n in z.namelist():
        if n.startswith('.system/rgb30/cores/') and n.endswith('.so'):
            out = '$ROMS/.system/$PLATFORM/cores/' + os.path.basename(n)
            open(out,'wb').write(z.read(n))
            print('    core:', os.path.basename(n))
"

# Assets
echo "  Extrahiere Assets..."
python3 -c "
import zipfile, os
with zipfile.ZipFile('$MINUI_ZIP') as z:
    for n in z.namelist():
        if not n.startswith('.system/res/'): continue
        rel = n[len('.system/res/'):]
        if not rel or n.endswith('/'): continue
        out = '$ROMS/.system/res/' + rel
        os.makedirs(os.path.dirname(out), exist_ok=True)
        open(out,'wb').write(z.read(n))
"

# Basis-Emus aus MinUI.zip (rgb30 PAKs: launch.sh + default.cfg)
echo "  Extrahiere Basis-Emus..."
python3 -c "
import zipfile, os
with zipfile.ZipFile('$MINUI_ZIP') as z:
    for n in z.namelist():
        if not n.startswith('.system/rgb30/paks/Emus/'): continue
        rel = n[len('.system/rgb30/paks/'):]
        if not rel or n.endswith('/'): continue
        out = '$ROMS/.system/$PLATFORM/paks/' + rel
        os.makedirs(os.path.dirname(out), exist_ok=True)
        open(out,'wb').write(z.read(n))
        print('    emu:', rel)
"

# Extras-Emus und Tools (nur Clock und Input — rgb30-kompatibel, aarch64)
cp -r "$EXTRAS_DIR/Emus/rgb30/"* "$ROMS/.system/$PLATFORM/paks/Emus/"
mkdir -p "$ROMS/Tools/$PLATFORM"
cp -r "$EXTRAS_DIR/Tools/rgb30/Clock.pak" "$ROMS/Tools/$PLATFORM/"
cp -r "$EXTRAS_DIR/Tools/rgb30/Input.pak" "$ROMS/Tools/$PLATFORM/"
cp -r "$EXTRAS_DIR/Tools/rgb30/Files.pak" "$ROMS/Tools/$PLATFORM/"
# dArkOS DinguxCommander (SDL2, 640x480) statt rgb30-Version (SDL1.2, 480x480)
[ -f "$FIRMWARE/DinguxCommander" ] && \
    cp "$FIRMWARE/DinguxCommander" "$ROMS/Tools/$PLATFORM/Files.pak/"
# Ersetze rgb30-Binaries durch selbst gebaute (SDL_WINDOW_SHOWN statt FULLSCREEN)
[ -f "$FIRMWARE/clock.elf"  ] && cp "$FIRMWARE/clock.elf"  "$ROMS/Tools/$PLATFORM/Clock.pak/"
[ -f "$FIRMWARE/minput.elf" ] && cp "$FIRMWARE/minput.elf" "$ROMS/Tools/$PLATFORM/Input.pak/"
echo "  Emus: $(ls "$ROMS/.system/$PLATFORM/paks/Emus/")"
echo "  Tools: $(ls "$ROMS/Tools/")"

# MinUI.pak/launch.sh
cat > "$ROMS/.system/$PLATFORM/paks/MinUI.pak/launch.sh" << 'LAUNCH'
#!/bin/sh
export PLATFORM="rg353m"
export SDCARD_PATH="/roms"
export BIOS_PATH="$SDCARD_PATH/Bios"
export SAVES_PATH="$SDCARD_PATH/Saves"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export DATETIME_PATH="$SHARED_USERDATA_PATH/datetime.txt"

mkdir -p "$LOGS_PATH" "$SHARED_USERDATA_PATH/.minui"

export PATH=$SYSTEM_PATH/bin:$PATH
export LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu:$SYSTEM_PATH/lib:/lib:$LD_LIBRARY_PATH
export SDL_VIDEODRIVER=kmsdrm
export SDL_VIDEO_EGL_DRIVER=libEGL.so
export SDL_AUDIODRIVER=alsa

LOG="$LOGS_PATH/minui.txt"
echo "=== $(date) === MinUI.pak/launch.sh started ===" >> "$LOG"
echo "  PATH=$PATH" >> "$LOG"
echo "  LD_LIBRARY_PATH=$LD_LIBRARY_PATH" >> "$LOG"
echo "  SYSTEM_PATH exists: $([ -d "$SYSTEM_PATH" ] && echo yes || echo NO)" >> "$LOG"
echo "  minui.elf exists: $([ -f "$SYSTEM_PATH/bin/minui.elf" ] && echo yes || echo NO)" >> "$LOG"
echo "  DRM devices: $(ls /dev/dri/ 2>&1)" >> "$LOG"

# Letzte bekannte Uhrzeit wiederherstellen (kein Hardware-RTC)
if [ -f "$DATETIME_PATH" ]; then
    SAVED_DT=$(cat "$DATETIME_PATH")
    date -s "$SAVED_DT" > /dev/null 2>&1 && \
        echo "  datetime restored: $SAVED_DT" >> "$LOG" || \
        echo "  datetime restore failed: $SAVED_DT" >> "$LOG"
fi

keymon.elf >> "$LOGS_PATH/keymon.txt" 2>&1 &
echo "  keymon.elf PID: $!" >> "$LOG"

cd "$(dirname "$0")"
EXEC_PATH=/tmp/minui_exec
NEXT_PATH=/tmp/next
AUTO_PATH="$USERDATA_PATH/auto.sh"

# Auto-Resume: wenn minarch beim letzten Shutdown eine auto.sh geschrieben hat
if [ -f "$AUTO_PATH" ]; then
    NEXT_CMD=$(cat "$AUTO_PATH")
    eval set -- "$NEXT_CMD"
    PAK_SCRIPT="$1"
    PAK_ROM="$2"
    echo "$(date +'%F %T') auto-resume: $PAK_SCRIPT $PAK_ROM" >> "$LOG"
    rm -f "$AUTO_PATH"
    "$PAK_SCRIPT" "$PAK_ROM" &
    wait $! 2>/dev/null || true
    while pidof minarch.elf > /dev/null 2>&1; do sleep 0.1; done
    echo "$(date +'%F %T') auto-resume emulator exited" >> "$LOG"
    sleep 0.3
    sync
fi

touch "$EXEC_PATH" && sync

while [ -f "$EXEC_PATH" ]; do
    echo "$(date +'%F %T') starting minui.elf" >> "$LOG"
    minui.elf >> "$LOG" 2>&1
    EXIT=$?
    echo "$(date +'%F %T') minui.elf exited (code=$EXIT) poweroff=$([ -f /tmp/poweroff ] && echo yes || echo no) next=$([ -f "$NEXT_PATH" ] && cat "$NEXT_PATH" || echo none)" >> "$LOG"
    echo "$(date +'%F %T')" > "$DATETIME_PATH"
    sync
    if [ -f /tmp/poweroff ]; then
        if [ -f "$NEXT_PATH" ]; then
            cp "$NEXT_PATH" "$AUTO_PATH"
            echo "$(date +'%F %T') auto-resume gespeichert: $(cat "$NEXT_PATH")" >> "$LOG"
            sync
        fi
        break
    fi
    if [ -f "$NEXT_PATH" ]; then
        NEXT_CMD=$(cat "$NEXT_PATH")
        eval set -- "$NEXT_CMD"
        PAK_SCRIPT="$1"
        PAK_ROM="$2"
        rm -f "$NEXT_PATH"
        echo "$(date +'%F %T') launching: $PAK_SCRIPT ${PAK_ROM:-(no rom)}" >> "$LOG"
        sleep 0.5
        if [ -n "$PAK_ROM" ]; then
            "$PAK_SCRIPT" "$PAK_ROM" &
        else
            "$PAK_SCRIPT" &
        fi
        LAUNCH_PID=$!
        # Warten: erst auf den direkten Child-Prozess, dann auf evtl. geforktes minarch.elf
        wait $LAUNCH_PID 2>/dev/null || true
        while pidof minarch.elf > /dev/null 2>&1; do sleep 0.1; done
        echo "$(date +'%F %T') pak fully exited" >> "$LOG"
        sleep 0.3
        sync
    fi
done

echo "$(date +'%F %T') loop exited, calling poweroff" >> "$LOG"
rm -f "$EXEC_PATH" /tmp/poweroff
sync
poweroff 2>/dev/null || systemctl poweroff 2>/dev/null || echo o > /proc/sysrq-trigger
LAUNCH
chmod +x "$ROMS/.system/$PLATFORM/paks/MinUI.pak/launch.sh"

# .tmp_update/launch.sh
cat > "$ROMS/.tmp_update/launch.sh" << 'LAUNCH_SH'
#!/bin/sh
LOG=/roms/minui_boot.log
exec >> "$LOG" 2>&1
set -x

echo "=== $(date) === .tmp_update/launch.sh started ==="
echo "  USER=$(id)"
echo "  /roms mounted: $(mountpoint -q /roms && echo yes || echo NO)"
echo "  /roms contents: $(ls /roms 2>&1)"

echo 0  > /sys/class/backlight/backlight/bl_power  2>/dev/null || true
echo 96 > /sys/class/backlight/backlight/brightness 2>/dev/null || true

LAUNCH=/roms/.system/rg353m/paks/MinUI.pak/launch.sh
if [ -f "$LAUNCH" ]; then
    exec "$LAUNCH"
else
    echo "FEHLER: $LAUNCH nicht gefunden"
    ls -la /roms/.system/ 2>&1 || true
    sleep 10
    poweroff 2>/dev/null || systemctl poweroff 2>/dev/null || echo o > /proc/sysrq-trigger
fi
LAUNCH_SH
chmod +x "$ROMS/.tmp_update/launch.sh"
cp "$ROMS/.tmp_update/launch.sh" "$ROMS/.tmp_update/updater"

echo "  EASYROMS befüllt: $(du -sh "$ROMS/.system" | cut -f1)"

# ─── Schritt 2: ROOTFS patchen (braucht sudo) ─────────────────────────────────

echo ""
echo "=== Schritt 2: ROOTFS patchen (sudo erforderlich) ==="

# ROOTFS remounten als rw (idempotent — kein Fehler wenn schon rw)
if findmnt -n -o OPTIONS "$ROOTFS" | grep -q '\bro\b'; then
    sudo mount -o remount,rw "$ROOTFS"
    echo "  ROOTFS remounted rw"
else
    echo "  ROOTFS bereits rw"
fi

# minui.service
sudo tee "$ROOTFS/etc/systemd/system/minui.service" > /dev/null << 'SVCEOF'
[Unit]
Description=MinUI
After=local-fs.target systemd-udevd.service roms.mount
Wants=local-fs.target roms.mount

[Service]
Type=simple
User=root
WorkingDirectory=/roms
ExecStart=/roms/.tmp_update/launch.sh
Environment="SDL_VIDEODRIVER=kmsdrm"
Environment="SDL_VIDEO_EGL_DRIVER=libEGL.so"
Environment="HOME=/home/ark"
Restart=no
StandardOutput=append:/roms/minui_boot.log
StandardError=append:/roms/minui_boot.log

[Install]
WantedBy=multi-user.target
SVCEOF

# Persistentes Journal
sudo mkdir -p "$ROOTFS/etc/systemd/journald.conf.d"
sudo tee "$ROOTFS/etc/systemd/journald.conf.d/persistent.conf" > /dev/null << 'JOURNALEOF'
[Journal]
Storage=persistent
Compress=yes
SystemMaxUse=32M
JOURNALEOF

# Services
WANTS="$ROOTFS/etc/systemd/system/multi-user.target.wants"
for svc in emulationstation NetworkManager wpa_supplicant networking ogage cron shutdowntasks; do
    sudo rm -f "$WANTS/$svc.service"
done
sudo rm -f "$ROOTFS/etc/systemd/system/sysinit.target.wants/welcome-message.service"
sudo ln -sf /etc/systemd/system/minui.service "$WANTS/minui.service"

# EmulationStation maskieren
sudo ln -sf /dev/null "$ROOTFS/etc/systemd/system/emulationstation.service"

# boot_text.sh: "Welcome to dArkOS" → "MinUI"
sudo sed -i 's/center_screen "Welcome to \$DISTRO_VERSION"/center_screen "MinUI"/g' \
    "$ROOTFS/usr/local/bin/boot_text.sh"

# sleep-hook: ES-Start nach Wake entfernen
SLEEP_HOOK="$ROOTFS/lib/systemd/system-sleep/sleep"
if grep -q "systemctl start emulationstation" "$SLEEP_HOOK" 2>/dev/null; then
    sudo python3 -c "
path = '$SLEEP_HOOK'
content = open(path).read()
content = content.replace(
    'systemctl is-active --quiet emulationstation.service && echo ok || systemctl start emulationstation',
    '# minui: ES start removed'
)
open(path, 'w').write(content)
"
    echo "  sleep-hook: ES-Start entfernt"
fi

# Journal-Verzeichnis voranlegen → systemd loggt ab erstem Boot persistent
sudo mkdir -p "$ROOTFS/var/log/journal"
sudo chmod 2755 "$ROOTFS/var/log/journal"
echo "  Journal-Verzeichnis angelegt: $(ls "$ROOTFS/var/log/journal")"

sudo mount -o remount,ro "$ROOTFS" 2>/dev/null || echo "  WARN: remount ro fehlgeschlagen (ignoriert)"

echo ""
echo "=== Schritt 3: SD-Karte unmounten ==="
for part in "$EASYROMS" "$ROOTFS" "/run/media/$USER/dArkOS_Fat"; do
    if mountpoint -q "$part" 2>/dev/null; then
        sudo umount "$part" && echo "  unmounted: $part" || echo "  WARN: unmount fehlgeschlagen: $part"
    fi
done

echo ""
echo "=== Fertig ==="
echo "SD-Karte kann jetzt sicher ausgeworfen werden."
