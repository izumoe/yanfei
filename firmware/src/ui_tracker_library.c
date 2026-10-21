/* SPDX-License-Identifier: GPL-3.0-only */
/* Song/chain navigation and reference edits. No flash work in these handlers. */
static struct { uint8_t kind, a, b, value, allocated, song_row[NTRK], beats, loop; int8_t transpose; } ts_history;
static void tracker_history_swap(void)
{
#if FELUCCA_CORE
    if (!ts_history.kind || song.playing) { ui_message("NOTHING TO UNDO"); return; }
    uint32_t f = motion_guard(); uint8_t value; int8_t transpose;
    if (ts_history.kind == 1) {
        ts_link_t *l = &ts_bank.chain[ts_history.a][ts_history.b];
        value = l->phrase; transpose = l->transpose; l->phrase = ts_history.value; l->transpose = ts_history.transpose;
        ts_history.transpose = transpose;
    } else if (ts_history.kind == 2) {
        value = ts_bank.order[ts_history.a][ts_history.b]; ts_bank.order[ts_history.a][ts_history.b] = ts_history.value;
    } else if (ts_history.kind == 3) {
        value = ts_bank.phrase[ts_history.a].instrument[ts_history.b];
        ts_bank.phrase[ts_history.a].instrument[ts_history.b] = ts_history.value;
    } else {
        value = 0;
        uint8_t beats = ts_section[ts_history.a].beats, loop = ts_section[ts_history.a].loop;
        ts_section[ts_history.a] = (ts_section_t){ts_history.beats, ts_history.loop};
        ts_history.beats = beats; ts_history.loop = loop;
        if (ts_history.kind == 4) for (uint32_t k = 0; k < NTRK; k++) {
            uint8_t old = ts_bank.order[ts_history.a][k];
            ts_bank.order[ts_history.a][k] = ts_history.song_row[k]; ts_history.song_row[k] = old;
        }
    }
    /* Undo removes the reference. The created library item remains reusable. */
    ts_history.value = value;
    motion_unguard(f);
    if (tracker_section_on()) {
        if (tracker.page >= tracker_section_pages()) tracker.page = (uint8_t)(tracker_section_pages() - 1u);
        tracker_section_bind();
    }
    ui.force = 1; ui_message("UNDO / REDO");
#endif
}
static void tracker_remember(uint32_t kind, uint32_t a, uint32_t b)
{
#if FELUCCA_CORE
    ts_history.kind = (uint8_t)kind; ts_history.a = (uint8_t)a; ts_history.b = (uint8_t)b; ts_history.allocated = TS_NONE;
    if (kind == 1) { ts_history.value = ts_bank.chain[a][b].phrase; ts_history.transpose = ts_bank.chain[a][b].transpose; }
    else if (kind == 2) ts_history.value = ts_bank.order[a][b];
    else if (kind == 3) ts_history.value = ts_bank.phrase[a].instrument[b];
    else {
        if (kind == 4) memcpy(ts_history.song_row, ts_bank.order[a], NTRK);
        ts_history.beats = ts_section[a].beats; ts_history.loop = ts_section[a].loop;
    }
    undo.trk = 0;
#endif
}
static uint32_t tracker_new_phrase(void)
{
#if FELUCCA_CORE
    for (uint32_t id = 0; id < TS_PHRASES; id++) if (!(ts_bank.phrases & (1u << id))) {
        ts_phrase_t *p = &ts_bank.phrase[id]; memset(p, 0, sizeof *p);
        memset(p->instrument, TS_NONE, TS_ROWS); p->length = TS_ROWS; p->sound = ts_bank.sound[song.sel];
        for (uint32_t row = 0; row < TS_ROWS; row++) p->step[row].time = ST_REST;
        ts_bank.phrases |= 1u << id; return id;
    }
#endif
    return TS_NONE;
}
static uint32_t tracker_new_chain(void)
{
#if FELUCCA_CORE
    for (uint32_t id = 0; id < TS_CHAINS; id++) {
        uint32_t used = 0;
        for (uint32_t r = 0; r < TS_ROWS; r++) used |= ts_bank.chain[id][r].phrase != TS_NONE;
        for (uint32_t r = 0; r < TS_SONG_ROWS; r++) for (uint32_t k = 0; k < NTRK; k++) used |= ts_bank.order[r][k] == id;
        if (!used) return id;
    }
#endif
    return TS_NONE;
}
static void tracker_clone(void)
{
#if FELUCCA_CORE
    if (!tracker_edit_ok() || song.playing || ts_sync()) { ui_message("STOP TO CLONE"); return; }
    if (tracker.view == 1) {
        ts_link_t *link = &ts_bank.chain[tracker.chain][tracker.chain_row];
        uint32_t source = link->phrase, extra = 0;
        if (source >= TS_PHRASES) { ui_message("SELECT A PHRASE"); return; }
        for (uint32_t e = 0; e < ts_bank.count; e++) extra += ts_bank.event[e].phrase == source;
        if (ts_bank.count + extra > MOTION_MAX) { ui_message("AUTOMATION FULL"); return; }
        uint32_t id = tracker_new_phrase();
        if (id == TS_NONE) { ui_message("PHRASE BANK FULL"); return; }
        ts_bank.phrase[id] = ts_bank.phrase[source];
        uint32_t count = ts_bank.count;
        for (uint32_t e = 0; e < count; e++) if (ts_bank.event[e].phrase == source) {
            ts_motion_t copy = ts_bank.event[e]; copy.phrase = (uint8_t)id;
            ts_bank.event[ts_bank.count++] = copy;
        }
        tracker_remember(1, tracker.chain, tracker.chain_row); link->phrase = (uint8_t)id;
        ui_message("PHRASE CLONED");
    } else if (tracker.view == 2) {
        uint8_t *ref = &ts_bank.order[tracker.song_row][tracker.song_col];
        if (*ref >= TS_CHAINS) { ui_message("SELECT A CHAIN"); return; }
        uint32_t id = tracker_new_chain();
        if (id == TS_NONE) { ui_message("CHAIN BANK FULL"); return; }
        memcpy(ts_bank.chain[id], ts_bank.chain[*ref], sizeof ts_bank.chain[0]);
        tracker_remember(2, tracker.song_row, tracker.song_col); *ref = (uint8_t)id;
        ui_message("CHAIN CLONED");
    }
    ui.force = 1;
#endif
}
static void tracker_library_cell(int32_t delta, int clear)
{
#if FELUCCA_CORE
    if (!tracker_edit_ok()) return;
    if (song.playing) { ui_message("STOP TO EDIT LIBRARY"); return; }
    if (ts_sync()) { ui_message("AUTOMATION FULL"); return; }
    if (tracker.view == 1) {
        ts_link_t *l = &ts_bank.chain[tracker.chain][tracker.chain_row];
        if (!delta && !clear && l->phrase != TS_NONE) {
            tracker.table_clip_kind = 1; tracker.table_clip_id = l->phrase; tracker.table_clip_transpose = l->transpose;
            ui_message("LINK COPIED"); return;
        }
        tracker_remember(1, tracker.chain, tracker.chain_row);
        if (tracker.table_col == 1) l->transpose = (int8_t)(clear ? 0 : clamp(l->transpose + delta, -24, 24));
        else if (clear) { l->phrase = TS_NONE; l->transpose = 0; }
        else if (delta) {
            uint32_t id = (uint32_t)clamp((l->phrase == TS_NONE ? 0 : (int32_t)l->phrase) + delta, 0, TS_PHRASES - 1u);
            if (!(ts_bank.phrases & (1u << id))) { ts_bank.phrases |= 1u << id; ts_bank.phrase[id].sound = ts_bank.sound[song.sel]; }
            l->phrase = (uint8_t)id;
        } else {
            uint32_t id = tracker_new_phrase();
            if (id == TS_NONE) { ui_message("PHRASE BANK FULL"); return; }
            l->phrase = (uint8_t)id; ts_history.allocated = (uint8_t)id;
        }
    } else if (tracker.view == 2) {
        uint8_t *value = &ts_bank.order[tracker.song_row][tracker.song_col];
        if (!delta && !clear && *value != TS_NONE) {
            tracker.table_clip_kind = 2; tracker.table_clip_id = *value; ui_message("CHAIN COPIED"); return;
        }
        tracker_remember(2, tracker.song_row, tracker.song_col);
        if (clear) *value = TS_NONE;
        else if (delta) *value = (uint8_t)clamp((*value == TS_NONE ? 0 : (int32_t)*value) + delta, 0, TS_CHAINS - 1u);
        else {
            uint32_t id = tracker_new_chain(); if (id == TS_NONE) { ui_message("CHAIN BANK FULL"); return; }
            *value = (uint8_t)id;
        }
        if (!clear && ts_bank.song_length <= tracker.song_row) ts_bank.song_length = (uint8_t)(tracker.song_row + 1u);
    } else {
        uint32_t id = ts_bank.edit[song.sel]; uint8_t *value = &ts_bank.phrase[id].instrument[ui.cursor];
        uint32_t sound = *value == TS_NONE ? ts_bank.sound[song.sel] : *value;
        tracker_remember(3, id, ui.cursor);
        if (clear) *value = TS_NONE;
        else {
            sound = (uint32_t)clamp((int32_t)sound + delta, 0, TS_INSTRUMENTS - 1u);
            if (ts_select_sound(song.sel, sound)) { ui_message("STOP TO EDIT SOUND"); return; }
            *value = (uint8_t)sound;
        }
    }
    ui.force = 1;
#endif
}
static void tracker_library_move(int32_t delta)
{
    if (tracker.view == 1) tracker.chain_row = (uint8_t)clamp((int32_t)tracker.chain_row + delta, 0, TS_ROWS - 1u);
    else if (tracker.view == 2) tracker.song_row = (uint8_t)clamp((int32_t)tracker.song_row + delta, 0, TS_SONG_ROWS - 1u);
    else tracker_move((int32_t)ui.cursor + delta);
}
static void tracker_play(uint32_t song_mode)
{
#if FELUCCA_CORE
    if (ts_run.enabled) {
        uint32_t mode = song_mode || tracker.view == 2 ? TS_SONG : tracker.view == 1 ? TS_CHAIN : tracker_section_on() ? TS_SECTION : tracker.view == 3 ? TS_ALL : TS_PHRASE;
        uint32_t start = mode >= TS_SONG ? tracker.song_row : tracker.chain_row;
        uint32_t rc = ts_prepare(mode, song.sel, tracker.chain, start);
        if (rc) { ui_message(rc == 2 ? "AUTOMATION FULL" : "EMPTY CHAIN / SONG"); return; }
        transport_req = 1; return;
    }
#endif
    if (song_mode) chain_play_ui(); else transport_req = 1;
}
static void tracker_library_input(uint32_t pressed, uint32_t action, uint32_t held)
{
#if FELUCCA_CORE
    uint32_t edit = held & (1u << TK_EDIT), option = held & (1u << TK_OPTION), shift = held & (1u << TK_SHIFT);
    if (pressed & (1u << TK_PLAY)) {
        if (song.playing || seq_counting() || chain_busy()) transport_req = 2; else tracker_play(shift != 0u);
    }
    if ((pressed & (1u << TK_EDIT)) && option && shift) tracker_clone();
    else if ((pressed & (1u << TK_EDIT)) && option) tracker_library_cell(0, 1);
    else if ((pressed & (1u << TK_EDIT)) && shift) {
        if (tracker_edit_ok() && !song.playing && tracker.view == 2 && tracker.table_clip_kind == 3) {
            tracker_remember(4, tracker.song_row, 0);
            memcpy(ts_bank.order[tracker.song_row], tracker.song_clip, NTRK);
            ts_section[tracker.song_row] = (ts_section_t){tracker.song_clip_beats, tracker.song_clip_loop};
            if (ts_bank.song_length <= tracker.song_row) ts_bank.song_length = tracker.song_row + 1u;
            ui_message("SECTION PASTED"); ui.force = 1;
        } else if (tracker_edit_ok() && !song.playing && tracker.table_clip_kind == tracker.view && tracker.table_clip_id != TS_NONE) {
            if (tracker.view == 1) {
                tracker_remember(1, tracker.chain, tracker.chain_row);
                ts_bank.chain[tracker.chain][tracker.chain_row] = (ts_link_t){tracker.table_clip_id, tracker.table_clip_transpose};
            } else {
                tracker_remember(2, tracker.song_row, tracker.song_col);
                ts_bank.order[tracker.song_row][tracker.song_col] = tracker.table_clip_id;
                if (ts_bank.song_length <= tracker.song_row) ts_bank.song_length = tracker.song_row + 1u;
            }
            ui_message("REFERENCE PASTED"); ui.force = 1;
        } else ui_message("EDIT A LINK TO COPY IT");
    } else if (pressed & (1u << TK_EDIT)) tracker_library_cell(0, 0);
    if ((pressed & (1u << TK_OPTION)) && shift) {
        if (tracker.view == 1) {
            const ts_link_t *l = &ts_bank.chain[tracker.chain][tracker.chain_row];
            tracker.table_clip_id = l->phrase; tracker.table_clip_transpose = l->transpose;
            tracker.table_clip_kind = 1; ui_message("REFERENCE COPIED");
        } else {
            memcpy(tracker.song_clip, ts_bank.order[tracker.song_row], NTRK);
            tracker.song_clip_beats = ts_section[tracker.song_row].beats; tracker.song_clip_loop = ts_section[tracker.song_row].loop;
            tracker.table_clip_kind = 3; ui_message("SECTION COPIED");
        }
    }
    if (!action) return;
    int32_t dx = (action & (1u << TK_RIGHT) ? 1 : 0) - (action & (1u << TK_LEFT) ? 1 : 0);
    int32_t dy = (action & (1u << TK_DOWN) ? 1 : 0) - (action & (1u << TK_UP) ? 1 : 0);
    if (shift && !edit && !option && dy < 0 && tracker.view == 2) {
        tracker.view = 3; tracker.section = 1; tracker.page = 0;
        track_select(tracker.song_col); ui.cursor = ui.bank = 0; tracker_section_bind();
    } else if (edit) tracker_library_cell(dx - dy * (tracker.view == 1 && tracker.table_col ? 12 : 4), 0);
    else if (shift && dx) {
        if (dx < 0) { tracker.view = 2; tracker.song_col = song.sel; }
        else if (tracker.view == 2) {
            uint32_t id = ts_bank.order[tracker.song_row][tracker.song_col];
            if (id == TS_NONE) { tracker_library_cell(0, 0); id = ts_bank.order[tracker.song_row][tracker.song_col]; }
            if (id < TS_CHAINS) { tracker.chain = (uint8_t)id; tracker.view = 1; track_select(tracker.song_col); }
        } else {
            uint32_t id = ts_bank.chain[tracker.chain][tracker.chain_row].phrase;
            if (id == TS_NONE) { tracker.table_col = 0; tracker_library_cell(0, 0); id = ts_bank.chain[tracker.chain][tracker.chain_row].phrase; }
            if (id < TS_PHRASES && !ts_select_phrase(song.sel, id)) { tracker.view = 0; tracker.section = 0; cursor_set(0); }
        }
    } else if (option && dx) {
        track_select((uint32_t)clamp((int32_t)song.sel + dx, 0, NTRK - 1u));
    } else if (option && dy && tracker.view == 1) tracker.chain = (uint8_t)clamp((int32_t)tracker.chain + dy, 0, TS_CHAINS - 1u);
    else if (dy) tracker_library_move(dy * (shift ? 16 : 1));
    else if (dx && tracker.view == 1) tracker.table_col = (uint8_t)clamp((int32_t)tracker.table_col + dx, 0, 1);
    else if (dx) tracker.song_col = (uint8_t)clamp((int32_t)tracker.song_col + dx, 0, NTRK - 1u);
    ui.force = 1;
#endif
}
