/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded song/chain/phrase and full-instrument model. Main-loop editing uses
 * the existing track caches; audio reads the committed phrase bank. */
#define TS_PHRASES 32u
#define TS_CHAINS 16u
#define TS_ROWS 16u
#define TS_SONG_ROWS 32u
#define TS_INSTRUMENTS 16u
#define TS_NONE 255u
enum { TS_ALL = 1, TS_PHRASE, TS_CHAIN, TS_SONG, TS_SECTION };
#if FELUCCA_CORE
/* Zero length preserves the pre-section-timer arrangement behavior. */
typedef struct { uint8_t beats, loop; } ts_section_t;
static ts_section_t ts_section[TS_SONG_ROWS];
typedef struct {
    step_t step[TS_ROWS];
    uint8_t instrument[TS_ROWS];
    uint8_t gate[TS_ROWS];        /* 0 HOLD; 1..128 fraction of a step */
    uint16_t off;                /* an explicit OFF; other empty rows do nothing */
    uint8_t length, sound, motion;
} ts_phrase_t;
typedef struct { uint8_t phrase; int8_t transpose; } ts_link_t;
typedef struct { int16_t p[P_COUNT]; uint8_t fm6[FM6_PACKED], engine, preset; } ts_instrument_t;
typedef struct { uint8_t phrase, row, param; int8_t value; } ts_motion_t;
typedef struct {
    ts_phrase_t phrase[TS_PHRASES];
    ts_link_t chain[TS_CHAINS][TS_ROWS];
    uint8_t order[TS_SONG_ROWS][NTRK];
    ts_instrument_t instrument[TS_INSTRUMENTS];
    ts_motion_t event[MOTION_MAX];
    uint32_t phrases;
    uint16_t sounds;
    uint8_t count, song_length, edit[NTRK], sound[NTRK];
} ts_bank_t;
static ts_bank_t ts_bank;
static struct {
    uint8_t enabled, mode, pending, part, chain, row, start, active, waiting, saved_rec, error;
    uint8_t phrase[NTRK], order[NTRK], link[NTRK], chain_id[NTRK], current[NTRK], cycle[NTRK];
    int8_t transpose[NTRK];
    int16_t length[NTRK];
    uint32_t cache[NTRK], patch[NTRK], motion_cache[NTRK];
    uint32_t clock_pos, carry;
    uint16_t clock_row;
    uint8_t clock_first, gap;
} ts_run;
static motion_store_t ts_motion_scratch __attribute__((section(".pool")));
static motion_store_t motion;
static step_t *ts_play_cache(uint32_t part);
_Static_assert(sizeof(ts_bank_t) < 14500u, "tracker bank RAM budget");
static uint32_t motion_guard(void);
static void motion_unguard(uint32_t);
static int16_t motion_base_value(const track_t *, uint32_t);
static void motion_restore(track_t *);
static void motion_rebase(track_t *);
static void seq_release(track_t *);
static void seq_stop(void);
static uint32_t ts_hash(const void *p, uint32_t n)
{
    const uint8_t *b = p; uint32_t h = 2166136261u;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}
static int ts_sound_param(uint32_t id)
{
    return id != P_MUTE && !(id >= P_AMODE && id <= P_SGATE) &&
           !(id >= P_SLCR && id <= P_SLDEPTH) && id != P_CHRD && id != P_VOIC && id != P_SQNT;
}
static void ts_capture_sound(uint32_t part, uint32_t sound)
{
    ts_instrument_t *s = &ts_bank.instrument[sound];
    track_t *t = &trk[part];
    for (uint32_t p = 0; p < P_COUNT; p++) s->p[p] = motion_base_value(t, p);
    s->engine = t->eng_req; s->preset = t->preset;
    fm6_pack(fm6_patch[part], s->fm6);
    ts_bank.sounds |= (uint16_t)(1u << sound);
}
/* The existing engine fade owns and clears the shared workspace before any
 * queued notes start. Same-engine patch changes also fade the old voices. */
static __attribute__((noinline)) void ts_load_sound(uint32_t part, uint32_t sound)
{
    if (sound >= TS_INSTRUMENTS || !(ts_bank.sounds & (1u << sound))) return;
    track_t *t = &trk[part]; const ts_instrument_t *s = &ts_bank.instrument[sound];
    uint32_t any = 0;
    for (uint32_t v = 0; v < NVOICE; v++) any |= t->v[v].active;
    seq_release(t); trk_all_off(t); motion_restore(t);
    t->eng_req = s->engine; t->preset = s->preset; t->user = 0;
    for (uint32_t p = 0; p < P_COUNT; p++) if (ts_sound_param(p)) t->p[p] = s->p[p];
    fm6_unpack(s->fm6, fm6_patch[part]); fm6_adopt(part);
    if (any) { t->xf_on = 1; t->xf = XF_BLOCKS; }
    engine_block(t); motion_rebase(t);
    ts_run.current[part] = (uint8_t)sound;
}
static uint32_t ts_cache_hash(uint32_t part)
{
    return ts_hash(trk[part].step, TS_ROWS * sizeof(step_t)) ^ (uint32_t)trk[part].p[P_SLEN];
}
static uint32_t ts_patch_hash(uint32_t part)
{
    track_t *t = &trk[part]; uint32_t h = t->eng_req | (uint32_t)t->preset << 8;
    for (uint32_t p = 0; p < P_COUNT; p++) if (ts_sound_param(p)) h = (h ^ (uint16_t)motion_base_value(t, p)) * 16777619u;
    return ts_hash(fm6_patch[part], FP_SIZE) ^ h;
}
/* Flush steps before switching phrases/saving/playing. A phrase shared by
 * several parts is refreshed into unchanged caches after the edit commits. */
static uint32_t ts_motion_hash(uint32_t part)
{
    uint32_t h = (motion.on >> part) & 1u;
    for (uint32_t i = 0; i < motion.count; i++) if ((motion.event[i].place >> 6) == part)
        h = (h ^ ts_hash(&motion.event[i], sizeof motion.event[i])) * 16777619u;
    return h;
}
static int ts_bind_motion(void);
static int ts_sync(void)
{
    if (!ts_run.enabled || ts_run.mode >= TS_CHAIN || (song.playing && !ts_run.mode)) return 0;
    uint32_t f = motion_guard();
    for (uint32_t part = 0; part < NTRK; part++) {
        uint32_t h = ts_cache_hash(part), id = ts_bank.edit[part];
        if (h != ts_run.cache[part]) {
            memcpy(ts_bank.phrase[id].step, trk[part].step, sizeof ts_bank.phrase[id].step);
            ts_bank.phrase[id].length = (uint8_t)clamp(trk[part].p[P_SLEN], 1, TS_ROWS);
            ts_run.cache[part] = h;
        }
        if (!song.playing) {
            h = ts_patch_hash(part);
            if (h != ts_run.patch[part]) { ts_capture_sound(part, ts_bank.sound[part]); ts_run.patch[part] = h; }
        }
    }
    for (uint32_t part = 0; part < NTRK; part++) {
        const ts_phrase_t *p = &ts_bank.phrase[ts_bank.edit[part]];
        memcpy(trk[part].step, p->step, sizeof p->step);
        trk[part].p[P_SLEN] = p->length;
        ts_run.cache[part] = ts_cache_hash(part);
    }
    /* Only changed working views replace shared automation. An unchanged
     * second view of the same phrase must not overwrite the edited one. */
    uint8_t replace[NTRK] = {0}; uint32_t changed = 0, keep = 0, add = 0, expanded = 0;
    for (uint32_t k = 0; k < NTRK; k++) {
        replace[k] = ts_motion_hash(k) != ts_run.motion_cache[k]; changed |= replace[k];
    }
    if (!changed) { motion_unguard(f); return 0; }
    for (uint32_t k = 0; k < NTRK; k++) if (replace[k])
        for (uint32_t q = k + 1u; q < NTRK; q++) if (replace[q] && ts_bank.edit[q] == ts_bank.edit[k]) replace[k] = 0;
    for (uint32_t i = 0; i < ts_bank.count; i++) {
        uint32_t removed = 0, bindings = 0;
        for (uint32_t k = 0; k < NTRK; k++) {
            removed |= replace[k] && ts_bank.event[i].phrase == ts_bank.edit[k];
            bindings += ts_bank.event[i].phrase == ts_bank.edit[k];
        }
        keep += !removed; if (!removed) expanded += bindings;
    }
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i]; uint32_t k = e->place >> 6;
        if (!replace[k] || (e->place & 63u) >= TS_ROWS) continue;
        add++;
        for (uint32_t q = 0; q < NTRK; q++) expanded += ts_bank.edit[q] == ts_bank.edit[k];
    }
    if (keep + add > MOTION_MAX || expanded > MOTION_MAX) { motion_unguard(f); return 2; }
    uint32_t w = 0;
    for (uint32_t i = 0; i < ts_bank.count; i++) {
        uint32_t removed = 0;
        for (uint32_t k = 0; k < NTRK; k++) removed |= replace[k] && ts_bank.event[i].phrase == ts_bank.edit[k];
        if (!removed) ts_bank.event[w++] = ts_bank.event[i];
    }
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i]; uint32_t k = e->place >> 6;
        if (replace[k] && (e->place & 63u) < TS_ROWS)
            ts_bank.event[w++] = (ts_motion_t){ts_bank.edit[k], (uint8_t)(e->place & 63u), e->param, e->value};
    }
    ts_bank.count = (uint8_t)w;
    for (uint32_t k = 0; k < NTRK; k++) if (replace[k]) ts_bank.phrase[ts_bank.edit[k]].motion = (motion.on >> k) & 1u;
    ts_bind_motion();
    motion_unguard(f);
    return 0;
}
static int ts_bind_motion(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < ts_bank.count; i++) for (uint32_t k = 0; k < NTRK; k++) count += ts_bank.event[i].phrase == ts_bank.edit[k];
    if (count > MOTION_MAX) return 2;
    memset(&motion, 0, sizeof motion);
    for (uint32_t k = 0; k < NTRK; k++) {
        if (ts_bank.phrase[ts_bank.edit[k]].motion) motion.on |= (uint8_t)(1u << k);
        for (uint32_t i = 0; i < ts_bank.count; i++) if (ts_bank.event[i].phrase == ts_bank.edit[k]) {
            const ts_motion_t *e = &ts_bank.event[i];
            motion.event[motion.count++] = (motion_event_t){(uint8_t)(k * 64u + e->row), e->param, e->value};
        }
    }
    for (uint32_t k = 0; k < NTRK; k++) ts_run.motion_cache[k] = ts_motion_hash(k);
    return 0;
}
static int ts_select_phrase(uint32_t part, uint32_t id)
{
    if (song.playing || id >= TS_PHRASES || ts_sync()) return 1;
    uint32_t old = ts_bank.edit[part], f = motion_guard();
    if (!(ts_bank.phrases & (1u << id))) {
        ts_bank.phrase[id].sound = ts_bank.sound[part]; ts_bank.phrases |= 1u << id;
    }
    ts_bank.edit[part] = (uint8_t)id;
    if (ts_bind_motion()) { ts_bank.edit[part] = (uint8_t)old; motion_unguard(f); return 2; }
    memcpy(trk[part].step, ts_bank.phrase[id].step, sizeof ts_bank.phrase[id].step);
    memset(trk[part].step + TS_ROWS, 0, (NSTEP - TS_ROWS) * sizeof(step_t));
    trk[part].p[P_SLEN] = ts_bank.phrase[id].length;
    ts_bank.sound[part] = ts_bank.phrase[id].sound;
    ts_load_sound(part, ts_bank.sound[part]);
    ts_run.cache[part] = ts_cache_hash(part); ts_run.patch[part] = ts_patch_hash(part);
    motion_unguard(f); return 0;
}
/* Convert the working 64-step patterns losslessly into four phrase pages per
 * part. Stored legacy projects themselves remain untouched. */
static int ts_activate(void)
{
    if (ts_run.enabled) return 0;
    uint32_t f = motion_guard();
    memset(&ts_bank, 0, sizeof ts_bank); memset(&ts_run, 0, sizeof ts_run);
    memset(ts_section, 0, sizeof ts_section);
    ts_section[0] = (ts_section_t){16, 15};
    for (uint32_t p = 0; p < TS_PHRASES; p++) {
        ts_bank.phrase[p].length = TS_ROWS;
        memset(ts_bank.phrase[p].instrument, TS_NONE, TS_ROWS);
        for (uint32_t row = 0; row < TS_ROWS; row++) ts_bank.phrase[p].step[row].time = ST_REST;
    }
    memset(ts_bank.chain, TS_NONE, sizeof ts_bank.chain);
    for (uint32_t c = 0; c < TS_CHAINS; c++) for (uint32_t row = 0; row < TS_ROWS; row++) ts_bank.chain[c][row].transpose = 0;
    memset(ts_bank.order, TS_NONE, sizeof ts_bank.order); ts_bank.song_length = 1; ts_bank.phrases = 0xFFFFu;
    for (uint32_t k = 0; k < NTRK; k++) {
        ts_capture_sound(k, k); ts_bank.edit[k] = (uint8_t)(k * 4u); ts_bank.sound[k] = (uint8_t)k;
        uint32_t len = (uint32_t)clamp(trk[k].p[P_SLEN], 1, NSTEP);
        uint32_t populated = 0;
        for (uint32_t row = 0; row < NSTEP; row++) populated |= trk[k].step[row].n || trk[k].step[row].hit || trk[k].step[row].time == ST_TIE;
        for (uint32_t page = 0; page < 4u; page++) {
            ts_phrase_t *p = &ts_bank.phrase[k * 4u + page];
            memcpy(p->step, trk[k].step + page * TS_ROWS, sizeof p->step);
            for (uint32_t row = 0; row < TS_ROWS; row++) {
                p->gate[row] = (uint8_t)clamp(trk[k].p[P_SGATE], 1, 127);
                if (populated && p->step[row].time == ST_REST) p->off |= (uint16_t)(1u << row);
            }
            p->sound = (uint8_t)k; p->motion = (motion.on >> k) & 1u;
            p->length = (uint8_t)(len > page * TS_ROWS ? clamp((int32_t)len - (int32_t)page * TS_ROWS, 1, TS_ROWS) : TS_ROWS);
            if (page * TS_ROWS < len) ts_bank.chain[k][page].phrase = (uint8_t)(k * 4u + page);
        }
        ts_bank.order[0][k] = (uint8_t)k;
    }
    ts_bank.count = motion.count;
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        ts_bank.event[i] = (ts_motion_t){(uint8_t)((e->place >> 6) * 4u + (e->place & 63u) / TS_ROWS),
                                   (uint8_t)(e->place % TS_ROWS), e->param, e->value};
    }
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = ts_bank.phrase[ts_bank.edit[k]].length;
        memset(trk[k].step + TS_ROWS, 0, (NSTEP - TS_ROWS) * sizeof(step_t));
        ts_run.cache[k] = ts_cache_hash(k); ts_run.patch[k] = ts_patch_hash(k);
    }
    ts_bind_motion(); ts_run.enabled = 1;
    motion_unguard(f); return 0;
}
static int ts_arranging(void) { return ts_run.mode >= TS_CHAIN; }
static int ts_select_sound(uint32_t part, uint32_t id)
{
    if (song.playing || id >= TS_INSTRUMENTS || ts_sync()) return 1;
    uint32_t f = motion_guard();
    if (!(ts_bank.sounds & (1u << id))) {
        ts_bank.instrument[id] = ts_bank.instrument[ts_bank.sound[part]];
        ts_bank.sounds |= (uint16_t)(1u << id);
    }
    ts_bank.sound[part] = (uint8_t)id; ts_load_sound(part, id);
    ts_run.patch[part] = ts_patch_hash(part);
    motion_unguard(f); return 0;
}
static __attribute__((noinline)) const step_t *ts_steps(const track_t *t)
{
    uint32_t part = (uint32_t)(t - trk), id = ts_run.phrase[part];
    const step_t *steps = id == ts_bank.edit[part] ? t->step : ts_play_cache(part);
    return id < TS_PHRASES || (ts_run.gap & (1u << part)) ? steps : 0;
}
static __attribute__((noinline)) const ts_phrase_t *ts_play_phrase(const track_t *t)
{
    uint32_t part = (uint32_t)(t - trk);
    uint32_t id = ts_run.mode ? ts_run.phrase[part] : ts_bank.edit[part];
    return ts_run.enabled && id < TS_PHRASES ? &ts_bank.phrase[id] : 0;
}
static __attribute__((noinline)) int ts_find_link(uint32_t part, uint32_t chain, uint32_t start)
{
    if (chain >= TS_CHAINS) return 0;
    for (uint32_t r = start; r < TS_ROWS; r++) if (ts_bank.chain[chain][r].phrase < TS_PHRASES) {
        uint32_t gap = ts_run.mode >= TS_SONG && ts_section[ts_run.order[part]].beats && r > start;
        ts_run.gap = (uint8_t)((ts_run.gap & ~(1u << part)) | gap << part);
        if (gap) r = start;
        ts_run.chain_id[part] = (uint8_t)chain; ts_run.link[part] = (uint8_t)r;
        ts_run.phrase[part] = gap ? TS_NONE : ts_bank.chain[chain][r].phrase;
        ts_run.transpose[part] = ts_bank.chain[chain][r].transpose;
        /* seq_steps retains its 64-row contract for the UI/editor. Reuse the
         * mutually exclusive legacy arrangement scratch instead of adding RAM. */
        step_t *cache = ts_play_cache(part);
        if (gap) {
            seq_release(&trk[part]);
            memset(cache, 0, TS_ROWS * sizeof(step_t));
            for (uint32_t row = 0; row < TS_ROWS; row++) cache[row].time = ST_REST;
        } else memcpy(cache, ts_bank.phrase[ts_run.phrase[part]].step, TS_ROWS * sizeof(step_t));
        memset(cache + TS_ROWS, 0, (NSTEP - TS_ROWS) * sizeof(step_t));
        return 1;
    }
    return 0;
}
static __attribute__((noinline)) int ts_find_song_row(uint32_t start)
{
    for (uint32_t row = start; row < ts_bank.song_length; row++) {
        ts_run.active = ts_run.waiting = ts_run.gap = 0;
        for (uint32_t k = 0; k < NTRK; k++) {
            ts_run.order[k] = (uint8_t)row;
            if (ts_find_link(k, ts_bank.order[row][k], 0)) ts_run.active |= (uint8_t)(1u << k);
            else ts_run.phrase[k] = TS_NONE;
        }
        if (ts_run.active || ts_section[row].beats) return 1;
    }
    return 0;
}
static int ts_prepare(uint32_t mode, uint32_t part, uint32_t chain, uint32_t start)
{
    if (song.playing) return 1;
    if (ts_sync()) return 2;
    if (mode == TS_CHAIN && !ts_find_link(part, chain, start)) return 1;
    if (mode == TS_SONG || mode == TS_SECTION) {
        if (!ts_find_song_row(start)) return 1;
    }
    ts_run.pending = (uint8_t)mode; ts_run.part = (uint8_t)part;
    ts_run.chain = (uint8_t)chain; ts_run.start = (uint8_t)start;
    return 0;
}
static __attribute__((noinline)) void ts_start(void)
{
    if (!ts_run.enabled) return;
    ts_run.mode = ts_run.pending ? ts_run.pending : TS_ALL; ts_run.pending = 0;
    ts_run.active = ts_run.waiting = ts_run.gap = 0; ts_run.saved_rec = song.rec;
    if (ts_arranging()) song.rec = 0;
    if (ts_run.mode == TS_SONG || ts_run.mode == TS_SECTION) ts_find_song_row(ts_run.start);
    ts_run.clock_pos = ts_run.carry = ts_run.clock_row = 0; ts_run.clock_first = 1;
    for (uint32_t k = 0; k < NTRK; k++) {
        ts_run.length[k] = trk[k].p[P_SLEN]; ts_run.cycle[k] = 1; ts_run.current[k] = TS_NONE;
        if (ts_run.mode < TS_SONG) ts_run.transpose[k] = 0;
        int active = ts_run.mode == TS_ALL || (ts_run.mode == TS_PHRASE && k == ts_run.part);
        if (active) ts_run.phrase[k] = ts_bank.edit[k];
        if (ts_run.mode == TS_CHAIN) active = k == ts_run.part && ts_find_link(k, ts_run.chain, ts_run.start);
        if (ts_run.mode == TS_SONG || ts_run.mode == TS_SECTION) active = (ts_run.active >> k) & 1u;
        if (active) { ts_run.active |= (uint8_t)(1u << k); trk[k].p[P_SLEN] = ts_run.gap & (1u << k) ? TS_ROWS : ts_bank.phrase[ts_run.phrase[k]].length; }
        else ts_run.phrase[k] = TS_NONE;
    }
}
static __attribute__((noinline)) void ts_stop(void)
{
    if (!ts_run.mode) { ts_run.pending = 0; return; }
    uint32_t arranging = ts_arranging(); ts_run.mode = ts_run.pending = 0;
    if (arranging) song.rec = ts_run.saved_rec;
    for (uint32_t k = 0; k < NTRK; k++) {
        trk[k].p[P_SLEN] = ts_run.length[k]; ts_load_sound(k, ts_bank.sound[k]);
    }
}
/* One common boundary resets every participating track before seq_tick runs.
 * Carries the fractional block remainder into the next section's first row. */
static __attribute__((noinline)) int ts_next_section(uint32_t carry)
{
    uint32_t next = ts_run.mode == TS_SECTION ? ts_run.start : ts_run.order[0] + 1u;
    for (uint32_t k = 0; k < NTRK; k++) seq_release(&trk[k]);
    if (!ts_find_song_row(next)) { seq_stop(); return 0; }
    ts_run.clock_pos = ts_run.carry = carry; ts_run.clock_row = 0;
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k]; t->rat_left = 0;
        if (ts_run.active & (1u << k)) {
            t->p[P_SLEN] = ts_run.gap & (1u << k) ? TS_ROWS : ts_bank.phrase[ts_run.phrase[k]].length;
            t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1); t->seq_pos = 0x7FFFFFFFu;
            t->nd_late = t->nd_early = 0; t->nd_fired = 0; ts_run.cycle[k] = 1;
        }
    }
    return 1;
}
/* Main transport samples include the external MIDI clock's advance. This
 * counter is independent of track length, division, swing and waiting state. */
static __attribute__((noinline)) void ts_section_tick(uint32_t n)
{
    if (!song.playing || ts_run.mode < TS_SONG || !ts_section[ts_run.order[0]].beats) return;
    if (ts_run.clock_first) { ts_run.clock_first = 0; return; }
    uint32_t period = div_samples(2);
    ts_run.clock_pos += n;
    while (ts_run.clock_pos >= period) {
        ts_run.clock_pos -= period;
        if (++ts_run.clock_row >= (uint32_t)ts_section[ts_run.order[0]].beats * 4u) {
            if (!ts_next_section(ts_run.clock_pos)) return;
        }
    }
}
static __attribute__((noinline)) int ts_advance(uint32_t part)
{
    if (!ts_run.mode) return 1;
    ts_run.cycle[part] = 1;
    if (ts_run.mode <= TS_PHRASE) return 1;
    if (ts_find_link(part, ts_run.chain_id[part], ts_run.link[part] + 1u)) ts_run.cycle[part] = 0;
    else if (ts_run.mode == TS_CHAIN) ts_find_link(part, ts_run.chain, 0);
    else if (ts_section[ts_run.order[part]].beats && (ts_section[ts_run.order[part]].loop & (1u << part)) &&
             ts_find_link(part, ts_run.chain_id[part], 0)) { /* Repeat this track within the section. */ }
    else {
        ts_run.waiting |= (uint8_t)(1u << part); seq_release(&trk[part]);
        if (ts_section[ts_run.order[part]].beats) return 0;
        if (ts_run.waiting != ts_run.active) return 0;
        if (!ts_next_section(0)) return 0;
        if (!(ts_run.active & (1u << part))) return 0;
        trk[part].seq_pos = 0;
    }
    trk[part].p[P_SLEN] = ts_run.gap & (1u << part) ? TS_ROWS : ts_bank.phrase[ts_run.phrase[part]].length;
    return 1;
}
static __attribute__((noinline)) void ts_row_sound(track_t *t, uint32_t row)
{
    uint32_t k = (uint32_t)(t - trk), id = ts_run.phrase[k];
    if (!ts_run.mode || id >= TS_PHRASES) return;
    const ts_phrase_t *p = &ts_bank.phrase[id]; uint32_t sound = p->sound;
    for (uint32_t r = 0; r <= row && r < TS_ROWS; r++) if (p->instrument[r] < TS_INSTRUMENTS) sound = p->instrument[r];
    if (sound != ts_run.current[k]) ts_load_sound(k, sound);
}
static __attribute__((noinline)) const motion_store_t *ts_motion(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk), id = ts_run.phrase[k];
    if (id >= TS_PHRASES || !ts_bank.phrase[id].motion) return 0;
    motion_store_t *m = &ts_motion_scratch; m->count = 0; m->on = (uint8_t)(1u << k);
    for (uint32_t i = 0; i < ts_bank.count; i++) if (ts_bank.event[i].phrase == id) {
        const ts_motion_t *e = &ts_bank.event[i];
        m->event[m->count++] = (motion_event_t){(uint8_t)(k * 64u + e->row), e->param, e->value};
    }
    return m;
}
#else
static int ts_arranging(void) { return 0; }
#endif
