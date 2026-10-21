# Building Yanfei

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `yanfei.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `yanfei.fwsc` | the installable package (app + loader) |

## Get the source

The engine library is in a git submodule (`firmware/engines/`, [Felucca-Engines](https://github.com/hugelton/Felucca-Engines),
MIT), so clone with `--recurse-submodules`:

```
git clone --recurse-submodules https://github.com/izumoe/yanfei.git
```

In a clone made without it, run `git submodule update --init` once.
GitHub source ZIP files omit submodule contents. The build requires the engine library.

## Prerequisites (macOS)

- Python 3 with Pillow and fontTools: `pip3 install Pillow fonttools` (the UI font and icons are
  rasterised at build time)
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).
- Emscripten (optional, `emcc`: the browser emulator, `web/emu/`).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build-yanfei.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build-yanfei.sh --release 1.0` makes a release build: the package identity becomes `FM-1_910`
and the version string `v1.0`; the package is `build/yanfei-1.0.fwsc`, and
`build/release-1.0/` holds what a release ships: the package, the app
(`yanfei-1.0-app.bin`), `SHA256SUMS`, the sample attribution, `LICENSE`, `LICENSING.md` and
`LICENSES/` (the package contains Apache-2.0 SDK files, so the licence texts travel with it).

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`, `core.h` and `icons.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_CDC_DEFAULT` | 1 | `0`: the console is built in but left out of USB from boot (as MENU > USB SERIAL OFF) |
| `FELUCCA_UAC` | 1 | USB audio input (the master output, stereo, 44.1 or 48 kHz) |
| `FELUCCA_UAC_48K` | 1 | `0`: the USB audio input at 44.1 kHz only, as up to 1.0.5.2 (`firmware/src/usb.c`) |
| `FELUCCA_UAC_AS_FIRST` | 1 | the audio control header lists the audio streaming interface before the MIDI one, so the USB audio driver of older macOS (up to 15) finds it; `0`: 1.0 to 1.5's order, byte for byte (`firmware/src/usb.c`) |
| `FELUCCA_CPU2` | 1 | the second core is built in (MENU > SYSTEM > 2ND CORE, OFF by default); `0`: none of it, 1.6's one-core code and RAM (`firmware/hal/fm1_cpu1.h`, `firmware/src/fx.c`) |
| `FELUCCA_UART` | 1 | TRS MIDI IN |
| `FELUCCA_SLICE` | 0 | SLICE is excluded from Yanfei |
| `FELUCCA_ICONS` | 1 | parameter icons on the knob cards |
| `FELUCCA_FM4` | 0 | the retired DIGITAL engine (4-operator FM) instead of its FM6 conversion |

`FELUCCA_USB_LAYOUT` (`0` to `3`, default `0`) picks other USB descriptor layouts for testing; see
`firmware/src/usb.c`.

## Samples

Yanfei includes the DRUM engine. SAMPLE and SLICE are excluded from this build.
The sample and drum sources keep their original licences in `LICENSING.md`.

## Local output and cleanup

`build/` is ignored by Git and is not source code. It also holds downloaded tools,
test reports, and firmware snapshots in this workspace. Keep the SDK and toolchain
when removing generated output. See [the cleanup notes](CONTRIBUTING.md#generated-files-and-local-cleanup)
for the UI capture archives and their restore command.

## Browser emulator

```
./build-yanfei.sh
FELUCCA_CORE=1 sh web/emu/build.sh
python3 -m http.server -d build/emu 8790     # open http://localhost:8790/
```

`web/emu/felucca_web.c` builds the firmware's sources as the host tests do, against a simulated FM-1, and
Emscripten compiles it to WebAssembly (`build/emu/felucca.wasm`, with `index.html`, `worklet.js` and the font in `fonts/`). The page
keeps the flash's storage sectors (projects, user presets, settings) in the browser. Not emulated: USB (audio,
MIDI out, the editor, updates), TRS MIDI and the user sample slots. Use the local URL above to run this Yanfei build.

## Tests

After the build, run:

```sh
sh tests/run_core_tests.sh
python3 tests/core_budget.py
node web/test_web.mjs
node web/test_backup.mjs
```

The core suite checks the twelve engines, factory presets, tracker, storage,
backup, voice limits and mixed audio. The budget check uses the target build.
The web tests check the editor, installer and backup code.

The upstream profile and its shared regression tests remain available through
`./build.sh` and `tests/run_tests.sh`. Both profiles share `build/`.
Rebuild Yanfei before you package or install it.

## Install

Use the local web installer below in Chrome or Edge.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/yanfei.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

```
python3 web/make_site.py build/yanfei.fwsc dev /tmp/yanfei-site
cd /tmp/yanfei-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Build the Pages site

Build the firmware and emulator first. Then run:

```sh
python3 -m pip install -r web/requirements-pages.txt
python3 web/make_pages.py build/yanfei.fwsc v1.6.1-y13 build/pages --emulator build/emu
python3 -m http.server 8000 --directory build/pages
```

Open `http://localhost:8000`. The site includes the editor, emulator, installer,
HTML user guide, firmware, licences, checksums, and a source archive.
The archive contains the current firmware and engine working files.

GitHub Pages serves the site from the `gh-pages` branch at its root.
Publish only the generated site to that branch. Normal source work stays on `main`.
The public URL is <https://izumoe.github.io/yanfei/>.
