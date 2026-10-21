/* SPDX-License-Identifier: GPL-3.0-only */
/* Four-page section editing reuses the existing phrase working caches. Missing
 * pages are read-only blanks until a note or reference is written. */
#if FELUCCA_CORE
static uint32_t tracker_section_phrase(uint32_t part)
{
    uint32_t chain = ts_bank.order[tracker.song_row][part];
    return chain < TS_CHAINS ? ts_bank.chain[chain][tracker.page].phrase : TS_NONE;
}
static const step_t *tracker_section_step(uint32_t part, uint32_t row)
{
    static const step_t empty = {.time = ST_REST, .vel = 96};
    uint32_t id = tracker_section_phrase(part);
    if (id >= TS_PHRASES) return &empty;
    if (id == ts_bank.edit[part] && (!song.playing || ts_run.phrase[part] == id)) return &trk[part].step[row];
    return &ts_bank.phrase[id].step[row];
}
static uint32_t tracker_section_pages(void)
{
    uint32_t beats = ts_section[tracker.song_row].beats, pages = 1;
    if (beats) return (beats + 3u) / 4u;
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t chain = ts_bank.order[tracker.song_row][k];
        if (chain < TS_CHAINS) for (uint32_t p = 0; p < 4u; p++)
            if (ts_bank.chain[chain][p].phrase < TS_PHRASES && p + 1u > pages) pages = p + 1u;
    }
    return pages;
}
/* Bind all four caches transactionally. Intermediate bindings must not reject
 * an otherwise valid final automation count or flush a blank onto an old page. */
static int tracker_section_bind(void)
{
    if (!ts_run.enabled || !tracker_section_on() || song.playing) return 1;
    if (ts_sync()) { ui_message("AUTOMATION FULL"); return 0; }
    uint8_t old[NTRK]; memcpy(old, ts_bank.edit, sizeof old);
    uint32_t f = motion_guard();
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t id = tracker_section_phrase(k);
        if (id < TS_PHRASES) ts_bank.edit[k] = (uint8_t)id;
    }
    if (ts_bind_motion()) {
        memcpy(ts_bank.edit, old, sizeof old); ts_bind_motion();
        motion_unguard(f); ui_message("AUTOMATION FULL"); return 0;
    }
    for (uint32_t k = 0; k < NTRK; k++) if (old[k] != ts_bank.edit[k]) {
        uint32_t id = ts_bank.edit[k];
        memcpy(trk[k].step, ts_bank.phrase[id].step, sizeof ts_bank.phrase[id].step);
        memset(trk[k].step + TS_ROWS, 0, (NSTEP - TS_ROWS) * sizeof(step_t));
        trk[k].p[P_SLEN] = ts_bank.phrase[id].length;
        ts_bank.sound[k] = ts_bank.phrase[id].sound; ts_load_sound(k, ts_bank.sound[k]);
        ts_run.cache[k] = ts_cache_hash(k); ts_run.patch[k] = ts_patch_hash(k);
    }
    motion_unguard(f); return 1;
}
static int tracker_section_ensure(void)
{
    if (!tracker_section_on()) return 1;
    if (!tracker_edit_ok() || song.playing) { ui_message("STOP TO EDIT"); return 0; }
    if (!tracker_section_bind()) return 0;
    uint32_t part = song.sel, chain = ts_bank.order[tracker.song_row][part];
    if (tracker_section_phrase(part) >= TS_PHRASES) {
        uint32_t new_chain = chain >= TS_CHAINS;
        if (new_chain && (chain = tracker_new_chain()) >= TS_CHAINS) { ui_message("CHAIN BANK FULL"); return 0; }
        uint32_t id = tracker_new_phrase();
        if (id >= TS_PHRASES) { ui_message("PHRASE BANK FULL"); return 0; }
        ts_link_t old = ts_bank.chain[chain][tracker.page];
        uint8_t old_order = ts_bank.order[tracker.song_row][part];
        ts_bank.chain[chain][tracker.page] = (ts_link_t){(uint8_t)id, 0};
        ts_bank.order[tracker.song_row][part] = (uint8_t)chain;
        if (!tracker_section_bind()) {
            ts_bank.chain[chain][tracker.page] = old;
            ts_bank.order[tracker.song_row][part] = old_order;
            ts_bank.phrases &= ~(1u << id); return 0;
        }
        if (ts_bank.song_length <= tracker.song_row) ts_bank.song_length = tracker.song_row + 1u;
    }
    return 1;
}
static void tracker_section_move(int32_t row)
{
    uint32_t old = tracker.page;
    int32_t length = (int32_t)tracker_section_pages() * TS_ROWS;
    int32_t absolute = clamp((int32_t)tracker.page * TS_ROWS + row, 0, length - 1);
    tracker.page = (uint8_t)(absolute / TS_ROWS);
    if (old != tracker.page && !tracker_section_bind()) { tracker.page = (uint8_t)old; return; }
    ui.cursor = (uint8_t)(absolute % TS_ROWS); ui.bank = 0; ui.entry_open = 0;
    if (old != tracker.page) ui.force = 1;
}
static void tracker_section_length(int32_t delta)
{
    if (!tracker_edit_ok() || song.playing) return;
    ts_section_t *s = &ts_section[tracker.song_row];
    uint32_t bars = s->beats ? (s->beats + 3u) / 4u : 0u;
    uint32_t next = (uint32_t)clamp((int32_t)bars + delta, 0, 4);
    tracker_remember(5, tracker.song_row, 0);
    if (!s->beats && next) s->loop = 15;
    s->beats = (uint8_t)(next * 4u);
    if (ts_bank.song_length <= tracker.song_row) ts_bank.song_length = tracker.song_row + 1u;
    if (tracker.page >= tracker_section_pages()) tracker.page = (uint8_t)(tracker_section_pages() - 1u);
    tracker_section_bind(); ui.force = 1;
    char b[24] = "SECTION AUTO";
    if (next) { str_cpy(b, "SECTION ", sizeof b); fmt_int(b + 8, (int32_t)next); str_cpy(b + str_len(b), " BAR", sizeof b - str_len(b)); }
    ui_message(b);
}
static void tracker_section_loop(int32_t delta)
{
    if (!tracker_edit_ok() || song.playing) return;
    if (!ts_section[tracker.song_row].beats) { ui_message("SET SECTION LENGTH"); return; }
    uint32_t part = tracker.view == 2 ? tracker.song_col : song.sel;
    tracker_remember(5, tracker.song_row, 0);
    if (delta > 0) ts_section[tracker.song_row].loop |= (uint8_t)(1u << part);
    else ts_section[tracker.song_row].loop &= (uint8_t)~(1u << part);
    ui.force = 1; ui_message(delta > 0 ? "TRACK REPEAT" : "TRACK WAIT");
}
static void tracker_section_copy(void)
{
    uint32_t id = tracker_section_phrase(song.sel), chain = ts_bank.order[tracker.song_row][song.sel];
    if (id >= TS_PHRASES || chain >= TS_CHAINS) { ui_message("EMPTY PAGE"); return; }
    tracker.table_clip_kind = 4; tracker.table_clip_id = (uint8_t)id;
    tracker.table_clip_transpose = ts_bank.chain[chain][tracker.page].transpose;
    ui_message("PAGE REFERENCE COPIED");
}
static void tracker_section_paste(void)
{
    if (tracker.table_clip_kind != 4 || tracker.table_clip_id >= TS_PHRASES) { ui_message("COPY A PAGE FIRST"); return; }
    if (!tracker_edit_ok() || song.playing || ts_sync()) return;
    uint32_t chain = ts_bank.order[tracker.song_row][song.sel], created = chain >= TS_CHAINS;
    if (created && (chain = tracker_new_chain()) >= TS_CHAINS) { ui_message("CHAIN BANK FULL"); return; }
    if (created) { tracker_remember(2, tracker.song_row, song.sel); ts_bank.order[tracker.song_row][song.sel] = (uint8_t)chain; }
    else tracker_remember(1, chain, tracker.page);
    ts_link_t old = ts_bank.chain[chain][tracker.page];
    ts_bank.chain[chain][tracker.page] = (ts_link_t){tracker.table_clip_id, tracker.table_clip_transpose};
    if (!tracker_section_bind()) {
        ts_bank.chain[chain][tracker.page] = old;
        if (created) ts_bank.order[tracker.song_row][song.sel] = TS_NONE;
        ts_history.kind = 0; return;
    }
    if (ts_bank.song_length <= tracker.song_row) ts_bank.song_length = tracker.song_row + 1u;
    ui.force = 1; ui_message("PAGE SHARED - CLONE TO VARY");
}
static void tracker_section_clone(void)
{
    uint32_t chain = ts_bank.order[tracker.song_row][song.sel];
    if (chain >= TS_CHAINS || tracker_section_phrase(song.sel) >= TS_PHRASES) { ui_message("EMPTY PAGE"); return; }
    uint8_t view = tracker.view, old_chain = tracker.chain, row = tracker.chain_row;
    tracker.view = 1; tracker.chain = (uint8_t)chain; tracker.chain_row = tracker.page;
    tracker_clone(); tracker.view = view; tracker.chain = old_chain; tracker.chain_row = row;
    tracker_section_bind();
}
#else
static int tracker_section_bind(void) { return 1; }
static int tracker_section_ensure(void) { return 1; }
static uint32_t tracker_section_pages(void) { return 1; }
static uint32_t tracker_section_phrase(uint32_t part) { (void)part; return TS_NONE; }
static const step_t *tracker_section_step(uint32_t part, uint32_t row) { return &trk[part].step[row]; }
static void tracker_section_move(int32_t row) { cursor_set(row); }
static void tracker_section_length(int32_t delta) { (void)delta; }
static void tracker_section_loop(int32_t delta) { (void)delta; }
static void tracker_section_copy(void) {}
static void tracker_section_paste(void) {}
static void tracker_section_clone(void) {}
#endif
