/* SPDX-License-Identifier: GPL-3.0-only */
/* Sixteen rows on the 240px display. Each dirty row is one 12px blit; no
 * framebuffer or new fonts. The normal palette and display styles apply. */
static struct { uint32_t head, foot, row[16]; } tracker_draw;
static void tracker_hex(char *s, uint32_t v)
{
    static const char H[] = "0123456789ABCDEF";
    s[0] = H[(v >> 4) & 15u]; s[1] = H[v & 15u]; s[2] = 0;
}
static int32_t tracker_x(uint32_t c, int drum)
{
#if FELUCCA_CORE
    if (ts_run.enabled) return drum ? c < 8u ? 22 + (int32_t)c * 21 : c == 8u ? 190 : 212 :
                                      c < 4u ? 22 + (int32_t)c * 27 : c == 4u ? 130 : c == 5u ? 154 : c == 6u ? 181 : 213;
#endif
    return drum ? 22 + (int32_t)c * 24 : c < 4u ? 22 + (int32_t)c * 31 : 146 + ((int32_t)c - 4) * 30;
}
static int32_t tracker_w(uint32_t c, int drum)
{
#if FELUCCA_CORE
    if (ts_run.enabled) return drum ? c < 8u ? 21 : c == 8u ? 22 : 28 : c < 4u ? 27 : c == 4u ? 24 : c == 5u ? 27 : c == 6u ? 32 : 27;
#endif
    return drum ? 24 : c < 4u ? 31 : 30;
}
/* Four compact DETAIL values fit the existing footer without reducing the
 * sixteen visible rows. The knobs retain their engine HOME controls. */
static void tracker_draw_detail(const track_t *t, const step_t *s)
{
    static const char *const label[4] = {"CH", "RT", "ND", "VEL"};
    char value[12];
    for (uint32_t c = 0; c < 4u; c++) {
        uint32_t kind = SD_KIND[c];
        int on = sd_takes(s, kind);
        if (c == 0u) {
            const char *unit = ev_chance_fmt(value, (int32_t)step_chance_v(s));
            str_cpy(value + str_len(value), unit, sizeof value - str_len(value));
        } else if (c == 1u) {
            value[0] = 'x'; fmt_int(value + 1, (int32_t)step_ratchet(s));
        } else if (c == 2u) {
            ev_nudge_fmt(value, step_nudge(s));
            if (step_nudge(s)) str_cpy(value + str_len(value), "/16", sizeof value - str_len(value));
            on &= ev_nudge_plays(t, s);
        } else {
            fmt_int(value, s->vel ? s->vel : 96);
            on &= ev_vel_plays(s);
        }
        int32_t x = (int32_t)c * 60;
        cv_text(2 + x, 0, &AF_S, label[c], T_MID);
        cv_text((c == 3u ? 24 : 20) + x, 0, &AF_S, value, on ? T_TEXT : T_DIM);
    }
}
static void tracker_draw_library(void)
{
#if FELUCCA_CORE
    char b[40], value[12]; uint32_t song_view = tracker.view == 2;
    uint32_t base = song_view ? (tracker.song_row / 16u) * 16u : 0;
    uint32_t sig = ts_hash(song_view ? (const void *)ts_bank.order : (const void *)ts_bank.chain[tracker.chain],
                           song_view ? sizeof ts_bank.order : sizeof ts_bank.chain[0]);
    sig ^= tracker.song_row | (uint32_t)tracker.chain_row << 6 | (uint32_t)tracker.song_col << 12 |
           (uint32_t)tracker.table_col << 16 | (uint32_t)tracker.chain << 20 | (uint32_t)tracker.view << 26;
    if (song_view) sig ^= ts_hash(ts_section, sizeof ts_section);
    sig ^= ts_hash(ts_run.order, sizeof ts_run.order) ^ ts_hash(ts_run.link, sizeof ts_run.link) ^ ts_run.mode;
    sig ^= (uint32_t)(ui.msg_t != 0u) << 31;
    if (!ui.force && sig == tracker_draw.head) { if (ui.msg_t) ui.msg_t--; return; }
    tracker_draw.head = sig; cv_oy = 0;
    cv_begin(240, 20, T_BG);
    str_cpy(b, song_view ? "SONG" : "CHAIN C", sizeof b);
    if (!song_view) { tracker_hex(value, tracker.chain); str_cpy(b + str_len(b), value, sizeof b - str_len(b)); }
    cv_text(4, 0, &AF_M, b, T_THEME);
    if (song_view) {
        uint32_t beats = ts_section[tracker.song_row].beats;
        if (beats) { fmt_int(value, (int32_t)((beats + 3u) / 4u)); str_cpy(value + str_len(value), " BAR", sizeof value - str_len(value)); }
        else str_cpy(value, "AUTO", sizeof value);
        cv_text(90, 3, &AF_S, value, T_MID);
    }
    cv_text(154, 3, &AF_S, song.playing ? "PLAY" : "STOP", T_MID);
    fmt_int(value, song.g[G_BPM]); cv_text(213, 3, &AF_S, value, T_MID); cv_blit(0, 0);
    cv_begin(240, 12, T_SURF); cv_text(4, -2, &AF_S, "#", T_MID);
    if (song_view) for (uint32_t k = 0; k < NTRK; k++) {
        b[0] = '1' + k; b[1] = ts_section[tracker.song_row].beats ?
            (ts_section[tracker.song_row].loop & (1u << k) ? 'R' : 'W') : ' '; b[2] = 0;
        cv_text(38 + (int32_t)k * 50, -2, &AF_S, b, T_MID);
    }
    else { cv_text(30, -2, &AF_S, "PHRASE", T_MID); cv_text(116, -2, &AF_S, "TRANSPOSE", T_MID); }
    cv_blit(0, 20);
    for (uint32_t r = 0; r < 16u; r++) {
        uint32_t row = base + r, selected = row == (song_view ? tracker.song_row : tracker.chain_row);
        uint16_t bg = selected ? T_TINT : r % 4u == 0u ? T_SURF : T_BG;
        cv_begin(240, 12, bg); tracker_hex(value, row); cv_text(4, -2, &AF_S, value, T_MID);
        for (uint32_t c = 0; c < (song_view ? NTRK : 2u); c++) {
            int32_t x = song_view ? 30 + (int32_t)c * 50 : c ? 110 : 28;
            uint32_t id = song_view ? ts_bank.order[row][c] : ts_bank.chain[tracker.chain][row].phrase;
            uint16_t fg = id == TS_NONE ? T_DIM : T_THEME, under = bg;
            if (!song_view && c) fmt_int(value, ts_bank.chain[tracker.chain][row].transpose);
            else if (id == TS_NONE) str_cpy(value, "--", sizeof value);
            else { value[0] = song_view ? 'C' : 'P'; tracker_hex(value + 1, id); }
            if (selected && c == (song_view ? tracker.song_col : tracker.table_col)) {
                cv_rect(x, 0, song_view ? 44 : c ? 80 : 70, 12, T_THEME); fg = T_INK; under = T_THEME;
            }
            cv_text_on(x + 3, -2, &AF_S, value, fg, under);
            uint32_t part = song_view ? c : song.sel;
            if (ts_run.mode && (ts_run.active & (1u << part)) &&
                (song_view ? ts_run.order[part] == row : ts_run.chain_id[part] == tracker.chain && ts_run.link[part] == row))
                cv_rect(x, 10, song_view ? 44 : 70, 1, T_ACCENT);
        }
        cv_blit(0, 32 + r * 12u);
    }
    cv_begin(240, 16, T_SURF);
    cv_text(4, 0, &AF_S, ui.msg_t ? ui.msg : song_view ? "SHIFT+UP: EDIT SECTION" : "SHIFT+RIGHT: PHRASE", T_MID);
    cv_blit(0, 224); if (ui.msg_t) ui.msg_t--; ui.force = 0;
#endif
}
#if FELUCCA_CORE
/* Exact pitch-class sets only: an unknown voicing is MULTI, never a guessed
 * major/minor label. Try the bass first to resolve e.g. C6 versus Am7. */
static int tracker_chord_label(char *out, const step_t *s)
{
    static const struct { uint16_t mask; const char *name; } shape[] = {
        {0x091, ""}, {0x089, "m"}, {0x049, "dim"}, {0x111, "+"},
        {0x085, "sus2"}, {0x0a1, "sus4"}, {0x491, "7"}, {0x891, "M7"},
        {0x489, "m7"}, {0x449, "m7b5"}, {0x249, "dim7"},
        {0x889, "mM7"}, {0x291, "6"}, {0x289, "m6"}
    };
    uint32_t mask = 0, bass = 127;
    for (uint32_t n = 0; n < s->n && n < 4u; n++) {
        uint32_t note = s->note[n]; mask |= 1u << (note % 12u);
        if (note < bass) bass = note;
    }
    for (uint32_t i = 0; i < 12u; i++) {
        uint32_t root = (bass + i) % 12u;
        if (!(mask & (1u << root))) continue;
        uint32_t relative = ((mask >> root) | (mask << (12u - root))) & 0xfffu;
        for (uint32_t q = 0; q < sizeof shape / sizeof shape[0]; q++) if (relative == shape[q].mask) {
            str_cpy(out, N_NOTE[root], 12);
            str_cpy(out + str_len(out), shape[q].name, 12 - str_len(out));
            return 1;
        }
    }
    return 0;
}
/* Return two separate note labels for dyads so even C#-1 / D#-1 fits. */
static uint32_t tracker_summary(const track_t *t, const step_t *s, uint32_t off, char a[12], char b[12])
{
    a[0] = b[0] = 0;
    if (s->time == ST_TIE) str_cpy(a, "===", 12);
    else if (s->time == ST_REST && off) str_cpy(a, "OFF", 12);
    else if (!step_on(s)) str_cpy(a, "---", 12);
    else if (drum_track(t)) {
        uint32_t lanes = step_lanes(s), count = 0;
        for (uint32_t k = 0; k < NLANE; k++) if (lanes & (1u << k)) {
            if (count < 2u) str_cpy(count ? b : a, drum_lane_abbr(t, k), 12);
            count++;
        }
        if (count > 2u) { str_cpy(a, "MULTI", 12); b[0] = 0; }
        else if (count == 2u) return 2;
    } else if (s->hit) str_cpy(a, "MULTI", 12);
    else if (s->n <= 2u) {
        note_name(a, s->note[0]);
        if (s->n == 2u) { note_name(b, s->note[1]); return 2; }
    } else if (!tracker_chord_label(a, s)) str_cpy(a, "MULTI", 12);
    return 1;
}
#endif
/* The section view can show an unbound blank page without changing a phrase. */
static uint32_t tracker_display_phrase(uint32_t part)
{
#if FELUCCA_CORE
    if (tracker_section_on()) return tracker_section_phrase(part);
    return ts_run.mode && ts_run.phrase[part] < TS_PHRASES ? ts_run.phrase[part] : ts_bank.edit[part];
#else
    (void)part; return 0;
#endif
}
static const step_t *tracker_display_step(uint32_t part, uint32_t row)
{
    return tracker_section_on() ? tracker_section_step(part, row) : &seq_steps(&trk[part])[row];
}
static uint32_t tracker_display_playing(uint32_t part, uint32_t row)
{
    if (!song.playing || trk[part].seq_idx != row) return 0;
#if FELUCCA_CORE
    if (ts_run.mode && (!(ts_run.active & (1u << part)) || (ts_run.waiting & (1u << part)))) return 0;
    if (tracker_section_on()) return ts_run.mode >= TS_SONG && ts_run.order[part] == tracker.song_row && ts_run.link[part] == tracker.page;
#endif
    return 1;
}
static void tracker_draw_footer(const track_t *t, uint32_t head, uint32_t len)
{
    uint32_t foot = fnv(head ^ (uint32_t)ui.bank ^ len << 8 ^ (uint32_t)(ui.msg_t != 0u) << 24,
                        ui.msg, str_len(ui.msg));
    const step_t *detail = tracker_display_step((uint32_t)(t - trk), ui.cursor);
    foot = fnv(foot ^ (uint32_t)ui.cursor << 16 ^ (uint32_t)t->p[P_SQNT] << 12, detail, sizeof *detail);
    if (ui.force || foot != tracker_draw.foot) {
        tracker_draw.foot = foot;
        cv_begin(240, 16, T_SURF);
        if (ui.msg_t) cv_text(4, 0, &AF_S, ui.msg, T_TEXT);
        else tracker_draw_detail(t, detail);
        cv_blit(0, 224);
    }
    if (ui.msg_t) ui.msg_t--;
}
static void tracker_draw_overview(void)
{
#if FELUCCA_CORE
    char a[12], b[12], label[24];
    uint32_t head = fnv(0x414c4cu, ts_bank.edit, sizeof ts_bank.edit) ^ song.sel ^
                    (uint32_t)tracker.notes << 8 ^ (uint32_t)song.rec << 9 ^
                    (uint32_t)song.playing << 13 ^ (uint32_t)tracker.follow << 14 ^
                    (uint32_t)song.g[G_BPM] << 16;
    uint32_t phrase[NTRK], length[NTRK];
    if (tracker_section_on()) head = fnv(head, &ts_section[tracker.song_row], sizeof ts_section[0]) ^
        ((uint32_t)tracker.song_row << 24) ^ ((uint32_t)(tracker.page + 1u) << 28);
    for (uint32_t k = 0; k < NTRK; k++) {
        phrase[k] = tracker_display_phrase(k);
        length[k] = tracker_section_on() ? phrase[k] < TS_PHRASES ? ts_bank.phrase[phrase[k]].length : TS_ROWS :
            (uint32_t)clamp(trk[k].p[P_SLEN], 1, TS_ROWS);
        head = fnv(head, &trk[k].eng_req, sizeof trk[k].eng_req);
        head = fnv(head, &phrase[k], sizeof phrase[k]);
    }
    cv_oy = 0;
    if (ui.force || head != tracker_draw.head) {
        tracker_draw.head = head; cv_begin(240, 20, T_BG);
        if (tracker_section_on()) { str_cpy(label, "ALL S", sizeof label); tracker_hex(label + 5, tracker.song_row); }
        else { str_cpy(label, "ALL 1 P", sizeof label); label[4] = '1' + song.sel; tracker_hex(label + 7, phrase[song.sel]); }
        cv_text(4, 0, &AF_M, label, T_THEME);
        if (tracker_section_on()) {
            label[0] = '1' + tracker.page; label[1] = '/'; label[2] = '0' + tracker_section_pages(); label[3] = 0;
            cv_text(82, 3, &AF_S, label, T_MID);
        }
        str_cpy(label, song.rec & (1u << song.sel) ? "ARM" : tracker.notes ? "NOTE" : "NAV", sizeof label);
        if (song.playing) str_cpy(label + str_len(label), " PLAY", sizeof label - str_len(label));
        cv_text(110, 3, &AF_S, label, song.rec & (1u << song.sel) ? T_REC : T_TEXT);
        fmt_int(a, song.g[G_BPM]); cv_text(213, 3, &AF_S, a, T_MID); cv_blit(0, 0);
        cv_begin(240, 12, T_SURF); cv_text(4, -2, &AF_S, "#", T_MID);
        for (uint32_t k = 0; k < NTRK; k++) {
            str_cpy(label, ENGINES[trk[k].eng_req % NENGINES]->name, sizeof label);
            cv_text(24 + (int32_t)k * 54, -2, &AF_S, label, k == song.sel ? T_THEME : T_MID);
        }
        cv_blit(0, 20);
    }
    for (uint32_t r = 0; r < TS_ROWS; r++) {
        uint32_t sig = head ^ (ui.cursor == r ? 0x10000000u : 0u);
        for (uint32_t k = 0; k < NTRK; k++) {
            const step_t *s = tracker_display_step(k, r);
            sig = fnv(sig, s, sizeof *s);
            uint32_t state = length[k] | ((phrase[k] < TS_PHRASES ? (uint32_t)ts_bank.phrase[phrase[k]].off >> r : 0u) & 1u) << 5 |
                (tracker_display_playing(k, r) ? 0x80000000u : 0u);
            sig = fnv(sig, &state, sizeof state);
        }
        if (!ui.force && tracker_draw.row[r] == sig) continue;
        tracker_draw.row[r] = sig;
        uint16_t bg = r % 4u == 0u ? T_SURF : T_BG;
        cv_begin(240, 12, bg); tracker_hex(a, r + (tracker_section_on() ? (uint32_t)tracker.page * TS_ROWS : 0u)); cv_text(4, -2, &AF_S, a, T_MID);
        for (uint32_t k = 0; k < NTRK; k++) {
            uint32_t selected = k == song.sel && r == ui.cursor, valid = r < length[k];
            int32_t x = 22 + (int32_t)k * 54;
            uint16_t under = selected ? T_THEME : bg, fg = selected ? T_INK : valid ? T_TEXT : T_DIM;
            if (selected) cv_rect(x, 0, 53, 12, under);
            uint32_t count = tracker_summary(&trk[k], tracker_display_step(k, r),
                phrase[k] < TS_PHRASES ? (ts_bank.phrase[phrase[k]].off >> r) & 1u : 0u, a, b);
            if (!valid) { a[0] = b[0] = 0; count = 1; }
            cv_text_on(x + 2, -2, &AF_S, a, fg, under);
            if (count == 2u) cv_text_on(x + 29, -2, &AF_S, b, fg, under);
            if (tracker_display_playing(k, r))
                cv_rect(x, 11, 53, 1, T_ACCENT);
        }
        cv_blit(0, 32 + r * 12u);
    }
    tracker_draw_footer(TSEL, head, length[song.sel]); ui.force = 0;
#endif
}

static void tracker_draw_page(void)
{
    track_t *t = TSEL;
    uint32_t i, c, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), base, head;
    int drum = drum_track(t);
    char b[40], v[12];
    tracker_fix();
#if FELUCCA_CORE
    if (ts_run.enabled && tracker.view == 3) { tracker_draw_overview(); return; }
    if (ts_run.enabled && tracker.view) { tracker_draw_library(); return; }
#endif
    base = ui.bank * 16u;
    head = (uint32_t)song.sel | (uint32_t)t->eng_req << 3 | (uint32_t)tracker.notes << 8 |
           (uint32_t)song.rec << 9 | (uint32_t)song.playing << 13 | (uint32_t)tracker.follow << 14 |
           (uint32_t)song.g[G_BPM] << 16;
#if FELUCCA_CORE
    uint32_t phrase = tracker_display_phrase(song.sel);
    if (ts_run.enabled) head ^= phrase << 24;
    if (tracker_section_on()) {
        len = phrase < TS_PHRASES ? ts_bank.phrase[phrase].length : TS_ROWS;
        head ^= ((uint32_t)(tracker.page + 1u) << 20) ^ ((uint32_t)tracker.song_row << 27);
    }
#endif
    cv_oy = 0;
    if (ui.force || head != tracker_draw.head) {
        tracker_draw.head = head;
        cv_begin(240, 20, T_BG);
        b[0] = '1' + song.sel; b[1] = ' '; b[2] = 0;
        str_cpy(b + 2, ENGINES[t->eng_req % NENGINES]->name, sizeof b - 2);
        cv_text(4, 0, &AF_M, b, T_THEME);
#if FELUCCA_CORE
        if (ts_run.enabled) {
            if (tracker_section_on()) { v[0] = '1' + tracker.page; v[1] = '/'; v[2] = '0' + tracker_section_pages(); v[3] = 0; }
            else { v[0] = 'P'; tracker_hex(v + 1, phrase); }
            cv_text(82, 3, &AF_S, v, T_MID);
        }
#endif
        str_cpy(b, song.rec & (1u << song.sel) ? "ARM" : tracker.notes ? "NOTE" : "NAV", sizeof b);
        if (song.playing) str_cpy(b + str_len(b), " PLAY", sizeof b - str_len(b));
        cv_text(110, 3, &AF_S, b, song.rec & (1u << song.sel) ? T_REC : T_TEXT);
        fmt_int(v, song.g[G_BPM]); cv_text(213, 3, &AF_S, v, T_MID);
        cv_blit(0, 0);
        cv_begin(240, 12, T_SURF);
        cv_text(4, -2, &AF_S, "#", T_MID);
        for (c = 0; c < tracker_ncol(); c++) {
            static const char *N[] = {"N1", "N2", "N3", "N4", "VEL", "%", "RT"};
            static const char *D[] = {"KD", "SD", "CH", "OH", "LT", "HT", "CP", "RM", "VEL"};
            uint32_t col = c; const char *name;
#if FELUCCA_CORE
            if (ts_run.enabled && c == (drum ? 8u : 4u)) name = "I";
            else { if (ts_run.enabled && c > (drum ? 8u : 4u)) col--; name = drum ? D[col] : N[col]; }
#else
            name = drum ? D[col] : N[col];
#endif
            cv_text(tracker_x(c, drum) + 2, -2, &AF_S, name, T_MID);
        }
        cv_blit(0, 20);
    }
    for (i = 0; i < 16u; i++) {
        uint32_t at = base + i, valid = at < len;
        const step_t *s = tracker_display_step(song.sel, at % NSTEP);
        uint32_t playing = tracker_display_playing(song.sel, at);
        uint32_t selected = ui.cursor == at;
        uint32_t sig = fnv(2166136261u, s, sizeof *s) ^ head ^ (base << 8) ^ len ^
                       (selected ? (tracker.col + 1u) << 16 : 0) ^ (playing ? 0x80000000u : 0);
#if FELUCCA_CORE
        if (ts_run.enabled && phrase < TS_PHRASES) sig ^= (uint32_t)ts_bank.phrase[phrase].instrument[at % TS_ROWS] << 8 ^ ts_bank.phrase[phrase].off;
#endif
        uint16_t bg = selected ? T_TINT : i % 4u == 0u ? T_SURF : T_BG;
        if (!ui.force && tracker_draw.row[i] == sig) continue;
        tracker_draw.row[i] = sig;
        cv_begin(240, 12, bg);
        tracker_hex(v, at + (tracker_section_on() ? (uint32_t)tracker.page * TS_ROWS : 0u));
        cv_text(4, -2, &AF_S, v, valid ? playing ? T_ACCENT : T_MID : T_DIM);
        if (playing) cv_rect(0, 0, 2, 12, T_ACCENT);
        for (c = 0; c < tracker_ncol(); c++) {
            uint32_t col = c;
#if FELUCCA_CORE
            if (ts_run.enabled && c > (drum ? 8u : 4u)) col--;
#endif
            uint16_t fg = valid ? T_TEXT : T_DIM;
            int32_t x = tracker_x(c, drum), w = tracker_w(c, drum);
            uint16_t under = bg;
            str_cpy(v, "---", sizeof v);
            if (!valid) str_cpy(v, "", sizeof v);
#if FELUCCA_CORE
            else if (ts_run.enabled && c == (drum ? 8u : 4u)) {
                uint32_t instrument_id = phrase < TS_PHRASES ? ts_bank.phrase[phrase].instrument[at % TS_ROWS] : TS_NONE;
                if (instrument_id == TS_NONE) str_cpy(v, "..", sizeof v); else tracker_hex(v, instrument_id);
            }
#endif
            else if (drum && c < NLANE) {
                str_cpy(v, step_lanes(s) & (1u << c) ? step_accents(s) & (1u << c) ?
#if FELUCCA_CORE
                        ts_run.enabled ? "!" :
#endif
                        "ACC" : "X" : ".", sizeof v);
                fg = step_lanes(s) & (1u << c) ? T_THEME : T_DIM;
            } else if (!drum && c < 4u) {
                if (s->time == ST_TIE && c == 0u) str_cpy(v, "===", sizeof v);
#if FELUCCA_CORE
                else if (ts_run.enabled && s->time == ST_REST && c == 0u && phrase < TS_PHRASES && (ts_bank.phrase[phrase].off & (1u << (at % TS_ROWS)))) str_cpy(v, "OFF", sizeof v);
#endif
                else if (s->time == ST_NOTE && c < s->n) { note_name(v, s->note[c]); fg = T_THEME; }
                else fg = T_DIM;
            } else if (drum || col == 4u) fmt_int(v, s->vel ? s->vel : 96);
            else if (col == 5u) {
                uint32_t cond = step_cond(s);
                if (cond) str_cpy(v, COND_NAME[cond - 1u], sizeof v);
                else fmt_int(v, (int32_t)step_chance(s));
            } else fmt_int(v, (int32_t)step_ratchet(s));
            if (selected && c == tracker.col) { cv_rect(x, 0, w - 1, 12, T_THEME); fg = T_INK; under = T_THEME; }
            cv_text_on(x + 2, -2, &AF_S, v, fg, under);
        }
        cv_blit(0, 32 + i * 12u);
    }
    tracker_draw_footer(t, head, len);
    ui.force = 0;
}
