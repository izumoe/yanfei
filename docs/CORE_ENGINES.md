# Yanfei engines and firmware budgets

For sound controls and examples, read the [engine guide](ENGINE_GUIDE.md).
This document records implementation details and firmware budgets.

See [the waveform review](WAVEFORM_REVIEW.md) for live scopes, signal measurements,
the corrected decay/envelope details and the high-note WARP limitation.

This is the first implementation of the twelve-engine profile. `./build-yanfei.sh`
builds it; `./build.sh` retains the upstream profile for reference. Both write into
`build/`, so rebuild the desired profile before packaging or emulator comparisons.
Yanfei identifies itself as `v1.6.1-y13`. Engine additions are
paused for the [tracker sequencer work](TRACKER.md); core8 adds reusable phrases, chains, song sections and row instruments.
The engines and 96 presets are retained; tracker projects use a larger journal.
Y13 adds scrolling over four section pages, a four-bar limit, and repeat/wait per track.
Core11 makes ALL the default tracker view after a restart.
Core10 adds an editable four-track ALL view. SHIFT + UP switches to PHRASE.
Core9 adds the selected row's four DETAIL values to the PHRASE footer.

| Engine | Eight factory presets |
| --- | --- |
| ANALOG | SQR BASS, SAW LEAD, SOFT PAD, JUNO CHORUS, JUPITER SYNC, 303 SQUELCH, MINI ROUND, MS REED |
| NES | PULSE LD, WAVE BASS, NOISE HIT, FAMI HERO, FAMI LOW, FAMI ARP, 8BIT SNARE, DUNGEON PAD |
| DMG | HANDHELD, WAVE LEAD, SHORT HIT, POCKET BASS, POCKET BELL, POCKET WIND, 7BIT TOM, LINK LEAD |
| PSG | BUZZ BASS, PSG CHORD, SEGA HIT, AY BUZZ LEAD, AY ZAP, SEGA SQUARE, SEGA CHORD, SEGA HAT |
| TRIO | FAT BASS, SYNC LEAD, RING BELL, SID PWM, SID RUBBER, SID SYNC, SID METAL, SID CHOIR |
| FM6 | TINE EP, FM BASS, PAD, TX RUBBER, DX GLASS, DX MALLET, DX BRASS, DX ORGAN |
| YM | MD BASS, FM BRASS, METAL BELL, MD SLAP, MD REED, ARCADE LEAD, MD EPIANO, MD CHIME |
| WAVE | MORPH PAD, GLASS WALK, WAVE SUB, PPG CHOIR, ESQ BRASS, VS AIR, PPG METAL, ESQ REED |
| PHASE | BRASS, RESO, BELL, CZ BASS, CZ STRINGS, CZ GLASS, CZ ORGAN, CZ WIRE |
| STRING | PLUCK, DRONE STRING, HARP, KOTO PICK, SITAR BUZZ, ZITHER, CLAVI PLUCK, BOWED WIRE |
| MODAL | BELL TREE, MARIMBA, HAND DRUM, VIBE KEYS, TOY BELL, GAMELAN, STEEL TINE, SYNTH TOM |
| DRUM | DRUM KIT, DEEP KIT, TIGHT KIT, ELECTRO KIT, HOUSE KIT, ARCADE KIT, DUB KIT, MINIMAL KIT |

There are 96 factory presets. The first three per engine keep their indices and
sound; five new inspired sounds are appended to each. See [the inspired preset
catalog](INSPIRED_PRESETS.md) for references, compatibility details and audition
clips. The reference profile retains its original presets.

FM6 retains three compatible SLOT entries; its five new full operator patches
load into OWN. OWN remains 3 and B1..B32 remain 4..35, so stored sounds do not
change meaning. The new FM6 patch travels with projects and user presets.
Existing user files are not rewritten or converted into unrelated engines.

## YM and PSG

YM is separate from FM6 and has six voices per part. Its eight presets
use the same saved format as other core sounds, including all operator controls.
EDIT cycles through two main pages, four operator envelope pages, operator levels, four
operator tone pages and AM controls. AR/D1R/D1L/RR/TL and MUL/DT are per operator;
D2R, SSG and KS are shared. The chip ladder DAC behavior is built in. It does not
stream game music, play PCM drums or expose channel-3 mode or CSM timers.

PSG's third EDIT page, CHIP, selects AY or SEGA with knob 1. Pause more than
300 ms between EDIT taps to avoid opening its double-tap shortcut layer. AY preserves the existing buzzer path. SEGA
uses a MAME-derived PSG core, with BUZZ becoming the white/periodic noise mode
and B-RATE becoming stepped VOL. The third factory preset demonstrates Sega noise.
Existing saved PSG sounds stay in AY mode. The core code retains its BSD license
notices; test-only reference cores are not part of the firmware.

## PSG HOME controls (core5)

| Chip | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
| --- | --- | --- | --- | --- |
| AY | TONE2 | N-MIX | BUZZ | B-RATE |
| SEGA | TONE2 | N-MIX | NOISE | TONE |

TONE2 blends the second square tone; N-MIX moves between tones and noise.
AY's BUZZ and B-RATE set its buzzer envelope shape and clock rate; B-RATE
has no effect at FLAT. Sega's NOISE picks white (W) or periodic (P) noise at
HI/MID/LO clock rates, or LNK for the tone-linked clock. TONE sets brightness.
Sega's stepped VOL remains on EDIT > COLOR. N-DIV affects Sega noise only
in a LNK mode. The neutral COLOR page title applies to both chips.

The mode-dependent HOME map also drives MIDI learn, momentary edits and step
locks. Existing sounds, parameter IDs, ranges and preset audio are unchanged;
this is a control/layout update. HOME > LEVELS still controls the four part levels.

## WAVE banks

BANK selects four related waves; POS morphs along them. RATE and SCAN move the
position automatically. The five banks are:

| Bank | Wave corners | Character |
| --- | --- | --- |
| BASIC | Sine, triangle, square, saw | Familiar oscillator shapes |
| COLOR | Hollow, glass, octaves, bright glass | General digital colors |
| PPG | Nasal, comb, vocal, metal | Upper harmonics and formant-like peaks |
| ESQ | Bass, reed, keys, hollow reed | Hybrid digital synth-style tones |
| VS | Airy, glass, brass, shimmer | Vector synth-style pad colors |

These are original harmonic recipes inspired by vintage timbral families. They
use linear scanning through four corners; VS does not add two-dimensional vector
mixing. All four harmonic limits are generated offline, including the new waves.
Core3 adds 24 KiB of tables over core2, with no new RAM buffers or sample-loop work.
The original eight tables are byte-identical and keep bank IDs 0/1. Core2 project,
preset and settings formats are retained; existing saved sounds stay compatible.
Core6 adds factory presets using the same five WAVE banks; no tables are added.

SAMPLE remains a possible later addition, GRAIN second priority, FORMANT/WHEEL
optional, NOISE/SLICE omitted. The upstream implementations remain in source as
references, but are not selectable in the core profile. Link-time elimination and
an empty core sample header remove unused engine descriptors/render paths and the
instrument PCM bank from the target image. Shared noise primitives remain.

## Saved data

The profile deliberately uses a fresh namespace: projects `0x59434632`, user-bank
magic `0x32435055`, preset versions 8/9, settings `0x59435032`. Upstream projects,
settings and user banks are refused rather than interpreted using the new IDs.
The internal slots are ANALOG=0, PHASE=2, NES=3, DMG=4, PSG=5, TRIO=6,
WAVE=7, STRING=8, MODAL=9, DRUM=10, FM6=12, YM=13. Slots 1 and 11 are unavailable;
the visible order is the twelve above. Keeping several host IDs avoids unrelated
changes in FM6 and DRUM code. The editor receives names and descriptors from the
firmware. The web editor refreshes engine-specific common descriptors and preserves stable
parameter keys across engines. Core4 keeps core2/core3 saved data compatible;
existing PSG sounds retain CHIP=AY. See [OPN core integration](OPN_CORES.md) for
YM and Sega PSG controls, upstream provenance, licenses and reference tests.

## Budget and validation

The upstream baseline app is 511,472 bytes, ordinary RAM 60,984/73,728, pool
328,896/368,640. The core8 app is 428,864 bytes (152,700 bytes free in its app slot),
with ordinary RAM 66,248/73,728 bytes and pool use 345,048/368,640 bytes. Its
13,648-byte active tracker library and fixed project staging buffer fit the
existing guards; see [tracker storage and validation](TRACKER.md).
The WAVE tables total exactly 40,960 bytes. YM, PSG and STRING/MODAL share the existing
12,888-byte per-part workspace; no voice struct or pool expansion was needed.
Preserve the build guards: 4 KiB ordinary RAM and 8 KiB pool free.

The core DSP uses fixed point with no audio-path allocation. Table interpolation
and adjacent-wave interpolation are separate. Harmonic limits crossfade before
their partials reach Nyquist; selection and
brightness run at block rate. Phase warp tapers at high pitches. This reduces
aliasing without oversampling or extra RAM; it is not an alias-free guarantee.
NES/DMG/AY are inspired instruments. YM and Sega PSG use tested chip cores,
with instrument controls, rate conversion and the common mixer/effects around them. DMG's DUTY descriptor uses four groups of four values
within the same 0..15 parameter range as WAV# so switching modes keeps a valid
stored value and editor range.

```sh
export JIELI_TOOLCHAIN=/workspace/yanfei/build/jieli/toolchain
export AC79_SDK=/workspace/yanfei/build/ac79-sdk
./build-core.sh
sh tests/run_core_tests.sh
python3 tests/core_budget.py
sh firmware/engines/tests/run.sh   # existing 108 golden renders remain unchanged

source /workspace/shared/emsdk/emsdk_env.sh
FELUCCA_CORE=1 sh web/emu/build.sh
cc -DFELUCCA_CORE=1 -fwrapv -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src \
  -o build/emu/core_native web/emu/native_check.c -lm
build/emu/core_native build/emu/core_native.f32
node web/emu/emu_test.mjs build/emu/felucca.wasm build/emu/core_native.f32
```

The core test renders every factory preset, checks new-engine releases, checks
foreign-data rejection and current-data round trips, exercises concurrent parts
and engine workspace switching. The standalone chip test checks noise periods,
distinct envelope clocks and 128 parameter-extreme/low/high-note renders.
`tests/core_budget.py` retains upstream cost limits for shared DSP and uses
`tests/core_target_budget.txt` for new functions. These initial cost baselines and
second-core split estimates are not hardware timing claims.

The core builds explicitly use `-fwrapv`: existing fixed-point FM6 phase
accumulators rely on signed wrapping. Sanitizer runs retain overflow checks with
that defined behavior, disabling only shift checks for existing fixed-point DSP.
The standalone chip/WAVE tests also pass without `-fwrapv`.

GCC 14.2 at `-O1` miscompiles the existing dynamic custom-palette count: disassembly
reuses a previous `menu_n` result across `cpal_store`. The same test passes at
`-O2` and with `-fno-ipa-pure-const`; the broader host review uses the latter.
This workaround is for the host compiler, not a firmware source change. The target
and emulator use Clang. See [the review record](CODE_REVIEW.md) for checks and limits.

## Next stages

DRUM still has its existing synthesis styles. The proposed 808 redesign, 909
hybrid and 707/DMX PCM kits are not implemented or renamed into existence here.
They need sound-design work, sample provenance and separate asset budgets.
Sega PSG mode is included; WAV import remains deferred. Hardware testing is
still required for audio deadlines, latency, USB behavior and the sound of dense
parts with FX and long releases. This environment has not flashed a device.
