# Yanfei

Yanfei is custom firmware for the M-VAVE FM-1. It is based on
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita and Hügelton Instruments.
The fork source is [izumoe/yanfei](https://github.com/izumoe/yanfei).

Yanfei has twelve engines, 96 factory presets, and a four-track tracker.
The tracker starts in ALL. Press SHIFT + UP to open detailed PHRASE.
Scroll through sections of up to four bars. Use shared phrases and chains to arrange music.

## Use Yanfei

[Open the website](https://izumoe.github.io/yanfei/) for the editor, emulator, installer, and guide.

- [User guide](docs/USER_GUIDE.md)
- [Tracker quick reference](docs/TRACKER_QUICK_REFERENCE.md)
- [Engines and firmware budgets](docs/CORE_ENGINES.md)
- [Inspired presets](docs/INSPIRED_PRESETS.md)
- [Tracker implementation](docs/TRACKER.md)

## Build

Follow [BUILDING.md](BUILDING.md) to install the toolchain and SDK.
Then run:

```sh
./build-yanfei.sh
```

The installable package is `build/yanfei.fwsc`. The app image is `build/yanfei.bin`.
The existing `./build-core.sh` command also builds Yanfei.
The package retains the FM-1 update identity required by the installer.

For the browser emulator, activate Emscripten, then run:

```sh
FELUCCA_CORE=1 sh web/emu/build.sh
python3 -m http.server 8000 --directory build/emu
```

Open `http://localhost:8000` in Chrome or Edge.
The internal build flag keeps its existing name for compatibility.

## Upstream and licences

Yanfei is GPL-3.0-only. Read [LICENSE](LICENSE), [LICENSING.md](LICENSING.md),
and the component notices in [LICENSES](LICENSES/).
The engines in `firmware/engines` come from
[Felucca-Engines](https://github.com/hugelton/Felucca-Engines), under MIT.
Upstream copyright notices and credits are retained.
See [CREDITS.md](CREDITS.md) and the [upstream README](docs/UPSTREAM_README.md).

This fork was developed with AI coding assistance.
M-VAVE and FM-1 are trademarks of their respective owners.
Yanfei is not affiliated with or endorsed by M-VAVE.

Upstream copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments.
