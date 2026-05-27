# RG353M — MinUI Port: Architektur & Stand

## Hardware

| | |
|--|--|
| **SoC** | Rockchip RK3566 (aarch64, Cortex-A55, Quad-Core @ 1.8 GHz) |
| **GPU** | Mali-G52 MP2 (libEGL_mesa / Panfrost) |
| **RAM** | 2 GB LPDDR4 |
| **Display** | 3,5" IPS, 640×480 |
| **Speicher** | 32 GB eMMC intern (Android, unberührt) + SD-Slot |
| **Kernel** | 5.10.x |
| **OS-Basis** | Debian GNU/Linux 13 (trixie) |
| **CFW** | dArkOS |

## SD-Karten-Partitionslayout

| Nr | Label | Größe | FS | Funktion |
|----|-------|-------|----|----------|
| p1 | — | 4 MB | raw | idbloader (Rockchip BL1/BL2) |
| p2 | — | 4 MB | raw | U-Boot |
| p3 | dArkOS_Fat | 99 MB | FAT32 | Kernel, DTB, extlinux |
| p4 | ROOTFS | ~9 GB | btrfs | Debian-Root |
| p5 | EASYROMS | Rest | exFAT | ROMs, MinUI-System, Saves |

**Wichtig:** Die eMMC enthält Android und bleibt unberührt. Der RK3566-ROM prüft SD vor eMMC.

## Boot-Chain

```
RK3566 ROM → idbloader (p1) → U-Boot (p2)
→ extlinux.conf (p3) → Kernel + DTB + uInitrd
→ systemd (ROOTFS/p4)
→ emulationstation.service → emulationstation.sh
→ prüft /roms/.tmp_update/updater
→ MinUI.pak/launch.sh
```

`emulationstation.sh` wurde so eingerichtet, dass es `/roms/.tmp_update/updater` prüft und
bei Vorhandensein MinUI startet statt EmulationStation.

## Grafik / SDL

- **Kein Wayland, kein X11** — direktes KMS/DRM
- **SDL_VIDEODRIVER:** `kmsdrm`
- **SDL_WINDOW_SHOWN** (nicht FULLSCREEN — sonst schwarzer Screen auf KMS)
- **EGL:** `libEGL_mesa.so`
- DRM-Device: `/dev/dri/card0`, Render: `/dev/dri/renderD128`

## Input-Events

| Device | Event | Verwendung |
|--------|-------|------------|
| rk805 pwrkey | event0 | Power-Taste (KEY_POWER 116) |
| Touchscreen | event1 | ignoriert |
| adc-keys | event2 | ignoriert |
| gpio-keys | event3 | Vol Up/Down (115/114) |
| retrogame_joypad | **event4** | **Hauptgamepad** |
| rk-headset | event5 | ignoriert |

### Gamepad-Keycodes (event4)

| Button | Keycode |
|--------|---------|
| A | 304 BTN_SOUTH |
| B | 305 BTN_EAST |
| X | 307 BTN_NORTH |
| Y | 308 BTN_WEST |
| L1/R1 | 310/311 |
| L2/R2 | 312/313 |
| Select | 314 |
| Start | 315 |
| Menu (F) | 316 BTN_MODE |
| L3/R3 | 317/318 |
| D-Pad | 544–547 |

Analog-Sticks: ABS_X/ABS_Y (links), ABS_RX/ABS_RY (rechts), Range: -1800…+1800

## Projektstruktur

```
rg353m-minui-port/
├── build.sh              — Docker-Build (rgb30-toolchain)
├── install.sh            — Deployment auf gemountete SD
├── firmware/             — Gebaute Binaries (Output von build.sh)
│   ├── minui.elf
│   ├── minarch.elf
│   ├── keymon.elf
│   ├── say.elf
│   ├── syncsettings.elf
│   ├── clock.elf
│   ├── minput.elf
│   ├── DinguxCommander   — Von dArkOS-Image extrahiert (SDL2, 640x480)
│   ├── libmsettings.so
│   └── libSDL_ttf-2.0.so.0
├── platform/             — RG353M platform.c/h für MinUI
├── keymon/               — keymon.c (Power/Volume Handler)
├── libmsettings/         — msettings.c (Volume/Brightness)
├── minui/                — MinUI-Extras (aus MinUI-Extras-Release)
│   ├── MinUI.zip         — MinUI-Base-Release
│   ├── Emus/rgb30/       — Extras-Emulatoren (MGBA, PKM, SGB, …)
│   └── Tools/rgb30/      — Clock.pak, Input.pak, Files.pak
└── dArkOS_RG353m_trixie_05112026.img  — Original dArkOS-Image (Backup)
```

## Install-Prozess

1. dArkOS auf SD flashen (z.B. mit Balena Etcher)
2. SD einmal im Gerät booten (expandtoexfat.sh expandiert EASYROMS)
3. SD in PC einlegen
4. `./build.sh` — baut alle Binaries
5. `./install.sh [EASYROMS] [ROOTFS]` — deployt MinUI auf die SD

Das Script ist idempotent — kann beliebig oft ausgeführt werden.

## MinUI auf EASYROMS (/roms)

```
/roms/
├── .system/rg353m/
│   ├── bin/          — minui.elf, minarch.elf, keymon.elf, say.elf, syncsettings.elf
│   ├── lib/          — libmsettings.so, libSDL_ttf-2.0.so.0
│   ├── res/          — Fonts, Themes
│   └── paks/
│       ├── MinUI.pak/launch.sh   — Haupt-Loop
│       ├── Emus/                 — Stock PAKs aus MinUI.zip
│       └── System/               — MinUI Stock-System-PAKs
├── .tmp_update/
│   └── launch.sh / updater      — Boot-Hook für emulationstation.sh
├── .userdata/shared/
│   ├── logs/         — minui.txt, keymon.txt, etc.
│   └── datetime.txt  — Letzte Systemzeit (kein Hardware-RTC)
├── Tools/rg353m/
│   ├── Clock.pak     — Uhrzeit einstellen (selbst gebaut)
│   ├── Input.pak     — Input-Tester (selbst gebaut)
│   └── Files.pak     — DinguxCommander (von dArkOS, SDL2)
├── Roms/             — ROM-Verzeichnisse pro System
├── Bios/             — BIOS-Dateien
└── Saves/            — Savegames
```

## MinUI-Loop (MinUI.pak/launch.sh)

```
Boot → datetime.txt wiederherstellen → keymon.elf starten
→ auto.sh? → Spiel direkt laden (Quicksave-Resume)
→ while-loop:
    minui.elf starten
    /tmp/poweroff? → auto.sh schreiben → poweroff
    /tmp/next?    → PAK_SCRIPT + PAK_ROM parsen
                  → sleep 0.5 → PAK starten (&)
                  → wait PID → pidof minarch.elf warten
                  → sleep 0.3 → zurück zu minui.elf
```

### Besonderheit: minarch.elf forkt sich

minarch.elf forkt sich ~1s nach dem Start — der Parent-Prozess exitiert,
der Child-Prozess läuft weiter. Daher reicht `wait $PID` nicht aus;
es wird zusätzlich `while pidof minarch.elf` gewartet.

## Bekannte Einschränkungen

| Problem | Status |
|---------|--------|
| Kein Hardware-RTC | Zeit friert beim Ausschalten ein; Clock-Tool zum manuellen Setzen |
| DinguxCommander Seitenverhältnis | Leichtes Letterboxing (480×480 UI in 640×480) |
| Kein WLAN-Tool | dArkOS `wifictl`/`nmcli` nicht im MinUI-PATH verfügbar |
| Splore (Pico-8) | Nicht installiert (braucht separates Pico-8-Binary) |

## Unterschiede zum GKD Pixel 2 (MinUI-Referenzport)

| | GKD Pixel 2 | RG353M |
|--|-------------|--------|
| SoC | RK3326S (Cortex-A35) | RK3566 (Cortex-A55) |
| OS-Basis | ROCKNIX (SquashFS) | dArkOS (Debian, btrfs) |
| Compositor | Sway (Wayland) | Keiner (KMS/DRM direkt) |
| SDL-Mode | `wayland` | `kmsdrm` |
| SDL-Window | FULLSCREEN | **SHOWN** |
| Gamepad-Event | event2 | event4 |
| Menu-Button | 704 BTN_TRIGGER_HAPPY1 | 316 BTN_MODE |
| Compiler-Flag | `-mtune=cortex-a35` | `-mtune=cortex-a55` |
| Boot-Hook | launchersway.service | emulationstation.sh |
| Root-FS | SquashFS (readonly) | btrfs (read-write) |
| SDCARD_PATH | `/mnt/SDCARD` | `/roms` |
