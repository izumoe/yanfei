/* SPDX-License-Identifier: GPL-3.0-only */
/* Five independent A/B objects: four projects and restore-last. Each copy is
 * four sectors, in the first two retired sample slots (A0000..C7FFF). The third
 * sample slot, user presets, FM6 banks, settings and OTA are untouched. */
#if FELUCCA_CORE
#define TS_STORE_BASE 0xA0000u
#define TS_STORE_COPY 0x4000u
#define TS_STORE_OBJECTS 5u
#define TS_STORE_MAGIC 0x54533831u
#define TS_DISK_BANK (sizeof(ts_bank_t) - TS_INSTRUMENTS * P_COUNT)
typedef struct { uint32_t magic, version, size, active; project_store_t project; uint8_t bank[TS_DISK_BANK]; ts_section_t section[TS_SONG_ROWS]; } ts_disk_t;
#define TS_DISK_V1_BYTES ((uint32_t)__builtin_offsetof(ts_disk_t, section))
static int ts_disk_length(uint32_t n) { return n == sizeof(ts_disk_t) || n == TS_DISK_V1_BYTES; }
typedef struct { uint32_t magic, object, copy, seq, length, crc, reserved, hcrc; } ts_head_t;
static ts_disk_t ts_disk __attribute__((aligned(4), section(".pool")));
static uint32_t ts_disk_gen;
_Static_assert(sizeof(ts_head_t) == 32 && sizeof(ts_disk_t) <= TS_STORE_COPY - 256u, "tracker flash extent");
#if !FELUCCA_FLASH
/* Host-only RAM backend, keeping the UI harness useful without a flash HAL. */
static uint8_t ts_host_flash[TS_STORE_OBJECTS * 2u * TS_STORE_COPY];
static int ts_host_read(uint32_t off, void *dst, uint32_t n)
{
    if (off < TS_STORE_BASE || off - TS_STORE_BASE > sizeof ts_host_flash || n > sizeof ts_host_flash - (off - TS_STORE_BASE)) return -1;
    memcpy(dst, ts_host_flash + off - TS_STORE_BASE, n); return 0;
}
static int ts_host_erase(uint32_t off) { memset(ts_host_flash + off - TS_STORE_BASE, 255, 4096); return 0; }
static int ts_host_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src; uint8_t *d = ts_host_flash + off - TS_STORE_BASE;
    while (n--) *d++ &= *s++; return 0;
}
#define TS_READ ts_host_read
#define TS_ERASE ts_host_erase
#define TS_PROG ts_host_prog
#else
#ifndef TS_READ
#define TS_READ st_read
#endif
#ifndef TS_ERASE
#define TS_ERASE st_erase
#endif
#ifndef TS_PROG
#define TS_PROG st_prog
#endif
#endif
static uint32_t ts_crc_add(uint32_t c, const void *data, uint32_t n)
{
    const uint8_t *p = data;
    while (n--) { c ^= *p++; for (uint32_t bit = 0; bit < 8; bit++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u))); }
    return c;
}
static uint32_t ts_crc(const void *p, uint32_t n) { return ~ts_crc_add(~0u, p, n); }
static uint32_t ts_address(uint32_t object, uint32_t copy) { return TS_STORE_BASE + (object * 2u + copy) * TS_STORE_COPY; }
static void ts_encode_bank(void)
{
    uint8_t *out = ts_disk.bank;
    uint32_t prefix = (uint32_t)__builtin_offsetof(ts_bank_t, instrument), suffix = (uint32_t)__builtin_offsetof(ts_bank_t, event);
    memcpy(out, &ts_bank, prefix); out += prefix;
    for (uint32_t i = 0; i < TS_INSTRUMENTS; i++) {
        const ts_instrument_t *s = &ts_bank.instrument[i];
        for (uint32_t p = 0; p < P_COUNT; p++) *out++ = (uint8_t)(int8_t)s->p[p];
        memcpy(out, s->fm6, FM6_PACKED); out += FM6_PACKED; *out++ = s->engine; *out++ = s->preset;
    }
    memcpy(out, (const uint8_t *)&ts_bank + suffix, sizeof ts_bank - suffix);
    memcpy(ts_disk.section, ts_section, sizeof ts_section);
}
static const uint8_t *ts_disk_instrument(uint32_t i)
{
    return ts_disk.bank + __builtin_offsetof(ts_bank_t, instrument) + i * (P_COUNT + FM6_PACKED + 2u);
}
static int ts_disk_valid(void)
{
    if (ts_disk.magic != TS_STORE_MAGIC ||
        !((ts_disk.version == 1 && ts_disk.size == TS_DISK_V1_BYTES) || (ts_disk.version == 2 && ts_disk.size == sizeof ts_disk)) || ts_disk.active > 1u ||
        !proj_import(&proj_scratch, &ts_disk.project, sizeof ts_disk.project)) return 0;
    if (ts_disk.version == 2) for (uint32_t r = 0; r < TS_SONG_ROWS; r++)
        if (ts_disk.section[r].beats > 16u || (ts_disk.section[r].loop & ~15u)) return 0;
    if (!ts_disk.active) return 1;
    uint32_t suffix = (uint32_t)__builtin_offsetof(ts_bank_t, event);
    uint32_t packed_suffix = suffix - TS_INSTRUMENTS * P_COUNT;
    const uint8_t *tail = ts_disk.bank + packed_suffix;
#define TS_FIELD(field, type) (*(const type *)(tail + __builtin_offsetof(ts_bank_t, field) - suffix))
    uint16_t sounds = TS_FIELD(sounds, uint16_t);
    uint32_t count = TS_FIELD(count, uint8_t), length = TS_FIELD(song_length, uint8_t);
    uint32_t used = TS_FIELD(phrases, uint32_t);
    if (!sounds || count > MOTION_MAX || !length || length > TS_SONG_ROWS) return 0;
    const uint8_t *edit = tail + __builtin_offsetof(ts_bank_t, edit) - suffix;
    const uint8_t *sound = tail + __builtin_offsetof(ts_bank_t, sound) - suffix;
    for (uint32_t k = 0; k < NTRK; k++) if (edit[k] >= TS_PHRASES || !(used & (1u << edit[k])) || sound[k] >= TS_INSTRUMENTS || !(sounds & (1u << sound[k]))) return 0;
    for (uint32_t i = 0; i < TS_INSTRUMENTS; i++) if (sounds & (1u << i)) {
        const uint8_t *s = ts_disk_instrument(i); uint32_t engine = s[P_COUNT + FM6_PACKED];
        if (engine >= NENGINES || !eng_ok(engine) || s[P_COUNT + FM6_PACKED + 1u] >= ENGINES[engine]->npresets) return 0;
        for (uint32_t p = 0; p < P_COUNT; p++) {
            const param_desc_t *d = param_desc_of(engine, p); int32_t v = (int8_t)s[p];
            if (v < d->min || v > d->max) return 0;
        }
        for (uint32_t b = 0; b < FM6_PACKED; b++) if (s[P_COUNT + b] > 127u) return 0;
    }
    const ts_phrase_t *phrases = (const ts_phrase_t *)ts_disk.bank;
    for (uint32_t i = 0; i < TS_PHRASES; i++) {
        const ts_phrase_t *p = &phrases[i];
        if (!p->length || p->length > TS_ROWS || p->sound >= TS_INSTRUMENTS || (used & (1u << i) && !(sounds & (1u << p->sound))) || p->motion > 1u) return 0;
        for (uint32_t r = 0; r < TS_ROWS; r++) {
            const step_t *s = &p->step[r];
            if (p->gate[r] > 128u || s->n > 4u || s->time > ST_REST || s->vel > 127u || s->probability > COND_FIRST + NCOND - 1u ||
                (s->acc & ~s->hit) || (p->instrument[r] != TS_NONE && (p->instrument[r] >= TS_INSTRUMENTS || !(sounds & (1u << p->instrument[r]))))) return 0;
            for (uint32_t n = 0; n < s->n; n++) if (s->note[n] > 127u) return 0;
        }
    }
    const ts_link_t *links = (const ts_link_t *)(ts_disk.bank + __builtin_offsetof(ts_bank_t, chain));
    for (uint32_t i = 0; i < TS_CHAINS * TS_ROWS; i++)
        if ((links[i].phrase != TS_NONE && (links[i].phrase >= TS_PHRASES || !(used & (1u << links[i].phrase)))) || links[i].transpose < -24 || links[i].transpose > 24) return 0;
    const uint8_t *order = ts_disk.bank + __builtin_offsetof(ts_bank_t, order);
    for (uint32_t i = 0; i < TS_SONG_ROWS * NTRK; i++) if (order[i] != TS_NONE && order[i] >= TS_CHAINS) return 0;
    const ts_motion_t *events = (const ts_motion_t *)tail;
    for (uint32_t i = 0; i < count; i++) if (events[i].phrase >= TS_PHRASES || !(used & (1u << events[i].phrase)) || events[i].row >= TS_ROWS || !motion_param(events[i].param & 127u)) return 0;
    uint32_t expanded = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (events[i].value < -64) return 0;
        for (uint32_t k = 0; k < NTRK; k++) expanded += events[i].phrase == edit[k];
        for (uint32_t q = 0; q < i; q++) if (events[i].phrase == events[q].phrase && events[i].row == events[q].row &&
            (events[i].param & 127u) == (events[q].param & 127u)) return 0;
    }
    if (expanded > MOTION_MAX) return 0;
#undef TS_FIELD
    return 1;
}
static void ts_upgrade_disk(void)
{
    if (ts_disk.version == 1) memset(ts_disk.section, 0, sizeof ts_disk.section);
    ts_disk.version = 2; ts_disk.size = sizeof ts_disk;
}
static void ts_decode_bank(void)
{
    const uint8_t *in = ts_disk.bank;
    uint32_t prefix = (uint32_t)__builtin_offsetof(ts_bank_t, instrument), suffix = (uint32_t)__builtin_offsetof(ts_bank_t, event);
    memset(&ts_bank, 0, sizeof ts_bank); memcpy(&ts_bank, in, prefix); in += prefix;
    for (uint32_t i = 0; i < TS_INSTRUMENTS; i++) {
        ts_instrument_t *s = &ts_bank.instrument[i];
        for (uint32_t p = 0; p < P_COUNT; p++) s->p[p] = (int8_t)*in++;
        memcpy(s->fm6, in, FM6_PACKED); in += FM6_PACKED; s->engine = *in++; s->preset = *in++;
    }
    memcpy((uint8_t *)&ts_bank + suffix, in, sizeof ts_bank - suffix);
    if (ts_disk.version == 2) memcpy(ts_section, ts_disk.section, sizeof ts_section);
    else memset(ts_section, 0, sizeof ts_section);
}
static int ts_read_copy(uint32_t object, uint32_t copy, ts_head_t *h)
{
    if (object >= TS_STORE_OBJECTS || copy > 1u) return 0;
    uint32_t base = ts_address(object, copy);
    if (TS_READ(base, h, sizeof *h) || h->magic != TS_STORE_MAGIC || h->object != object || h->copy != copy ||
        (h->length && !ts_disk_length(h->length)) || h->hcrc != ts_crc(h, sizeof *h - 4u)) return 0;
    if (!h->length) return 1;
    memset(&ts_disk, 0, sizeof ts_disk);
    if (TS_READ(base + 256u, &ts_disk, h->length) || h->crc != ts_crc(&ts_disk, h->length) || ts_disk.size != h->length) return 0;
    return ts_disk_valid();
}
/* Returns the newest fully validated copy, falling back from torn/newer data.
 * A committed zero-length record is a tombstone, not an empty legacy slot. */
static int ts_store_current(uint32_t object, ts_head_t *winner)
{
    ts_disk_gen++;
    ts_head_t a, b; int va = ts_read_copy(object, 0, &a), vb = ts_read_copy(object, 1, &b);
    int pick = vb && (!va || (int32_t)(b.seq - a.seq) > 0) ? 1 : va ? 0 : -1;
    if (pick >= 0) { *winner = pick ? b : a; if (!ts_read_copy(object, (uint32_t)pick, winner)) return -1; }
    return pick;
}
static int ts_store_write(uint32_t object, int previous, uint32_t sequence, uint32_t length)
{
    uint32_t copy = previous == 0 ? 1u : 0u, base = ts_address(object, copy);
    ts_head_t h = {TS_STORE_MAGIC, object, copy, sequence, length, length ? ts_crc(&ts_disk, length) : 0, 0, 0};
    uint8_t verify[256]; uint32_t crc = ~0u;
    for (uint32_t off = 0; off < TS_STORE_COPY; off += 4096u) if (TS_ERASE(base + off)) return 1;
    for (uint32_t off = 0; off < length; off += 256u) {
        uint32_t n = length - off > 256u ? 256u : length - off;
        if (TS_PROG(base + 256u + off, (const uint8_t *)&ts_disk + off, n) || TS_READ(base + 256u + off, verify, n)) return 1;
        crc = ts_crc_add(crc, verify, n);
    }
    if (length && ~crc != h.crc) return 1;
    h.hcrc = ts_crc(&h, sizeof h - 4u);
    if (TS_PROG(base, &h, sizeof h) || TS_READ(base, verify, sizeof h) || memcmp(verify, &h, sizeof h)) return 1;
    return 0;
}
static int ts_project_fetch(uint32_t slot)
{
    ts_head_t h; int which = ts_store_current(slot & 3u, &h);
    if (which < 0) return 0;
    if (!h.length) memset(&proj_slot[slot & 3u], 0, sizeof proj_slot[0]);
    else proj_slot[slot & 3u] = ts_disk.project;
    return 1;
}
static int ts_store_exists(uint32_t object)
{
    ts_head_t h; return ts_store_current(object, &h) >= 0;
}
static int ts_project_save_as(uint32_t object, const char *name)
{
    if (transport_busy() || ts_sync()) { ui_message("STOP / AUTOMATION FULL"); return 1; }
#if FELUCCA_FLASH
    if (!flash_ok) { ui_message("FLASH UNAVAILABLE"); return 2; }
#endif
    ts_head_t old; int previous = ts_store_current(object, &old);
    project_capture(&proj_scratch);
    if (name) { memset(proj_scratch.name, 0, sizeof proj_scratch.name); memcpy(proj_scratch.name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN); }
    proj_wire_gen++;
    if (!proj_pack(&proj_wire, &proj_scratch)) return 2;
    memset(&ts_disk, 0, sizeof ts_disk);
    ts_disk.magic = TS_STORE_MAGIC; ts_disk.version = 2; ts_disk.size = sizeof ts_disk; ts_disk.active = ts_run.enabled;
    ts_disk.project = proj_wire; if (ts_run.enabled) ts_encode_bank();
    if (!ts_disk_valid() || ts_store_write(object, previous, previous < 0 ? 1u : old.seq + 1u, sizeof ts_disk)) { ui_message("SAVE ERROR"); return 2; }
    if (object < 4u) {
        proj_slot[object] = ts_disk.project; proj_cur = (uint8_t)object; pat_from_set(object + 1u);
        proj_name_get(proj_name, (const uint8_t *)proj_scratch.name); ui_message("SAVED");
    }
    return 0;
}
static int ts_apply_disk(void)
{
    if (!ts_disk_valid()) return 1;
    /* Restore base first while stopped; the separate bank staging stays intact. */
    if (project_restore_runtime(&proj_scratch)) return 1;
    if (ts_disk.active) {
        ts_decode_bank(); memset(&ts_run, 0, sizeof ts_run); ts_run.enabled = 1;
        ts_bind_motion();
        for (uint32_t k = 0; k < NTRK; k++) {
            memcpy(trk[k].step, ts_bank.phrase[ts_bank.edit[k]].step, sizeof ts_bank.phrase[0].step);
            trk[k].p[P_SLEN] = ts_bank.phrase[ts_bank.edit[k]].length;
            ts_run.cache[k] = ts_cache_hash(k); ts_run.patch[k] = ts_patch_hash(k);
        }
    } else memset(&ts_run, 0, sizeof ts_run);
    tracker.view = 3; tracker.section = 1; tracker.page = 0; tracker.song_row = 0;
    ts_history.kind = 0; ui.force = 1;
    return 0;
}
static int ts_project_load(uint32_t object)
{
    ts_head_t h; int which = ts_store_current(object, &h);
    if (which < 0) return 0;
    if (!h.length) { if (object < 4u) ui_message("EMPTY SLOT"); return 1; }
    proj_wire_gen++;
    if (ts_apply_disk()) return 1;
    if (object < 4u) { proj_slot[object] = ts_disk.project; proj_cur = (uint8_t)object; pat_from_set(object + 1u); }
    return 1;
}
static int ts_project_rename(uint32_t slot, const char *name)
{
    ts_head_t h; int previous = ts_store_current(slot, &h);
    if (previous < 0 || !h.length) { ui_message("EMPTY SLOT"); return 1; }
    ts_upgrade_disk();
    memset(proj_scratch.name, 0, sizeof proj_scratch.name);
    memcpy(proj_scratch.name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    proj_wire_gen++;
    if (!proj_pack(&ts_disk.project, &proj_scratch) || ts_store_write(slot, previous, h.seq + 1u, sizeof ts_disk)) {
        ui_message("SAVE ERROR"); return 2;
    }
    proj_slot[slot] = ts_disk.project;
    if (proj_cur == slot) proj_name_get(proj_name, (const uint8_t *)proj_scratch.name);
    ui_message("RENAMED"); return 0;
}
static int ts_project_erase(uint32_t slot)
{
    ts_head_t h; int previous = ts_store_current(slot, &h);
    if (previous < 0) return 0;
    if (ts_store_write(slot, previous, h.seq + 1u, 0)) return 2;
    memset(&proj_slot[slot], 0, sizeof proj_slot[0]); if (proj_cur == slot) proj_cur = PROJ_NO_SLOT;
    ui_message("PROJECT ERASED"); return 1;
}
#endif
