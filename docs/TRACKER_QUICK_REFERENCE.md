# Yanfei tracker quick reference

See the [user guide](USER_GUIDE.md) for procedures and examples.

ALL is the default tracker view after a restart.
The tracker keeps your selected view during the current session.
Hold SHIFT and press UP to switch between ALL and detailed PHRASE.
In ALL, LEFT and RIGHT select a track. UP and DOWN select a row.
Use REC note entry to replace the cell with one note, a chord, or drum hits.
PRESETS and EDIT with arrows transpose the whole chord.
SHIFT + OPTION copies the row. SHIFT + EDIT pastes it. OPTION + EDIT clears it.
PLAY repeats the selected section. SHIFT + PLAY starts the song.
A new section has four pages: 64 rows at DIV 1/16, or four bars.
SELECT and UP/DOWN scroll across pages. SHIFT + SELECT changes page.
One or two notes show their pitches. Larger sets show a chord name or MULTI.
See [Use the four-track view](USER_GUIDE.md#use-the-four-track-view) for details.

## Keyboard keys

| Key position | Function |
| --- | --- |
| F3 | LEFT |
| F#3 | OFF in NOTE mode |
| G3 | DOWN |
| G#3 | UP |
| A3 | RIGHT |
| A#3 through C#5 | Note entry |
| D5 | SHIFT |
| E5 | PLAY |
| F5 | OPTION |
| G5 | EDIT |

These positions do not change with the octave setting.
The EDIT key and panel EDIT button are different controls.
For a combination, hold the modifiers before you press the last key.

## Navigation

| Control | Action |
| --- | --- |
| LEFT / RIGHT | Select a column. |
| UP / DOWN | Select a row. |
| SELECT knob | Select a row in the tracker. |
| OPTION + LEFT / RIGHT | Select a track. |
| OPTION + UP / DOWN | Change section page; in separate PHRASE, select a phrase; in CHAIN, select a chain. |
| SHIFT + UP in SONG | Edit the selected section in ALL. |
| SHIFT + DOWN in ALL / PHRASE | Switch section / separate phrase editing. |
| SHIFT + SELECT | Change section page. |
| SHIFT + OPTION + SELECT | Set section length: AUTO or one through four bars. |
| OPTION + PRESETS in section / SONG | Right: selected track repeats; left: plays once and waits. |
| SHIFT + LEFT | PHRASE → CHAIN → SONG. |
| SHIFT + RIGHT | SONG → selected CHAIN → selected PHRASE → sound editing. |
| GLO button | Open SONG after tracker activation. |
| ALGORITHM knob | Select a track. |

Press SEQ to cycle the tracker, STEP, DETAIL, and AUTOMATION pages.
In section editing, the heading shows the page number.
In separate PHRASE editing, it shows the engine and phrase number.
The table shows N1 through N4, or KD through RM.
In PHRASE and ALL, the bottom strip shows CH (chance), RT (ratchet), ND (nudge), and VEL (velocity) for the selected row.
KNOB 1 through KNOB 4 still control the sound in PHRASE.

## Edit and copy

| Control | Action |
| --- | --- |
| EDIT on an empty note cell | Insert a note. |
| EDIT on a filled note cell | Copy its pitch for later entry. |
| EDIT on a drum lane | Switch its hit on or off. |
| PRESETS knob | Change the selected tracker cell. |
| EDIT + LEFT / RIGHT | Change the value by one; move pitch by one semitone. |
| EDIT + UP / DOWN on a note | Raise/lower one octave. |
| EDIT + UP / DOWN on a drum lane | Set/remove accent. |
| OPTION + EDIT | Clear the selected cell or reference. |
| SHIFT + OPTION in PHRASE | Copy the whole phrase row. |
| SHIFT + OPTION in CHAIN | Copy the link and its transpose value. |
| SHIFT + OPTION in SONG | Copy all four cells, length, and repeat settings. |
| SHIFT + OPTION + LEFT in a section | Copy the selected track's page reference. |
| SHIFT + OPTION + RIGHT in a section | Paste a shared page reference. |
| SHIFT + OPTION + EDIT in a section | Clone its page for separate note edits. |
| EDIT on a filled SONG cell | Copy only its chain reference. |
| SHIFT + EDIT | Paste at the selected location. |
| SHIFT + OPTION + EDIT in CHAIN | Clone the selected phrase. |
| SHIFT + OPTION + EDIT in SONG | Clone the selected chain. |
| SAVE button held | Undo; hold again to redo. |

Pasted references share the original item.
A cloned chain still shares its phrases.
A cloned phrase still shares its instruments.
Row paste includes notes, instrument, gate, and step settings.
Existing automation at the destination stays in place.
Phrase clone also copies automation.

## Notes and playback

| Control or value | Action or meaning |
| --- | --- |
| REC button in section editing | NAV ↔ NOTE. |
| REC button in separate phrase editing | NAV → NOTE → ARM → NAV. |
| Note-entry keys in NOTE | Enter up to four pitches; release all keys to advance. |
| F#3 in NOTE | Write OFF and advance. |
| EDIT + F#3 in NOTE | Write a tie and advance. |
| OPTION held + SELECT knob | Set the row gate; the footer shows its value. |
| EDIT held + SELECT knob | Extend/shorten a note with ties. |
| EDIT + PLAY in PHRASE | Hear the selected row while stopped. |
| PLAY in section editing | Repeat the selected section. |
| PLAY in separate PHRASE editing | Repeat the selected track's phrase. |
| PLAY in CHAIN | Start at the selected link; repeat the chain on the selected track. |
| PLAY in SONG | Play from the selected section through the song end. |
| SHIFT + PLAY | Play SONG from the selected song row. |
| PLAY during playback | Stop. |
| HOME button in PHRASE | Switch playhead following on or off during phrase playback. |
| GLO button held + SELECT knob | Set tempo. |

Blank rows do not stop held notes.
HOLD has no timed note-off.
OFF releases the track's held notes.
For short notes, set a finite gate.

## DETAIL and save

Press SEQ until DETAIL appears.
Turn PRESETS to select the row.

| Knob | Field |
| --- | --- |
| KNOB 1 | CHANCE / condition |
| KNOB 2 | RATCH, one through four repeats |
| KNOB 3 | NUDGE, early or late timing |
| KNOB 4 | VEL |

NUDGE requires QUANTIZE OFF and RATCH set to one.
Set QUANTIZE on the AUTOMATION page.

To save a whole project:

1. Stop playback.
2. Press SAVE until PROJECT appears.
3. Select the slot with KNOB 1.
4. Turn KNOB 4 right to select SAVE.
5. Press OCT+.
6. Follow the overwrite and name prompts.
7. Press OCT+ on NAME to write the project.

To load, select LOAD with KNOB 3 on PROJECT, then press OCT+.
Loading replaces current work.
SAVE > USER saves a sound, not the complete project.

Fixed section lengths use musical time, independent of track LEN, DIV, and SWING.
At other divisions, a page is not necessarily one bar. A shorter section keeps
notes on later pages. SONG shows R for REPEAT and W for WAIT beside each track.
AUTO preserves the previous finish-all-chains timing for old projects.
Browsing blank pages uses no phrase slots. Written pages and clones share the
project's limit of 32 phrases. Pasting a page shares the source phrase.
Clone a shared chain in SONG before you change pages for only one section.
