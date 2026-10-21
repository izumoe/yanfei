# Contributing to Yanfei

Yanfei is an FM-1 firmware fork of Felucca. Report Yanfei bugs and changes in
[izumoe/yanfei](https://github.com/izumoe/yanfei).
Include the firmware version, steps to reproduce, and expected and actual results.
For editor problems, include the browser and operating system.
State whether you tested on hardware, in the emulator, or with host tests.

## Repository layout

| Location | Contents |
| --- | --- |
| `firmware/src/` | Firmware, engine integration, UI, tracker, storage, and USB. |
| `firmware/hal/` | FM-1 hardware support. |
| `firmware/engines/` | Engine library submodule, with its own Git history and MIT licence. |
| `web/` | Editor, installer, Pages generator, and browser checks. |
| `web/emu/` | Firmware emulator and browser runtime. |
| `tests/` | Host firmware tests, fixtures, audio references, and budget checks. |
| `tools/` | Build tools, generators, installer utilities, and documentation captures. |
| `docs/` | User guides, pictures, and technical review records. |
| `assets/` | Source assets and preset data. |
| `build/` | Ignored local output, tool downloads, and validation reports. |

The firmware uses C source inclusion in several places. Check include dependencies
before moving files or turning an included file into a separate translation unit.

## Make and check changes

Keep changes focused. Preserve saved parameter IDs and project compatibility unless
an intentional format change is part of the task. Keep component licence notices.
New source files must carry the appropriate SPDX header.

Follow [BUILDING.md](BUILDING.md) for the toolchain and generated tables.
Build Yanfei with `./build-yanfei.sh`; `./build-core.sh` is a compatibility alias.
The upstream reference profile remains available through `./build.sh`.
Both profiles write to `build/`, so rebuild the intended profile before packaging.

For firmware changes, run the relevant host tests. The full core checks are:

```sh
sh tests/run_core_tests.sh
python3 tests/core_budget.py
```

For editor, installer, or backup changes:

```sh
node web/test_web.mjs
node web/test_backup.mjs
```

The editor's presentation is in `web/editor.css`. Its HTML, device protocol, mock,
and application code are in `web/editor.html`. Keep protocol changes separate from
presentation changes. Browser checks are in `web/test_editor_ux.py`.
Each browser-check script describes its required URL and output directory.

For a generated Pages site:

```sh
python3 web/test_pages.py build/pages
```

This checks local links, document anchors, source archives, and checksums.
The visual guide and engine guide browser checks also verify pictures and phone layouts.
Host and emulator checks do not replace physical FM-1 testing for hardware behavior.
Record what you actually tested.

## Generated files and local cleanup

`build/` is ignored. Do not force-add it to the source branch.
It can contain the SDK and toolchain as well as firmware packages, test binaries,
raw renders, screenshots, and source archives. Check its contents before deleting it.
Do not remove `tests/fixtures/`, reference data, or documentation pictures as build output.

Large raw UI captures can be archived as `build/ui_*/ppm.tar.gz`.
The cleanup pass verifies each archived file before removing its uncompressed copy.
Test render commands regenerate the `ppm/` directory when run again.
To restore an existing archive, for example:

```sh
tar -xzf build/ui_core6/ppm.tar.gz -C build/ui_core6
```

PNG previews, test reports, firmware snapshots, the SDK, and the toolchain stay in place.
Python caches and local firmware packages are also covered by `.gitignore`.

## Source and deployment

The source worktree and the engine submodule can both have uncommitted changes.
Review both with `git status` and `git -C firmware/engines status`.
A parent-repository commit does not include uncommitted submodule files.
When preparing commits, commit engine changes in that repository, then update the
parent's submodule reference to an accessible commit.

The engine history is published on the `yanfei-engines` branch of
`izumoe/yanfei`. The submodule URL uses that repository and pins an engine commit.
Its tree contains the engine library, not the firmware repository. Publish the
engine branch before pushing a parent commit that refers to it.
The original upstream remains [hugelton/Felucca-Engines](https://github.com/hugelton/Felucca-Engines).

The generated website is published separately on `gh-pages`.
That branch intentionally contains the site, firmware download, and source archive.
A Pages deployment does not commit the working source changes to the source branch.
Do not replace source files with the generated site.

Yanfei firmware is GPL-3.0-only. The engine library and third-party components retain
their own licences; see [LICENSING.md](LICENSING.md).
