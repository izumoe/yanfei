# Yanfei engine guide

Use this guide to choose an engine and learn its controls.
It covers the twelve engines in Yanfei v1.6.1-y13.
Each engine page includes pictures from the running firmware, HOME controls,
engine parameters, mode choices, factory preset names, and changes to try.

For envelopes, effects, scale, voice modes, and tracker operation, use the [mode guide](MODE_GUIDE.md).
For the physical controls, use the [visual guide](VISUAL_GUIDE.md).

## Choose an engine

| Engine | Start here when you want | Main sound method |
| --- | --- | --- |
| [ANALOG](ENGINE_ANALOG.md) | Bass, lead, pad, filter sweep | Oscillators into a resonant filter. |
| [NES](ENGINE_NES.md) | Console pulse lead or stepped triangle bass | Pulse, triangle, and clocked noise with chip decay. |
| [DMG](ENGINE_DMG.md) | Handheld wave, pulse bass, or short-register noise | Four-bit wave shapes, pulse, and noise. |
| [PSG](ENGINE_PSG.md) | AY buzzer or Sega square/noise | Two square tones, noise, and chip-specific controls. |
| [TRIO](ENGINE_TRIO.md) | SID-style stack, sync, or ring modulation | Three interacting oscillators and a filter. |
| [FM6](ENGINE_FM6.md) | DX-style electric piano, mallet, or digital pad | Six-operator FM with complete voice patches. |
| [YM](ENGINE_YM.md) | Genesis-style FM bass, brass, or arcade lead | Four-operator YM2612-style FM. |
| [WAVE](ENGINE_WAVE.md) | Moving digital pad or changing wave tone | Linear scanning through four waves per bank. |
| [PHASE](ENGINE_PHASE.md) | CZ-style bass, resonant tone, or glass | Two lines of phase-distortion synthesis. |
| [STRING](ENGINE_STRING.md) | Plucked, buzzing, or sympathetic string | Modelled string and optional sympathetic strings. |
| [MODAL](ENGINE_MODAL.md) | Bell, bar, plate, or tuned membrane | A strike into a bank of resonances. |
| [DRUM](ENGINE_DRUM.md) | A full kit on one track | Eight synthesized drum lanes. |

These are instrument designs and inspired sound families.
The names do not imply complete emulation of a vintage keyboard or console.
Use the notes on each engine page to identify its specific features and limits.

## Select and edit a sound

1. Stop playback.
2. Leave the tracker by pressing ENV, then press HOME.
3. Turn ALGORITHM to select a track.
4. Turn PRESETS to select a sound.
5. Play a note and change one HOME knob at a time.
6. Press the panel EDIT button to open the engine controls.

Pause between EDIT presses. A quick double press opens its shortcut layer.
The bottom-right page name helps identify the current page.
The first two engine page names differ by engine; for example, WAVE uses SCAN and TONE.
Later EDIT pages contain voice settings or engine-specific pages.

For another engine, choose one of its factory sounds with PRESETS.
You can also hold EDIT to open the engine selection layer.
Read the displayed key map before selecting an engine.
Save edits before loading another sound.

In the [web editor](WEB_EDITOR.md), select an engine in Sound, below the preset name.
The engine card contains its main pages. FM6 also has a separate 6-OP FM page.
Controls whose meanings change by model can have different labels on the device.

## Understand the two kinds of envelope

Most engines use the common ENV to control note amplitude.
NES and DMG add a separate stepped chip decay.
STRING and MODAL also have a model decay, so DAMP and ENV both affect the result.

FM6 and YM use their operator envelopes for amplitude and release.
Changing common ATK or REL is not the way to change their note duration.
DRUM uses the drum hit's own envelope; key release and common ADSR do not stop the hit.

If a sound stops too soon, check both its engine decay and its row gate or OFF instruction.
If it rings too long, check the same controls before adding effects.

## Voice limits

All four tracks share a total budget of eight active voices.
An engine can impose a lower per-track limit:

| Engine group | Per-track limit |
| --- | --- |
| STRING, MODAL | Three voices. |
| FM6, YM | Six voices. |
| Other melodic engines | Up to the shared voice budget, subject to voice mode. |
| DRUM | Eight lanes, subject to the same shared voice budget. |

A long release can still occupy a voice after a key is released.
Unison uses several voices for one played note.
A dense chord on one track can therefore take voices from another track.
Use shorter releases, fewer chord notes, or simpler arrangements if notes are cut off.

## Sound terms

| Term | Meaning |
| --- | --- |
| Oscillator | A source that repeats a wave to make a tone. |
| Harmonic | A tone component at a whole-number multiple of the played pitch. |
| Partial | One tone component; it does not have to be a harmonic. |
| Filter cutoff | The frequency around which a filter changes the signal. |
| Resonance | Emphasis around a filter cutoff, or ringing in a physical model. |
| Pulse width | The fraction of a pulse cycle spent at its high level. |
| Hard sync | Restart one oscillator whenever another begins a cycle. |
| Ring modulation | Multiply two signals to create extra sum and difference tones. |
| Wave table | Stored samples that describe a repeating wave. |
| Excitation | The strike, pluck, or repeated input that starts a model ringing. |
| Operator | An oscillator and envelope used in FM synthesis. |
| Attenuation | A reduction in level. A higher attenuation value is quieter. |
| Velocity | Note attack strength; it can change loudness, brightness, or both. |
| Polyphony | The number of voices that can sound together. |
| Gate | The time between a note starting and its release instruction. |

## Read the pictures

The engine pages show the actual y13 emulator screen.
Select a picture to open the full image.
The scope shows mixed output with level scaling; effects can change its shape.
It is not a direct drawing of an oscillator or an equal-loudness comparison.

The [inspired preset catalogue](INSPIRED_PRESETS.md) explains the newer patch references.
The [engine implementation record](CORE_ENGINES.md) contains technical budgets and validation details.
