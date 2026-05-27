# MinUI Port für den Anbernic RG353M

Inoffizieller Port von [MinUI](https://github.com/shauninman/MinUI) für den Anbernic RG353M,
basierend auf [dArkOS](https://github.com/christianhaitian/arkos) als Basis-Firmware.

## Voraussetzungen

- Anbernic RG353M
- SD-Karte (empfohlen: 32 GB oder größer)
- Linux-PC
- [dArkOS für RG353M](https://github.com/christianhaitian/arkos/wiki) — zum Flashen
- [MinUI-Extras-Release](https://github.com/shauninman/MinUI/releases) — manuell herunterladen (siehe unten)
- Docker (nur wenn selbst gebaut werden soll)

## Installation

### 1. dArkOS flashen und einmal booten

dArkOS auf die SD-Karte flashen (z.B. mit [Balena Etcher](https://etcher.balena.io/)).
SD in den RG353M einlegen und **einmal komplett booten** — dArkOS expandiert dabei
die EASYROMS-Partition auf die volle SD-Größe. Danach ausschalten und SD in den PC einlegen.

### 2. Dieses Repo klonen

```bash
git clone https://github.com/DEIN_USERNAME/minui-rg353m
cd minui-rg353m
```

### 3. MinUI-Extras herunterladen

MinUI selbst wird beim Build automatisch von GitHub geholt. Die **Extras** (zusätzliche
Emulatoren und Tools) müssen einmalig manuell heruntergeladen werden, da sie nicht
redistributiert werden dürfen.

Vom [MinUI-Extras-Release](https://github.com/shauninman/MinUI/releases) das
`MinUI-Extras.zip` herunterladen und entpacken:

```
minui/
├── Emus/
│   └── rgb30/        ← Inhalt aus Extras/Emus/rgb30/
└── Tools/
    └── rgb30/        ← Inhalt aus Extras/Tools/rgb30/
```

### 4. Binaries bauen

```bash
./build.sh
```

Klont MinUI automatisch und baut alle Binaries via Docker (rgb30-toolchain).
Ergebnis landet in `firmware/`.

### 5. Auf SD installieren

```bash
./install.sh /run/media/$USER/EASYROMS /run/media/$USER/ROOTFS
```

Das Script ist **idempotent** — es kann beliebig oft ausgeführt werden ohne
ROMs, BIOS-Dateien oder Savegames zu löschen.

### 6. Starten

SD in den RG353M einlegen und einschalten. MinUI startet automatisch.

---

## Enthaltene Emulatoren

### Stock-Emulatoren (aus MinUI, automatisch)
Game Boy, Game Boy Color, Game Boy Advance, NES, SNES, Sega Master System,
Game Gear, Neo Geo Pocket, und mehr.

### Extras-Emulatoren (aus `minui/Emus/rgb30/`)

| PAK | System |
|-----|--------|
| MGBA.pak | Game Boy Advance (mGBA-Core) |
| PKM.pak | Pokémon mini |
| SGB.pak | Super Game Boy |
| SMS.pak | Sega Master System |
| GG.pak | Sega Game Gear |
| SUPA.pak | Super Nintendo |
| PCE.pak | PC Engine |
| NGPC.pak | Neo Geo Pocket Color |
| NGP.pak | Neo Geo Pocket |
| VB.pak | Virtual Boy |

## Tools

| Tool | Funktion |
|------|----------|
| Clock | Systemzeit manuell einstellen |
| Input | Gamepad-Input-Tester |
| Files | Dateimanager (DinguxCommander) |

> **Hinweis zur Uhrzeit:** Der RG353M hat keinen batteriebetriebenen RTC.
> Die Uhrzeit friert beim Ausschalten ein. Mit dem Clock-Tool kann sie manuell gesetzt werden.

## Bekannte Einschränkungen

| Problem | Beschreibung |
|---------|-------------|
| Kein WLAN-Tool | dArkOS-Netzwerktools nicht im MinUI-Kontext verfügbar |
| Kein Hardware-RTC | Uhrzeit muss nach jedem Kaltstart neu gesetzt werden |

## Projektstruktur

```
├── build.sh          — Baut alle Binaries (Docker)
├── install.sh        — Deployt MinUI auf die SD
├── platform/         — RG353M-spezifischer MinUI-Platform-Layer
├── keymon/           — Power/Volume-Key-Monitor
├── libmsettings/     — Volume/Brightness-Settings-Library
├── firmware/         — Gebaute Binaries (Output von build.sh)
└── minui/            — MinUI-Extras (manuell befüllen, siehe oben)
```

## Credits

- [MinUI](https://github.com/shauninman/MinUI) von Shaun Inman
- [dArkOS](https://github.com/christianhaitian/arkos) von Christian Haitian
- [DinguxCommander](https://github.com/od-contrib/commander) von od-contrib
