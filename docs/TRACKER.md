# Yanfei tracker sequencer

For operating instructions, read the [user guide](USER_GUIDE.md).
Use the [quick reference](TRACKER_QUICK_REFERENCE.md) for the controls.
This document contains implementation and validation details.

## Yanfei name (y12)

The custom firmware is named Yanfei. The boot screen, ABOUT page, serial
console, editor identity and USB port names use that name. The source QR
links to github.com/izumoe/yanfei. Upstream credits and licence notices remain.
Projects, presets, backups, protocol command IDs and flash layout are unchanged.
Build with build-yanfei.sh. The package is yanfei.fwsc.
The earlier core11 build is the baseline for this name change.
The app is 431,356 bytes, an increase of 24 bytes. Ordinary RAM remains
66,248 bytes and the pool remains 345,048 bytes. The original interrupt
budget passes at 6,531 weighted instructions. The update package is 50 bytes
smaller because the USB names in the loader are shorter.

## Core11 default view

The tracker starts in ALL after a fresh boot. SHIFT + UP opens detailed PHRASE.
Leaving the tracker and returning keeps the current view during the session.
This changes the initial UI state only. Project, sound and playback formats
remain unchanged from core10.
The app grows by 16 bytes. Ordinary RAM, the pool, and the interrupt instruction
budget stay unchanged. Fresh-boot emulator checks verify the default and
return navigation.

## Core10 four-track overview

SHIFT + UP toggles PHRASE and ALL (view 3). The existing CHAIN and SONG
shortcuts stay available. ALL uses four 54-pixel columns and sixteen rows.
Each cell shows a pitch, two pitches, an exact chord label, or MULTI.
The chord matcher compares pitch-class sets against a small fixed table in
main-loop display code. It accepts inversions and octave doubling. It tries
the bass as root first for ambiguous sets. Drum summaries use kit-aware lane
abbreviations. Neither summaries nor transposition change the audio format.

ALL edits the selected track's working phrase through the existing entry,
clipboard, undo and instrument metadata paths. Pitch changes transpose the
whole stored chord, with group clamping at 0 and 127. Drum editing uses note
entry or the detailed lane view. PLAY uses the existing TS_ALL transport.
The display shows independent track playheads, including arrangement playback.
There is no additional framebuffer, project payload, engine workspace or audio
callback work. The format remains compatible with core8 and core9 projects.

The core10 app is 431,316 bytes, an increase of 2,704 bytes from core9.
Ordinary RAM remains 66,248 bytes. The pool remains 345,048 bytes.
The original target interrupt budget passes at 6,531 weighted instructions.
Display lint passes 9,200 scenes across ten palettes and all display styles.
Control tests run under address and undefined-behavior sanitizers. Browser
checks exercise actual WASM note entry, row edits, drum entry and playback.
The existing tracker library, backup and all-engine audio regressions pass.
Device audio deadlines have not been measured here.

## Core9 display update

PHRASE has four compact fields in its existing 16-pixel footer: CH (chance or
condition), RT (ratchet), ND (nudge), and VEL (velocity). These fields show the
selected row, including when the playhead is elsewhere. Inapplicable values are
dim. Messages temporarily replace the fields, including row-gate feedback.
The sixteen-row layout and engine sound-knob controls remain the same.

Idle screens draw nothing. A cursor move redraws at most two 12-pixel rows and
the 16-pixel footer. A selected-row value edit redraws its row and footer;
an edit on another row leaves the footer alone. Layout lint adds chance/condition,
nudge, ratchet, accent, and inactive-row cases, for 7,280 scenes in total.
Core9 uses the same project and preset formats as core8.
The core9 app is 428,612 bytes. Ordinary RAM and pool use are unchanged.

The control reference is [Ketch](https://github.com/risseraka/fm1-tracker-ketch),
reviewed at `87ff579`. This firmware implements the keypad workflow around our
engines, four-note chords, eight drum lanes and existing sound automation.

## Phrases, chains and song sections

Four tracks play simultaneously, sharing the existing eight-voice budget.
Each project has a fixed library:

| Item | Limit | Contents |
| --- | --- | --- |
| Phrase | 32 shared phrases, 1–16 rows each | Four-note chords or eight drum lanes, row instruments, gates and step settings |
| Chain | 16 chains, 16 links each | Phrase references with −24..+24 semitones transpose |
| Song | 32 section rows × four tracks | One chain reference per track per section |
| Instrument | 16 shared sounds | Engine, sound parameters and the full FM6 patch |
| Automation | 128 unique events/locks across the project | Sparse events attached to phrase rows |

IDs display in hexadecimal: P00..P1F, C00..C0F, instruments 00..0F.
A/B/C sections can be represented by song rows using different chains; there
are no additional named section banks. Paste reuses an existing item. Clone
creates a variation: a cloned phrase copies notes, instrument references, gates
and automation; its instruments are still shared. A cloned chain copies its
sequence and transpose values while retaining references to the same phrases.
Clone a phrase within that chain when its notes should differ too.

To make a variation, copy a chain reference to another song cell, clone that
chain, open it and clone whichever phrases need different notes. The original
section keeps its original phrases. To introduce another sound, select an unused
I number on a phrase row (it starts as a copy of the selected sound), then use
the normal preset/engine pages to edit it. Reusing an I number shares that sound.

PLAY on PHRASE loops the selected track. PLAY on CHAIN loops that chain on
the selected track. PLAY on SONG, or SHIFT+PLAY, plays the arrangement from
the selected song row. All participating tracks finish a section before the
next section starts; shorter chains wait silently. Empty sections and empty
links are skipped, and the song stops at its end. HOME/panel PLAY outside the
tracker loops the four currently selected phrases. The normal clock and MIDI
transport remain available. Chain/song editing and recording require stopping.

## Keybed controls

| Keys | Action |
| --- | --- |
| F3 / A3 | Left / right |
| G3 / G#3 | Down / up |
| D5 | SHIFT |
| E5 | PLAY / stop |
| F5 | OPTION |
| G5 | EDIT |
| SHIFT + left | Phrase → chain → song |
| SHIFT + right | Song → selected chain → selected phrase; from phrase, instrument editing |
| OPTION + left/right | Select track |
| OPTION + up/down | Select phrase ID; in CHAIN, select chain ID |
| EDIT tap | Insert last note into an empty cell; copy a filled note/value; toggle drum hit; create/copy a library reference |
| EDIT + arrows | Edit selected value; notes move by semitone/octave |
| OPTION + EDIT | Clear selected cell/reference |
| SHIFT + OPTION | Copy phrase row, chain link, or whole four-track song section |
| SHIFT + EDIT | Paste row/link/section; EDIT on a filled song cell copies just that reference |
| SHIFT + OPTION + EDIT | Clone the phrase referenced by a chain cell, or chain referenced by a song cell |
| EDIT + PLAY | Audition cursor row while stopped, on PHRASE |
| SHIFT + PLAY | Play song from selected section |

EDIT on an empty chain cell allocates a phrase. EDIT on an empty song cell
allocates an unused chain. PRESETS or EDIT+arrows can select an existing ID.
An exhausted library reports FULL; it never overwrites another phrase to make
room. Undo removes a newly created reference while retaining its library item.

## Notes and row parameters

Melodic rows display four notes, instrument (I), velocity, chance/condition and
ratchet. Drum rows display eight lanes, I and velocity; DETAIL supplies chance,
ratchet and nudge. Values other than pitch apply to the whole row/chord.
Drum lane accents are independent and play at velocity 127.

| Parameter | Range / behavior |
| --- | --- |
| Pitch | MIDI 0–127, up to four notes per row |
| Instrument | 00..0F; `..` inherits the latest earlier instrument, or the phrase's default |
| Velocity | 1–127; imported zero uses the existing default 96 |
| Chance / condition | 0–100%, or cycle conditions 1:2..4:4, FILL and !FILL |
| Ratchet | 1–4 evenly spaced triggers in a step |
| Nudge | −8..+7 sixteenths of a step; QUANTIZE OFF, without ratchet |
| Gate | HOLD, or 1..128 units of a step (displayed as a percentage) |
| Ties | Extend an entire chord across following rows |
| Locks / automation | Existing eligible engine/envelope/mix/FX parameters through STEP and AUTOMATION |

A blank row does not trigger or cut a note. HOLD sustains until a new note,
an explicit OFF, section completion or stop. HOLD does not imply pitch glide.
Finite gates release earlier; a following tie extends them. F#3 in NOTE mode
writes OFF; EDIT+F#3 writes a tie. Clearing a note cell leaves a blank.
Ratchets use a finite gate within each subdivision; HOLD maps to the full
subdivision for repeated hits. Existing slide flags remain supported.

Instrument changes use the existing short engine fade and workspace handoff;
release tails do not accumulate independent engine instances. Instrument edits
are shared across all references. Track timing/routing are not replaced by a
sound change. Live knob tweaks during playback are temporary; stop restores the
selected edit instrument. Dedicated tracker pitch-slide/vibrato/arpeggio command
columns are not added in this build; existing engines, modulation, locks and
ratchets remain available.

## Panel controls and undo

| Control | Action |
| --- | --- |
| REC | NOTE entry → ARM for live recording → NAV |
| A#3..C#5 in NOTE mode | Play/write a chord; advance once all note keys are released |
| SELECT | Move row cursor |
| EDIT + SELECT | Extend/shorten note with ties |
| OPTION + SELECT | Set cursor row gate; zero = HOLD; footer shows value |
| PRESETS | Edit selected cell without loading another sound |
| ALGORITHM | Select track |
| KNOB 1–4 | Current engine's HOME sound controls |
| HOME tap | Toggle playhead following |
| HOME held | Settings menu |
| GLO | Open tracker SONG once library is active |
| GLO held + SELECT | Tempo |
| SEQ tap | Tracker, STEP, DETAIL, AUTOMATION |
| SAVE | Existing save pages; PROJECT includes the entire library |
| SAVE held | One-gesture undo/redo, including row instrument/gate/OFF metadata |

Navigation silences the keybed. NOTE mode sounds only the middle sixteen keys;
modifiers remain silent. Live recording writes only the running play position.
Row paste includes instrument, gate and OFF along with notes/step settings;
automation stays attached to the destination. Phrase clone includes automation.
Undo of a pattern edit requires returning to its phrase to avoid applying a
snapshot to another phrase. Reference and instrument-cell edits also have undo.

## Storage, compatibility and budgets

On first entry to TRACKER while stopped, all four working 64-step patterns are
split into sixteen phrase pages, preserving even notes outside the active LEN.
Four initial chains preserve each track's active length. Existing notes, chords,
drum hits, chance/ratchet/nudge, automation and complete FM6 patches are kept.
Imported rests become explicit OFFs and existing gates are copied to their rows,
so conversion does not change their articulation. New phrases start blank.
Existing saved-project song arrangements remain readable through the legacy
path before activation; they are not automatically translated into the new song
library. Save a converted project into another slot to retain its original.

The new store has four manual project objects and one restore-last object.
Each uses two 16 KiB copies in **0xA0000..0xC7FFF**, the first two otherwise unused
sample slots. The third sample slot (0xC8000..0xDBFFF), user presets, FM6 banks,
settings and OTA are outside this allocation. Core sample upload/erase refuses
the two reserved slots. SAMPLE remains deferred.

The versioned payload is 15,728 bytes, including the legacy 3,840-byte base
project and compact instrument parameters. Writes erase only the alternate copy,
verify the body and commit its CRC-protected header last. Loading fully validates
both copies and can fall back from a torn or semantically invalid newer copy.
Erase commits a tombstone. Rename, restore-last and full browser backups preserve
the entire library. Legacy core projects remain loadable; new tracker libraries
require core8. The desktop sequencer still exposes the compatible working steps;
tracker-only fields and arrangement are edited on the device/emulator.

The active library is 13,648 bytes of ordinary RAM. Project staging and undo use
the fixed pool; there are no heap allocations or unbounded audio-path walks.
Target builds retain at least 4 KiB ordinary RAM and 8 KiB pool headroom. Exact
build totals and checks are recorded alongside the firmware in `build/core8`.
This build uses 428,864 bytes of app space, 66,248/73,728 bytes ordinary RAM,
and 345,048/368,640 bytes of the pool. The combined static ISR/keyboard/step
dispatch cost is 6,531 against the original 8,285 baseline and its unchanged
tolerance; the out-of-line step and keyboard work is charged back into that guard.

## Verification

`tests/run_core_tests.sh` covers the compatibility editor, full migration,
shared edits and automation, clone/reference undo, note articulation, arrangement
barriers, transposition, all factory sounds, journal failure/fallback, and the
full backup protocol. NOR tests inject power cuts through body/header writes and
erases, checking canaries outside the reserved region. Screen lint exercises
5,360 scenes across palettes, styles, LARGE settings, engines and library views.
`tools/capture_tracker.py` drives the actual WASM firmware panel, captures the
screens and checks a save/reload in a fresh instance. Host/sanitizer checks and
static target budgets supplement these tests; device audio deadlines and physical
control ergonomics still need FM-1 testing.
