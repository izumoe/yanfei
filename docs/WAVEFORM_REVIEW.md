# Core waveform review

The review used actual fixed-point engine renders and the running browser emulator,
not the earlier idle screenshots. Diagnostic captures, plots and measurement scripts
are in the local ignored `build/waveform-review/` directory.

Core3 appends three vintage-inspired banks while retaining the original eight
wave tables exactly. The extension's generator checks 20 distinct waves and all
80 harmonic mips; standalone renderer tests cover all five banks, four corners,
scan motion, low/high notes and warp extremes. The core3 full-mix run has 106
cases and zero PCM rail hits. Core2's detailed measurements below remain a record
of the base DSP; see CODE_REVIEW.md for the extension checks.

## Verified behavior

- Fifty dry single-voice captures cover all four pulse duties, the NES staircase,
  all sixteen DMG wave RAM shapes, short/long noise, every PSG envelope shape,
  chip decays and WAVE at low through very high notes.
- Basic A4 pulse, triangle and sine outputs measure 440 Hz. NES/DMG pulse duty
  settings yield the expected 12.5/25/50/75% shapes; the triangle uses a 32-position
  staircase and DMG preserves its 32-position, 4-bit wave character.
- The eleven engine UIs were inspected while sounding. FM6 EDIT intentionally
  displays its operator diagram, so HOME is used to view its output waveform.
- All 33 factory presets were exercised with one-note and eight-note input through
  the actual mix, FX, master and audio simulation, 66 independent processes in
  total. Existing voice caps and mono modes still apply. All outputs were finite;
  no samples reached the PCM rails. Largest observed peak was approximately 0.391
  full scale. This is not a claim of zero distortion: the existing soft saturation
  and limiter intentionally affect the output.

## Corrections made

The first NES/DMG decay step previously ignored the selected frame-clock divider:
it happened after one clock frame, even for a slow decay. Initialization now loads
the divider as well as resetting the volume. For D7 the first step waits about
33.3 ms on NES and 125 ms on DMG, matching the following steps. Regression tests
check both the full initial plateau and the first decrement.

PSG's reverse triangle envelope previously missed one level at its peak. It is
now the exact 15-minus-triangle complement over the sixteen envelope positions.
A regression test checks this relation for the complete cycle.

## Interpreting the scope and remaining limits

The on-device scope shows the left channel of the output after effects and MASTER
in OUT mode (MIX is an alternative). It auto-scales amplitude, displays every
other audio sample and triggers on a rising zero crossing. Therefore its waveform
can differ from the dry oscillator, equal visual heights do not imply equal
loudness, and very high-frequency traces can alias in the display itself. Spectral
checks use the full-rate samples instead.

Audio-rate PSG envelopes can introduce DC into the dry modulated waveform; the
existing output DC blocker removes it before the final saturation stage. Asymmetric
waves can acquire a small mean offset again through that nonlinear stage: the
NES pulse preset measured roughly -0.0038 full scale on the left, and the upstream
LOFI pulse preset similarly measured -0.0035. This is existing master-stage behavior,
not evidence of an incorrect new pulse duty. No extra filter or per-voice state was
added merely to compensate for it in the engines.

WAVE now crossfades harmonic limits before their bandwidth reaches Nyquist and
reduces effective warp with increasing pitch. In the same 7,040 Hz test, measured
off-harmonic energy is approximately -87 dB at WARP 0 and -87 dB at maximum WARP,
compared with -39 dB at maximum warp in the first implementation. This is a result
for this waveform and pitch, not a global aliasing bound. A regression checks
folded harmonics 4..12 below -60 dB in this case. High-pitch warp intentionally
becomes milder; there is no oversampling buffer or additional voice storage.

Chip pitch recalculation now retains the host's fine-tuning, pitch-bend residual
and unison detune. NES, DMG, PSG and WAVE retain their filter state on sounding
retriggers while restarting the chip envelopes. Integration and standalone tests
exercise both behaviors.

Core integration tests, standalone chip tests, sanitizer checks, target loop-cost
budgets and the native-versus-WASM audio comparison are rerun for these corrections.
Real FM-1 timing and analogue outputs still require hardware; these captures also
do not replace subjective listening.
