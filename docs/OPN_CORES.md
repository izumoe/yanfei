# OPN cores and provenance

Core4 uses chip code from [OPN-1](https://github.com/putchdev/opn-1), pinned to
`3319da44abc76fd0ce208de7c37405feef5770fd` (main at integration time).

- `firmware/src/opn2/opn2.c`, `opn2.h`: integer C port of Aaron Giles' ymfm YM2612 path, BSD-3-Clause. The complete notice is retained in the source.
- `firmware/engines/src/segapsg/psg.c`, `psg.h`: port of MAME's Sega VDP PSG by Nicola Salmoria and contributors, BSD-3-Clause. The complete notice is retained in the source.
- `tests/opn2/opn2_compare.cpp` and `firmware/engines/tests/segapsg/psg_compare.cpp`: OPN-1 comparison tests, GPL-3.0-only.
- Test-only ymfm reference: upstream `81aec25`, BSD-3-Clause; Nuked-OPN2 `335747d`, LGPL-2.1-or-later. Their licenses are in `tests/opn2/ref/`. Neither reference is compiled into firmware.
- Test-only MAME PSG transcription retains its BSD-3-Clause notice.

Our YM core adaptation prefixes private names, permits a one-channel instance,
uses a read-only expanded sine table and moves block scratch into each instance.
The default six-channel build is checked sample for sample against ymfm, using
the same register sequences, algorithms, LFO, SSG, channel-3, DAC and random writes
as OPN-1's suite. Nuked-OPN2 differences are reported rather than hidden.
A further 390,278 random-write samples compare the one-channel build against
the six-channel build: identical operator state and output after accounting for
the five unused channels' fixed ladder DAC offset.

The instrument uses six independent single-channel instances per part, within
the existing shared engine workspace. This avoids calculating five unused
channels for every voice and permits two CPUs to render different parts without
shared writable scratch. The native 53,267 Hz signal is linearly resampled to
44,100 Hz and DC filtered. The instrument exposes eight algorithms, feedback,
per-operator MUL/DT/AR/D1R/D1L/RR/TL, shared D2R/KS/SSG, LFO/PMS/AMS and an AM
operator mask. LEVEL sets instrument gain; the chip's ladder DAC is always used.
Timers/CSM, PCM streaming and channel-3 special mode are not instrument features.
Full register emulation tests do not imply bit-identical console output after
resampling, filtering, instrument controls and Felucca's mixer/effects.

PSG retains its existing AY-inspired tone/noise/buzzer path. CHIP=SEGA selects
the MAME-derived core: three 10-bit tone dividers, sixteen volume/mute steps,
a 16-bit LFSR, white/periodic noise and fixed/tone-linked noise clocks. Two tone
channels are audible; the third clocks linked noise. TONE2, INT, MIX and TONE
remain instrument controls. Sega VOL replaces AY B-RATE, and NOISE replaces BUZZ.
N-DIV affects the linked tone divider only, with the real 10-bit limit. No AY
buzzer envelope is applied in Sega mode. Its state is per voice and its 40-run
comparison checks eight million chip ticks against the MAME reference.

Run `sh tests/opn2/run.sh`, `sh firmware/engines/tests/segapsg/run.sh` and
`sh tests/run_core_tests.sh`. Reference matching, target size/loop checks and
host concurrency checks are useful evidence; device audio deadlines still need
measurement on an FM-1 running a multi-engine scene.

Core5 changes the HOME controls only: AY keeps TONE2/N-MIX/BUZZ/B-RATE,
while Sega uses TONE2/N-MIX/NOISE/TONE. Its stepped volume remains in EDIT.
The shared engine interface has an optional `home_knob` callback so the host
uses the same mode-dependent map for display, turns, MIDI learn and step locks.
PSG's noise and hold names fit the five-character value display. Preset DSP,
serialized parameter IDs/ranges and chip-core code remain unchanged from core4.

Core6 appends five YM sounds with independent operator settings and five PSG
sounds with explicit chip selections. Existing factory entries 0..2 are unchanged.
The operator table uses signed bytes (all values, including negative detune, fit)
to keep the additional patch data small. See INSPIRED_PRESETS.md for the catalog.
