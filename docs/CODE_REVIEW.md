# Core firmware review

Review scope: the code present in this workspace, including the engine submodule,
firmware integration, preset tables, persistence and editor/emulator paths. Local
changes on the owner's computer are not included. The firmware is a candidate for
FM-1 testing; hardware audio deadlines and analogue output have not been measured.

## Core3 WAVE extension

Core3 appends PPG-, ESQ- and Prophet VS-inspired banks: 12 original additive
waveforms, no additional factory presets. WAVE now has 20 waves / 80 harmonic
mips / 40 KiB of tables. The original eight waves retain their exact sample bytes
and bank IDs; all core2 save signatures and parameter meanings remain.

Focused validation covers regenerated table consistency, unchanged old-table
checksums, 20 distinct spectra, out-of-band energy below -80 dB for every mip,
all bank corners with scans and pitch/warp extremes, high-note folded harmonics,
editor bank selection, simulated-flash round trips and full mix health for every
bank corner. The emulator's heavy scene automates through the three new banks for
native/WASM comparison. Existing core tests and target budgets pass. The app is 394,276 bytes, with
187,288 bytes free in its slot. Ordinary RAM remains 52,280 bytes and the pool
328,896 bytes, unchanged from core2. All 106 mix runs have zero PCM rail hits.

The original core2 review record follows; its broader fuzz/UI measurements are
historical checks of that base, not claims that the entire review was repeated for
this table extension. Current build details are in the package's build report.

## Corrections

- Reduced the core factory library to exactly three presets per engine (33 total).
  FM6's packed factory voices are also reduced to three; OWN/bank slots, startup
  patches and categories use the new indices. Reference-profile presets remain.
- Preserved fine tuning, pitch-bend residuals and unison detune when chip engines
  recalculate pitch. Preserved filter state on sounding retriggers while resetting
  chip envelopes. Retained the earlier decay-divider and PSG envelope corrections.
- Replaced abrupt WAVE harmonic-table selection with crossfades within the safe
  bandwidth, and tapered high-pitch phase warp. No extra voice storage or buffers.
  The 7,040 Hz diagnostic improved from about -39 dB off-harmonic energy at maximum
  warp to about -87 dB; this is a test case, not a universal aliasing bound.
- Rejected reserved engine IDs in editor preset loads and user-preset input, and
  excluded legacy DIGITAL conversion paths from the fresh core profile. Bumped
  the profile's storage namespace to core2 because factory indices changed.
- Added compact STRG/MODL tags to fix icon/name overlap in EDIT and PRESETS.
- Made signed phase wrapping explicit with `-fwrapv` in core target/emulator builds.
  Hoisted block-constant performance-filter indices to preserve the original loop
  budget with wrapping enabled. Fixed a missing `<stdint.h>` in the existing preemption stress harness and kept
  standalone settings/parser builds compatible with the new profile switch.

## Evidence

- `sh tests/run_core_tests.sh`: all 33 presets render and release, distinct chip
  signals, descriptor ranges, engine/workspace switches, retriggers, storage
  signatures and reserved-ID rejection. Actual editor factory loads and user
  preset values round-trip. Chip tests cover clock dividers, noise periods, all
  parameter extremes, fine tuning, cyclic interpolation and folded-harmonic checks.
- Simulated NOR: all 33 presets survive user-bank reboot and project load, including
  FM6 patch data. FM6 bank transfer checks include power cuts after every programmed
  byte, interrupted erases and bank replacement while saved patches retain voices.
- Full mix health: 66 fresh processes, one-note and eight-note input for each preset,
  real FX/master path, finite output and zero PCM rail hits; largest peak about
  0.391 FS. Existing voice caps and mono modes apply.
- Two-core host comparison: 14 scenes, every split, four simultaneous parts with
  inserts, spread, slicer and modulation. All outputs match one-core rendering.
  This verifies state isolation, not FM-1 execution time.
- AddressSanitizer/UBSan: core integration, chip/WAVE DSP, editor and bank tests;
  additional malformed editor/USB packets (30,000 iterations), mutated projects
  (6,000), and preempted drum-grid editing during normal and song playback.
  Core sanitizer builds use `-fwrapv` and disable shift checks for existing DSP;
  standalone chip tests retain signed-overflow checks without `-fwrapv`.
- Native versus WebAssembly audio: 88,200 frames match bit for bit. Emulator boot,
  keys, MIDI, LEDs, playback and a saved preset across instances pass. Chromium
  HOME scopes were inspected for all eleven engines, with no page errors.
- Core screen sweep: 233 screen scenarios in ten palettes, all page/parameter
  sweeps, both styles and LARGE. Zero layout findings; 3,066,740 alignment
  measurements with none off by one pixel or more. Free-text ellipses are allowed.
- Reference engine regression: all 108 standalone golden renders unchanged.
  Broader firmware regression includes 98 golden renders and 720 engine-memory
  switching sequences unchanged, plus existing storage, MIDI, sequencer, UI and
  synthesis tests. Optional DaisySP comparison and ThreadSanitizer were not run.
- Target build checks retain flash/RAM reserves, RAM-only flash routines, core-1
  code restrictions, register-access audit and static DSP loop budgets. WAVE's new
  budget accounts for four table lookups instead of two; shared limits stay intact.

GCC 14.2 at `-O1` incorrectly reuses the existing custom-palette menu count across
a palette store. The test passes with `-fno-ipa-pure-const` and at `-O2`. The broader
host suite uses `CC='cc -fno-ipa-pure-const'`; firmware and WASM use Clang. Review
logs and measurements are under the ignored `build/` directory.

## Hardware boundary

The build report beside the candidate package records exact image/RAM sizes and
hashes. Static budgets and host performance cannot establish FM-1 interrupt/audio
headroom. Initial hardware tests should cover dense four-part playback, long FX
tails, rapid preset/engine switching, tuning/bend, repeated save/load and USB audio
with the second core both off and on. No device has been flashed here.

DRUM still uses the existing synthesis styles; the proposed 808/909 redesign and
707/DMX PCM kits are not implemented. SAMPLE and other optional engines remain
outside this core profile. No additional sample assets or audio-path allocation
were introduced.
