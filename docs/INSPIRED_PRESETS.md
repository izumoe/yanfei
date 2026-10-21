# Core6 inspired factory presets

Core6 adds 60 original patches to the default set: eight presets per engine, 96 total.
The existing three per-engine preset indices and sounds are retained; the new sounds occupy
positions 4–8 on each engine. No user-bank import is needed after installing core6.

The references describe a timbral direction. These are original parameter/operator recipes,
using the engines already present, with no factory ROM patches, samples or game melodies.
The acoustic STRING/MODAL additions draw on instrument-like sounds used in early digital
and physical-modelling synths. Drum presets tune the existing synthesis models.

## New sounds

### ANALOG

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | JUNO CHORUS | Juno-style PWM and chorus pad |
| 5 | JUPITER SYNC | Jupiter-style oscillator-sync lead |
| 6 | 303 SQUELCH | Acid-era resonant saw bass |
| 7 | MINI ROUND | Minimoog-style round sub bass |
| 8 | MS REED | MS-family band-pass reed lead |

### NES

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | FAMI HERO | Famicom narrow-pulse hero lead |
| 5 | FAMI LOW | Console triangle bass |
| 6 | FAMI ARP | Three-note chiptune chord arpeggio |
| 7 | 8BIT SNARE | Console long-register noise snare |
| 8 | DUNGEON PAD | Softened console pulse pad |

### DMG

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | POCKET BASS | Handheld square bass |
| 5 | POCKET BELL | Handheld wave-RAM bell |
| 6 | POCKET WIND | Filtered handheld noise atmosphere |
| 7 | 7BIT TOM | Short-register handheld percussion |
| 8 | LINK LEAD | Handheld pulse lead with stepped vibrato |

### PSG

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | AY BUZZ LEAD | AY buzzer-envelope lead |
| 5 | AY ZAP | AY tone/noise envelope zap |
| 6 | SEGA SQUARE | Mega Drive PSG square lead |
| 7 | SEGA CHORD | Mega Drive PSG two-tone fifth |
| 8 | SEGA HAT | Sega white-noise hat |

### TRIO

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | SID PWM | SID-style pulse-width lead |
| 5 | SID RUBBER | SID-style springy filtered bass |
| 6 | SID SYNC | SID-style hard-sync sweep |
| 7 | SID METAL | SID-style ring-modulated metal |
| 8 | SID CHOIR | SID combined-wave choir pad |

### FM6

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | TX RUBBER | TX-style elastic FM bass |
| 5 | DX GLASS | DX-style inharmonic glass bell |
| 6 | DX MALLET | DX-style wood/metal FM mallet |
| 7 | DX BRASS | DX-style FM brass ensemble |
| 8 | DX ORGAN | DX-style additive sine organ |

### YM

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | MD SLAP | Mega Drive-style metallic slap bass |
| 5 | MD REED | Mega Drive-style harmonically rich reed |
| 6 | ARCADE LEAD | OPN arcade-era feedback lead |
| 7 | MD EPIANO | Mega Drive FM electric piano |
| 8 | MD CHIME | Mega Drive FM chimes |

### WAVE

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | PPG CHOIR | PPG-style nasal/vocal wavetable motion |
| 5 | ESQ BRASS | ESQ-style digital brass |
| 6 | VS AIR | Prophet VS-style airy digital pad |
| 7 | PPG METAL | PPG-style metallic table strike |
| 8 | ESQ REED | ESQ-style reed lead |

### PHASE

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | CZ BASS | CZ-style phase-distortion bass |
| 5 | CZ STRINGS | CZ-style layered phase-distortion strings |
| 6 | CZ GLASS | CZ-style resonant glass pluck |
| 7 | CZ ORGAN | CZ-style doubled-sine organ |
| 8 | CZ WIRE | CZ-style resonant, lightly folded wire lead |

### STRING

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | KOTO PICK | Koto-like bright off-centre pluck |
| 5 | SITAR BUZZ | Sitar-like sympathetic buzz |
| 6 | ZITHER | Zither-like sympathetic strings |
| 7 | CLAVI PLUCK | Clavinet-like damped string attack |
| 8 | BOWED WIRE | Early physical-model bowed string texture |

### MODAL

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | VIBE KEYS | Electric-vibes-style resonant bars |
| 5 | TOY BELL | Toy keyboard bell timbre |
| 6 | GAMELAN | Gamelan-like inharmonic struck metal |
| 7 | STEEL TINE | Thumb-piano/steel-tine-like strike |
| 8 | SYNTH TOM | Early digital drum voice / tuned membrane |

### DRUM

| Position | Preset | Direction |
| --- | --- | --- |
| 4 | ELECTRO KIT | Bright analog-electro drum-machine feel |
| 5 | HOUSE KIT | Round low house drums |
| 6 | ARCADE KIT | Short driven arcade percussion |
| 7 | DUB KIT | Low tuned dub drum-machine feel |
| 8 | MINIMAL KIT | Small dry electronic percussion kit |

## Compatibility and size

Existing engine IDs, project/user-preset formats and the first three factory indices are
unchanged. Core5/core6 audio comparison checks all original 36 sounds through note-on,
release and effects. The expanded global preset browsing/program-change order includes
the appended sounds; saved projects and user presets carry their own sound parameters.

FM6 keeps F1–F3, OWN=3 and B1–B32=4–35. The five new full six-operator factory voices
load into OWN, and their actual operator patch is included in projects, user presets and
the optional desktop-library export. YM has separate operator settings for all eight voices.

The app grows from 405,784 to 408,704 bytes: 2,920 bytes. Ordinary RAM remains
52,280/73,728 bytes and the shared buffer pool 328,896/368,640. No recorded audio is
included in firmware. Hardware timing still needs checking on a device.

## Audition and validation

`build/core6/highlights.mp3` is a 48-second sampler (first new patch from each engine).
`build/core6/preset-auditions.zip` contains an offline HTML catalog and all 60 four-second
clips. Clips use a short original phrase, the actual sound loader/mixer/effects and the
presets' own levels; they are not loudness-normalized.

The optional `factory-library.json` is a desktop-library backup of all 96 defaults. The
firmware already contains them; the hardware user bank still has 32 slots.

All 96 presets render with distinct per-engine signatures, release and round-trip through
user-preset/project NOR storage. 232 independent mixer cases have no PCM rail hits; 27
two-core scenes match one core at every tested split. The new preview clips are also
checked for finite, audible, unclipped output.

To regenerate host audition assets after building the generated headers:

```sh
cc -DFELUCCA_CORE=1 -fwrapv -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src \
  tools/render_factory.c -lm -o build/host/render_factory
mkdir -p build/core6
build/host/render_factory --list > build/core6/factory-manifest.json
python3 tools/make_factory_previews.py build/core6
node tests/core_factory_library.mjs build/core6/factory-library.json
node tests/core_factory_compat.mjs build/core5/felucca.wasm build/emu/felucca.wasm
```
