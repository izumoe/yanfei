/* SPDX-License-Identifier: GPL-3.0-only */
/* Tracker milestone 1: edit the existing four parts' 64-row patterns. The
 * keypad follows Ketch (Adrien Risser-Zhuang, GPL-3.0); this implementation
 * keeps Felucca's four-note chords, eight drum lanes and stored step format.
 * No allocations or song walks in the audio path. */
static struct {
    uint8_t col, notes, follow, used, last_mask, clip_valid, repeat_key;
    uint8_t view, chain, chain_row, song_row, song_col, table_col;
    uint8_t table_clip_kind, table_clip_id;
    uint8_t song_clip[NTRK], song_clip_beats, song_clip_loop;
    int8_t table_clip_transpose;
    uint8_t last[7];
    uint32_t prev, repeat_at;
    step_t clip;
    uint8_t clip_instrument, clip_gate, clip_off;
    uint8_t section, page;
} tracker;
/* Set the default at startup; keep tracker in BSS and retain later view choices. */
static void tracker_init(void) { tracker.view = FELUCCA_CORE ? 3 : 0; tracker.section = FELUCCA_CORE; tracker.page = 0; }
static int tracker_section_on(void) { return FELUCCA_CORE && tracker.section && (tracker.view == 0 || tracker.view == 3); }
static int tracker_section_bind(void);
static int tracker_section_ensure(void);
static uint32_t tracker_section_pages(void);
static uint32_t tracker_section_phrase(uint32_t part);
static const step_t *tracker_section_step(uint32_t part, uint32_t row);
static void tracker_section_move(int32_t row);
static void tracker_section_length(int32_t delta);
static void tracker_section_loop(int32_t delta);
static void tracker_section_copy(void);
static void tracker_section_paste(void);
static void tracker_section_clone(void);
static void tracker_song_view(void) { tracker.view = 2; tracker.song_col = song.sel; }

static int tracker_on(void)
{
    return FELUCCA_CORE && !ui.home && !ui.menu && !ui.confirm && !ui.layer && !ui.uboot &&
           cur_page()->graph == GR_TRACKER;
}
static uint32_t tracker_keys_mode(void) { return tracker.notes ? 5u : 4u; }
static void tracker_publish(void)
{
    kb_tracker_mute = tracker.notes ? ~TRACKER_NOTE_KEYS : ~0u;
    song.grid = (uint8_t)tracker_keys_mode();
}
static void tracker_enter(void)
{
    tracker.prev = fm1_in.notes;
    tracker.used = 1;  /* release controls held while entering before editing */
    tracker.repeat_key = 0;
#if FELUCCA_CORE
    if (tracker_on() && !ts_run.enabled && !song.playing && !chain_busy()) ts_activate();
    if (tracker_on() && ts_run.enabled && tracker_section_on() && !song.playing) tracker_section_bind();
#endif
}
enum { TK_LEFT = 0, TK_OFF = 1, TK_DOWN = 2, TK_UP = 3, TK_RIGHT = 4,
       TK_SHIFT = 21, TK_PLAY = 23, TK_OPTION = 24, TK_EDIT = 26 };
static uint32_t tracker_ncol(void)
{
#if FELUCCA_CORE
    if (ts_run.enabled) return drum_track(TSEL) ? 10u : 8u;
#endif
    return drum_track(TSEL) ? 9u : 7u;
}
static void tracker_library_input(uint32_t pressed, uint32_t action, uint32_t held);
static void tracker_library_cell(int32_t delta, int clear);
static void tracker_play(uint32_t song_mode);
static void tracker_library_move(int32_t delta);
static void tracker_fix(void)
{
    if (!tracker_section_on()) cursor_fix();
    if (tracker.col >= tracker_ncol()) tracker.col = (uint8_t)(tracker_ncol() - 1u);
}
static void tracker_move(int32_t row)
{
    if (tracker_section_on()) tracker_section_move(row);
    else cursor_set(row);
    tracker.follow = 0;
}
static int tracker_edit_ok(void)
{
    if (chain_busy() || ts_arranging() || seq_counting() || (song.playing && (song.rec & (1u << song.sel)))) {
        ui_message("STOP TO EDIT");
        return 0;
    }
    return 1;
}
static void tracker_preview(void)
{
#if FELUCCA_CORE
    if (tracker_section_on() && (tracker_section_phrase(song.sel) >= TS_PHRASES || !tracker_section_bind())) return;
#endif
    if (!song.playing && !seq_counting() && !chain_busy() && transport_req != 1u)
        aud_req = (uint16_t)((uint32_t)ui.cursor << 8 | (song.sel + 1u));
}
static void tracker_gate(int32_t delta)
{
#if FELUCCA_CORE
    if (!ts_run.enabled || !tracker_edit_ok()) return;
    if (tracker_section_on() && !tracker_section_ensure()) return;
    step_undo_take(TSEL, 0x7600u + ui.cursor);
    ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[song.sel]];
    p->gate[ui.cursor] = (uint8_t)clamp((int32_t)p->gate[ui.cursor] + delta, 0, 128);
    motion_undo_done(TSEL);
    char b[24] = "GATE HOLD";
    if (p->gate[ui.cursor]) { str_cpy(b, "GATE ", sizeof b); fmt_int(b + 5, p->gate[ui.cursor] * 100 / 128); str_cpy(b + str_len(b), "%", sizeof b - str_len(b)); }
    ui_message(b);
#else
    (void)delta;
#endif
}
/* ALL edits a complete row. Transposition preserves the intervals even when
 * the lowest or highest note reaches the MIDI limit. No hidden note column. */
static void tracker_copy(void);
#if FELUCCA_CORE
static void tracker_overview_cell(int32_t delta, int clear)
{
    step_t *s = &TSEL->step[ui.cursor];
    if (!clear && !delta && (step_on(s) || s->time == ST_TIE ||
        (ts_bank.phrase[ts_bank.edit[song.sel]].off & (1u << ui.cursor)))) {
        tracker_copy(); return;
    }
    if (!tracker_edit_ok()) return;
    if (delta && drum_track(TSEL)) { ui_message("REC: ENTER DRUM HITS"); return; }
    uint32_t new_row = s->time != ST_NOTE || !s->n;
    step_undo_take(TSEL, delta ? 0x7700u + ui.cursor : 0);
    fm1_irq_off();
    if (clear) step_clear(s);
    else if (s->time == ST_NOTE && s->n && delta) {
        int32_t lo = 127, hi = 0;
        for (uint32_t n = 0; n < s->n && n < 4u; n++) {
            if (s->note[n] < lo) lo = s->note[n];
            if (s->note[n] > hi) hi = s->note[n];
        }
        delta = clamp(delta, -lo, 127 - hi);
        for (uint32_t n = 0; n < s->n && n < 4u; n++) s->note[n] = (uint8_t)(s->note[n] + delta);
    } else {
        step_clear(s); s->time = ST_NOTE; s->vel = 96;
        if (drum_track(TSEL)) s->hit = 1;
        else { s->n = 1; s->note[0] = (uint8_t)clamp(60 + delta, 0, 127); }
    }
    ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[song.sel]];
    p->off &= (uint16_t)~(1u << ui.cursor);
    if (clear || new_row) p->gate[ui.cursor] = 0;
    if (clear) p->instrument[ui.cursor] = TS_NONE;
    else if (new_row && p->instrument[ui.cursor] == TS_NONE) p->instrument[ui.cursor] = ts_bank.sound[song.sel];
    fm1_irq_on(); motion_undo_done(TSEL); tracker.follow = 0;
}
#endif
/* Change one cell transactionally. A missing chord cell appends a note; notes
 * stay packed, matching the existing sequencer and every stored project. */
static void tracker_cell(int32_t delta, int clear, int paste)
{
    tracker_fix();
#if FELUCCA_CORE
    if (ts_run.enabled && tracker_section_on()) {
        if (clear && tracker_section_phrase(song.sel) >= TS_PHRASES) return;
        if (!tracker_edit_ok() || !tracker_section_ensure()) return;
    }
#endif
    track_t *t = TSEL;
    uint32_t c = tracker.col, drum = (uint32_t)drum_track(t), k;
#if FELUCCA_CORE
    if (ts_run.enabled && tracker.view == 3) { tracker_overview_cell(delta, clear); return; }
    if (ts_run.enabled && (tracker.view == 1 || tracker.view == 2 || c == (drum ? 8u : 4u))) {
        tracker_library_cell(delta, clear); return;
    }
    if (ts_run.enabled && c > (drum ? 8u : 4u)) c--;
#endif
    step_t *s = &t->step[ui.cursor];
    if (!delta && !clear && !paste && !drum && (c >= 4u || (s->time == ST_NOTE && c < s->n))) {
        tracker.last[c] = (uint8_t)(c < 4u ? s->note[c] : c == 4u ? s->vel :
                                   c == 5u ? step_chance_v(s) : step_ratchet(s));
        tracker.last_mask |= (uint8_t)(1u << c);
        ui_message("CELL COPIED");
        return;
    }
    if (!tracker_edit_ok()) return;
    step_undo_take(t, clear || !delta ? 0u : 0x7400u + ui.cursor * 16u + c);
    fm1_irq_off();
    if (drum && c < NLANE) {
        if (clear) grid_hit(t, ui.cursor, c, 0);
        else if (delta > 1 || delta < -1) grid_acc(t, ui.cursor, c, delta > 0);
        else if (delta) grid_hit(t, ui.cursor, c, delta > 0);
        else grid_hit(t, ui.cursor, c, 2);
        if (step_on(s) && !s->vel) s->vel = 96;
    } else if (!drum && c < 4u) {
        if (clear) {
            if (s->time == ST_NOTE && c < s->n) {
                for (k = c; k + 1u < s->n; k++) s->note[k] = s->note[k + 1u];
                s->note[--s->n] = 0;
                if (!s->n && !s->hit) s->time = ST_REST;
            } else if (!s->n && !s->hit) {
                step_clear(s);
            }
        } else {
            uint32_t n = s->time == ST_NOTE ? s->n : 0u;
            uint32_t at = c < n ? c : n;
            int32_t base = at < n ? s->note[at] : (tracker.last_mask & (1u << c)) ? tracker.last[c] : 60;
            if (paste && (tracker.last_mask & (1u << c))) base = tracker.last[c];
            if (s->time != ST_NOTE) { step_clear(s); s->time = ST_NOTE; }
            if (!s->vel) s->vel = 96;
            s->note[at] = (uint8_t)clamp(base + delta, 0, 127);
            if (at >= n) s->n = (uint8_t)(at + 1u);
            tracker.col = (uint8_t)at;
            tracker.last[c] = s->note[at];
            tracker.last_mask |= (uint8_t)(1u << c);
        }
    } else {
        uint32_t field = drum ? 4u : c;
        int32_t v = field == 4u ? s->vel : field == 5u ? (int32_t)step_chance_v(s) : (int32_t)step_ratchet(s);
        if (clear) v = field == 4u ? 96 : field == 5u ? 100 : 1;
        else if (paste && (tracker.last_mask & (1u << field))) v = tracker.last[field];
        else if (!delta) { tracker.last[field] = (uint8_t)v; tracker.last_mask |= (uint8_t)(1u << field); }
        v += delta;
        if (field == 4u) s->vel = (uint8_t)clamp(v, 1, 127);
        else if (field == 5u) step_set_chance_v(s, (uint32_t)clamp(v, 0, CHANCE_V_MAX));
        else step_set_ratchet(s, (uint32_t)clamp(v, 1, 4));
    }
    fm1_irq_on();
#if FELUCCA_CORE
    if (ts_run.enabled && (drum ? c < NLANE : c < 4u)) {
        ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[song.sel]];
        p->off &= (uint16_t)~(1u << ui.cursor);
        if (!clear && !delta && !paste) p->gate[ui.cursor] = 0;
    }
#endif
    motion_undo_done(t);
    tracker.follow = 0;
}
static void tracker_copy(void)
{
    fm1_irq_off();
    tracker.clip = tracker_section_on() ? *tracker_section_step(song.sel, ui.cursor) : TSEL->step[ui.cursor];
#if FELUCCA_CORE
    if (ts_run.enabled) {
        uint32_t id = tracker_section_on() ? tracker_section_phrase(song.sel) : ts_bank.edit[song.sel];
        tracker.clip_instrument = TS_NONE; tracker.clip_gate = tracker.clip_off = 0;
        if (id < TS_PHRASES) {
            const ts_phrase_t *p = &ts_bank.phrase[id];
            tracker.clip_instrument = p->instrument[ui.cursor]; tracker.clip_gate = p->gate[ui.cursor];
            tracker.clip_off = (p->off >> ui.cursor) & 1u;
        }
    }
#endif
    fm1_irq_on();
    tracker.clip_valid = 1;
    ui_message("ROW COPIED");
}
static void tracker_paste(void)
{
    if (!tracker.clip_valid) { ui_message("COPY A ROW FIRST"); return; }
    if (!tracker_edit_ok()) return;
    if (tracker_section_on() && !tracker_section_ensure()) return;
    step_undo_take(TSEL, 0);
    fm1_irq_off();
    TSEL->step[ui.cursor] = tracker.clip;
#if FELUCCA_CORE
    if (ts_run.enabled) {
        ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[song.sel]];
        p->instrument[ui.cursor] = tracker.clip_instrument; p->gate[ui.cursor] = tracker.clip_gate;
        p->off = (uint16_t)((p->off & ~(1u << ui.cursor)) | (uint32_t)tracker.clip_off << ui.cursor);
    }
#endif
    fm1_irq_on();
    motion_undo_done(TSEL);
    tracker.follow = 0;
    ui_message("ROW PASTED");
}
static void tracker_rec(void)
{
    uint8_t bit = (uint8_t)(1u << song.sel);
    if (chain_busy() || ts_arranging()) { ui_message("STOP TO RECORD"); return; }
    if (!tracker.notes) {
        tracker.notes = 1;
        ui_message("NOTE ENTRY");
    } else if (!tracker_section_on() && !(song.rec & bit)) {
        song.rec |= bit;
        ui_message("ARMED - PLAY TO RECORD");
    } else {
        song.rec &= (uint8_t)~bit;
        tracker.notes = 0;
        ui_message("NAVIGATION");
    }
    ui.entry_open = 0;
    tracker_publish();
}
/* NOTE mode writes a chord on key-down, advances once all note keys are up.
 * Playing/armed uses the existing audio-side live recorder exclusively. */
static void tracker_entry(uint32_t pressed, uint32_t held)
{
    track_t *t = TSEL;
    uint32_t k, j;
    if (!tracker.notes || song.playing || seq_counting() || transport_req == 1u || chain_busy()) {
        ui.entry_open = 0;
        return;
    }
    pressed &= TRACKER_NOTE_KEYS;
    for (k = 5u; k <= 20u; k++)
        if ((pressed >> k & 1u) && kb_map(t, k) == KB_SILENT) pressed &= ~(1u << k);
    if (pressed) {
        if (tracker_section_on() && !tracker_section_ensure()) return;
        if (!ui.entry_open) step_undo_take(t, 0);
        fm1_irq_off();
        step_t *s = &t->step[ui.cursor];
        if (!ui.entry_open) {
            /* Re-enter notes without deleting the row's timing/probability. */
            memset(s->note, 0, sizeof s->note);
            s->n = s->hit = s->acc = 0; s->time = ST_NOTE; s->vel = kb_vel;
            ui.entry_open = 1;
#if FELUCCA_CORE
            if (ts_run.enabled) ts_bank.phrase[ts_bank.edit[song.sel]].instrument[ui.cursor] = ts_bank.sound[song.sel];
            if (ts_run.enabled) {
                ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[song.sel]];
                p->off &= (uint16_t)~(1u << ui.cursor); p->gate[ui.cursor] = 0;
            }
#endif
        }
        for (k = 5u; k <= 20u; k++) if ((pressed >> k) & 1u) {
            uint8_t ch[CHORD_MAX]; int32_t root; uint16_t mask;
            uint32_t note = kb_map(t, k), n;
            if (note == KB_SILENT) continue;
            n = chord_make(t, note, ch, &root, &mask);
            for (j = 0; j < n; j++) {
                uint32_t lane, q;
                if (drum_track(t) && (lane = drum_lane(ch[j])) < NLANE) s->hit |= (uint8_t)(1u << lane);
                else {
                    for (q = 0; q < s->n && s->note[q] != ch[j]; q++);
                    if (q == s->n && s->n < 4u) s->note[s->n++] = ch[j];
                }
            }
        }
        fm1_irq_on();
        motion_undo_done(t);
        tracker.follow = 0;
        }
    if (ui.entry_open && !(held & TRACKER_NOTE_KEYS)) {
        tracker_move((int32_t)ui.cursor + 1);
        }
}
static void tracker_input(uint32_t pressed)
{
    uint32_t held = fm1_in.notes & ~kb_layer, release = tracker.prev & ~held;
    uint32_t edit = held & (1u << TK_EDIT), shift = held & (1u << TK_SHIFT), option = held & (1u << TK_OPTION);
    uint32_t arrows = held & 0x1Du, action = pressed & 0x1Du, k;
    tracker.prev = held;
    tracker_fix();
    if (tracker.follow && song.playing && !chain_busy() && !ui.entry_open) {
#if FELUCCA_CORE
        if (tracker_section_on() && ts_run.mode >= TS_SONG) {
            tracker.song_row = ts_run.order[song.sel];
            tracker.page = (uint8_t)clamp(ts_run.link[song.sel], 0, tracker_section_pages() - 1u);
            ui.cursor = (uint8_t)(TSEL->seq_idx % TS_ROWS); ui.bank = 0;
        } else
#endif
        cursor_set((int32_t)(TSEL->seq_idx % (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP)));
    }
    if (pressed & (1u << TK_EDIT)) tracker.used = 0;
    if (arrows && !action && tracker.repeat_key && (held & (1u << (tracker.repeat_key - 1u))) &&
        (int32_t)(fm1_ms - tracker.repeat_at) >= 0) {
        action = 1u << (tracker.repeat_key - 1u);
        tracker.repeat_at = fm1_ms + 85u;
    } else if (action) {
        for (k = 0; k <= 4u; k++) if ((action >> k) & 1u) break;
        tracker.repeat_key = (uint8_t)(k + 1u);
        tracker.repeat_at = fm1_ms + 350u;
    } else if (!arrows) tracker.repeat_key = 0;
#if FELUCCA_CORE
    if (ts_run.enabled && (tracker.view == 1 || tracker.view == 2)) {
        tracker_library_input(pressed, action, held); return;
    }
    if (ts_run.enabled && tracker_section_on() && shift && option && (pressed & (1u << TK_EDIT))) {
        tracker_section_clone(); tracker.used = 1; return;
    }
    if (ts_run.enabled && tracker_section_on() && shift && option && (action & ((1u << TK_LEFT) | (1u << TK_RIGHT)))) {
        if (action & (1u << TK_LEFT)) tracker_section_copy(); else tracker_section_paste();
        tracker.used = 1; return;
    }
#endif
    if (pressed & (1u << TK_PLAY)) {
        if (edit) { tracker_preview(); tracker.used = 1; }
        else if (song.playing || seq_counting() || chain_busy()) transport_req = 2;
        else tracker_play(shift != 0u);
    }
    if ((pressed & (1u << TK_OPTION)) && shift) tracker_copy();
    if (edit && option && (pressed & ((1u << TK_EDIT) | (1u << TK_OPTION)))) {
        tracker_cell(0, 1, 0); tracker.used = 1;
    } else if ((pressed & (1u << TK_EDIT)) && shift) {
        tracker_paste(); tracker.used = 1;
    }
    if (action) {
        int32_t dx = (action & (1u << TK_RIGHT) ? 1 : 0) - (action & (1u << TK_LEFT) ? 1 : 0);
        int32_t dy = (action & (1u << TK_DOWN) ? 1 : 0) - (action & (1u << TK_UP) ? 1 : 0);
#if FELUCCA_CORE
        if (ts_run.enabled && shift && !edit && !option && dy < 0) {
            tracker.view = tracker.view == 3 ? 0 : 3;
            ui.entry_open = 0; tracker.used = 1; ui.force = 1; return;
        } else if (ts_run.enabled && shift && !edit && !option && dy > 0) {
            tracker.section ^= 1u; tracker.page = 0; ui.entry_open = 0;
            if (tracker.section) tracker_section_bind();
            ui.force = 1; ui_message(tracker.section ? "SECTION EDIT" : "PHRASE EDIT"); return;
        } else
#endif
        if (edit) {
            tracker_cell(dx - dy * (!drum_track(TSEL) && (tracker.view == 3 || tracker.col < 4u) ? 12 : 16), 0, 0);
            tracker.used = 1;
        } else if (option && dy) {
#if FELUCCA_CORE
            if (ts_run.enabled) {
                if (tracker_section_on()) tracker_move((int32_t)ui.cursor + dy * TS_ROWS);
                else {
                    int32_t id = (int32_t)ts_bank.edit[song.sel] + dy;
                    if (ts_select_phrase(song.sel, (uint32_t)clamp(id, 0, TS_PHRASES - 1u))) ui_message("STOP / AUTOMATION FULL");
                    else cursor_set(0);
                }
            }
#endif
        } else if ((option || (tracker.view == 3 && !shift)) && dx) {
            uint32_t row = ui.cursor;
            track_select((uint32_t)clamp((int32_t)song.sel + dx, 0, NTRK - 1));
            if (tracker.view == 3) { ui.cursor = (uint8_t)row; ui.entry_open = 0; }
            tracker_fix(); tracker.follow = 0;
        } else if (shift && dx) {
#if FELUCCA_CORE
            if (ts_run.enabled && dx < 0) {
                ts_sync();
                uint32_t id = ts_bank.order[tracker.song_row][song.sel];
                if (id < TS_CHAINS) tracker.chain = (uint8_t)id;
                for (uint32_t r = 0; r < TS_ROWS; r++) if (ts_bank.chain[tracker.chain][r].phrase == ts_bank.edit[song.sel]) {
                    tracker.chain_row = (uint8_t)r; break;
                }
                tracker.view = 1; tracker.used = 1; return;
            }
#endif
            open_family(dx > 0 ? FAM_EDIT : FAM_GLO);
            ui.entry_open = 0;
            return;
        } else if (dy) tracker_move((int32_t)ui.cursor + dy * (shift ? 16 : 1));
        else tracker.col = (uint8_t)clamp((int32_t)tracker.col + dx, 0, (int32_t)tracker_ncol() - 1);
        }
    if ((release & (1u << TK_EDIT)) && !tracker.used) tracker_cell(0, 0, 0);
    if (tracker.notes && (pressed & (1u << TK_OFF)) && tracker_edit_ok()) {
        if (tracker_section_on() && !tracker_section_ensure()) return;
        step_undo_take(TSEL, 0);
        fm1_irq_off();
        step_clear(&TSEL->step[ui.cursor]);
        if (edit) TSEL->step[ui.cursor].time = ST_TIE;
#if FELUCCA_CORE
        if (ts_run.enabled) {
            ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[song.sel]];
            if (edit) p->off &= (uint16_t)~(1u << ui.cursor); else p->off |= (uint16_t)(1u << ui.cursor);
        }
#endif
        fm1_irq_on();
        motion_undo_done(TSEL);
        tracker_move((int32_t)ui.cursor + 1);
        if (edit) tracker.used = 1;
    }
    tracker_entry(pressed, held);
}

#include "ui_tracker_library.c"
#include "ui_tracker_section.c"

