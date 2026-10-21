/* SPDX-License-Identifier: GPL-3.0-only */
/* Drive the real panel/UI and keyboard gate, not a second tracker model. */
#define FELUCCA_CORE 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void tk_keys(uint32_t mask)
{
    host_notes |= mask & ~fm1_in.notes;
    fm1_in.notes = mask;
    keyboard_block();
    frame();
}
static void tk_tap(uint32_t k) { tk_keys(1u << k); tk_keys(0); }
static void tk_combo(uint32_t modifier, uint32_t k)
{
    tk_keys(1u << modifier); tk_keys((1u << modifier) | (1u << k));
    tk_keys(1u << modifier); tk_keys(0);
}
static void tk_boot(void)
{
    ui_power_on();
    memset(&tracker, 0, sizeof tracker);
    memset(&tracker_draw, 0, sizeof tracker_draw);
    kb_prev = 0;
    open_family(FAM_SEQ);
    frame();
    assert(tracker_on() && song.grid == 4u && !song.playing);
    assert(cur_page()->graph == GR_TRACKER);
    /* Retain regression coverage of the core7 64-step compatibility path.
     * core_tracker_library_test exercises the active core8 library. */
    ts_run.enabled = 0;
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = 64;
}
int main(void)
{
    tk_boot();
    track_t *t = TSEL;
    /* Navigation is silent even when the ISR sees keys before the UI does. */
    for (uint32_t k = 0; k < 27; k++) {
        tk_keys(1u << k);
        assert(kb_note[k] == KB_SILENT && !kb_chn[k]);
        tk_keys(0);
        transport_req = 0;
        if (!tracker_on()) { open_family(FAM_SEQ); frame(); }
    }
    tk_boot(); t = TSEL;
    tk_tap(TK_DOWN); assert(ui.cursor == 1);
    tk_tap(TK_UP); assert(ui.cursor == 0);
    tk_combo(TK_SHIFT, TK_DOWN); assert(ui.cursor == 16 && ui.bank == 1);
    turn(EN_SELECT, -16); assert(ui.cursor == 0);
    tk_tap(TK_EDIT); assert(t->step[0].n == 1 && t->step[0].note[0] == 60);
    for (uint32_t n = 1; n < 4; n++) {
        tk_tap(TK_RIGHT); tk_combo(TK_EDIT, TK_UP);
        assert(t->step[0].n == n + 1);
    }
    step_t chord = t->step[0];
    assert(chord.note[0] == 60 && chord.note[1] == 72 && chord.note[2] == 72 && chord.note[3] == 72);
    tk_combo(TK_OPTION, TK_EDIT);
    assert(t->step[0].n == 3 && t->step[0].note[2] == 72);
    undo_swap(); assert(!memcmp(&chord, &t->step[0], sizeof chord));
    undo_swap(); assert(t->step[0].n == 3);
    tracker.col = 4; turn(EN_PRESET, 7); assert(t->step[0].vel == 103);
    tracker.col = 5; turn(EN_PRESET, 5); assert(step_cond(&t->step[0]) == 5);
    tracker.col = 6; turn(EN_PRESET, 2); assert(step_ratchet(&t->step[0]) == 3);
    tk_keys(1u << TK_EDIT); turn(EN_SELECT, 3); tk_keys(0);
    assert(note_len(t, 0) == 4 && t->step[1].time == ST_TIE && ui.cursor == 0);
    step_set_nudge(&t->step[0], -3);
    chord = t->step[0];
    tk_combo(TK_SHIFT, TK_OPTION);
    turn(EN_SELECT, 8); tk_combo(TK_SHIFT, TK_EDIT);
    assert(!memcmp(&t->step[8], &chord, sizeof chord));
    undo_swap(); assert(t->step[8].time == ST_REST);
    undo_swap(); assert(!memcmp(&t->step[8], &chord, sizeof chord));
    assert(project_save(0) == 0);
    step_clear(&t->step[0]); step_clear(&t->step[8]);
    project_load(0);
    assert(!memcmp(&t->step[0], &chord, sizeof chord) && !memcmp(&t->step[8], &chord, sizeof chord));
    puts("tracker: silent keypad, navigation, chords, chance/conditions, ratchets, length, copy/paste, undo, project roundtrip passed");

    tk_boot(); t = TSEL;
    press(B_REC); assert(tracker.notes && song.grid == 5 && !(song.rec & 1u));
    tk_keys((1u << 7) | (1u << 11) | (1u << 14));
    assert(t->step[0].n == 3 && ui.cursor == 0 && kb_note[7] != KB_SILENT);
    tk_keys(1u << 7); assert(ui.cursor == 0);
    tk_keys(0); assert(ui.cursor == 1);
    tk_combo(TK_EDIT, TK_OFF); assert(t->step[1].time == ST_TIE && ui.cursor == 2);
    tk_tap(TK_OFF); assert(t->step[2].time == ST_REST && ui.cursor == 3);
    press(B_REC); assert(song.rec & 1u); assert(!transport_req && !song.playing);
    /* Live recording must not write the edit cursor as well. */
    song.playing = 1; t->seq_idx = 10;
    step_t before = t->step[3]; tk_keys(1u << 7); tk_keys(0);
    assert(!memcmp(&before, &t->step[3], sizeof before));
    song.playing = 0; transport_req = 0;
    press(B_REC); assert(!tracker.notes && !(song.rec & 1u) && song.grid == 4u);
    puts("tracker: REC stages, three-note entry, release advance, tie/rest, live recording isolation passed");

    tk_boot(); t = TSEL; set_engine_of(t, ENGI_DRUM); t->engine = t->eng_req;
    frame();
    for (uint32_t n = 0; n < NLANE; n++) { tracker.col = (uint8_t)n; tk_tap(TK_EDIT); }
    assert(step_lanes(&t->step[0]) == 255 && t->step[0].n == 0);
    tracker.col = 2; tk_combo(TK_EDIT, TK_UP);
    assert(step_accents(&t->step[0]) == 4);
    tracker.col = 0; tk_combo(TK_OPTION, TK_EDIT);
    assert(step_lanes(&t->step[0]) == 254 && step_accents(&t->step[0]) == 4);
    tracker.col = 8; turn(EN_PRESET, 8); assert(t->step[0].vel == 104);
    assert(project_save(1) == 0);
    step_clear(&t->step[0]); project_load(1);
    assert(step_lanes(&t->step[0]) == 254 && step_accents(&t->step[0]) == 4 && t->step[0].vel == 104);
    puts("tracker: all eight simultaneous drum lanes, independent accents, velocity and project roundtrip passed");

    tk_boot(); t = TSEL; set_engine_of(t, ENGI_PSG); t->engine = t->eng_req;
    for (int mode = 0; mode < 2; mode++) {
        t->p[P_ED_FX] = mode;
        for (uint32_t k = 0; k < 4; k++) {
            uint32_t id = home_knob_id(t, k);
            const param_desc_t *d = track_desc(t, id);
            t->p[id] = d->min;
            turn(EN_K1 + k, 1);
            assert(t->p[id] > d->min);
        }
    }
    song.playing = 1; t->seq_idx = 35;
    press(B_HOME); frame(); assert(tracker.follow && ui.cursor == 35 && ui.bank == 2);
    tk_tap(TK_DOWN); assert(!tracker.follow && ui.cursor == 36);
    song.playing = 0;
    tk_combo(TK_OPTION, TK_RIGHT); assert(song.sel == 1);
    tk_combo(TK_SHIFT, TK_RIGHT); assert(cur_page()->fam == FAM_EDIT && !tracker_on() && song.grid == 0);
    puts("tracker: AY/Sega sound knobs, follow, manual override, channel selection and synth-page exit passed");

    tk_boot(); frame(); frames(2000);
    host_blit_rows = 0; frame(); assert(host_blit_rows == 0);
    tk_tap(TK_DOWN); assert(host_blit_rows <= 40);
    tracker_move(0);
    TSEL->step[0] = (step_t){.note={60},.n=1,.time=ST_NOTE,.vel=96};
    frame(); frames(2000);
    uint32_t footer = fnv(2166136261u, host_screen + 224u * 240u, 16u * 240u * sizeof(uint16_t));
    host_blit_rows = 0; TSEL->step[0].vel = 101; frame();
    assert(host_blit_rows == 28);
    assert(footer != fnv(2166136261u, host_screen + 224u * 240u, 16u * 240u * sizeof(uint16_t)));
    host_blit_rows = 0; TSEL->step[3].vel = 102; frame(); assert(host_blit_rows == 12);
    puts("tracker: idle draws nothing; selected-row values refresh the footer; other rows leave it unchanged");
    uint32_t rnd = 938147u;
    for (uint32_t i = 0; i < 5000u; i++) {
        rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5;
        int32_t delta = rnd & 1u ? 1 : -1;
        switch (rnd % 10u) {
        case 0: tk_tap(rnd & 1u ? TK_RIGHT : TK_LEFT); break;
        case 1: tk_combo(TK_EDIT, rnd & 1u ? TK_UP : TK_DOWN); break;
        case 2: tk_combo(TK_OPTION, TK_EDIT); break;
        case 3: turn(EN_PRESET, delta); break;
        case 4: turn(EN_SELECT, delta * 17); break;
        case 5: turn(EN_ALGO, delta); break;
        case 6: tk_combo(TK_SHIFT, rnd & 1u ? TK_OPTION : TK_EDIT); break;
        case 7: press(B_REC); break;
        case 8: tk_keys(1u << (5u + (rnd >> 8) % 16u)); tk_keys(0); break;
        case 9: TSEL->p[P_SLEN] = (int16_t)(1u + (rnd >> 8) % NSTEP); frame(); break;
        }
        assert(tracker_on() && !song.playing && !transport_req);
        assert(ui.cursor < (uint32_t)TSEL->p[P_SLEN] && tracker.col < tracker_ncol());
        for (uint32_t tr = 0; tr < NTRK; tr++) for (uint32_t row = 0; row < NSTEP; row++) {
            const step_t *st = &trk[tr].step[row];
            assert(st->n <= 4 && st->time <= ST_REST && st->vel <= 127 && st->probability <= 112);
        }
    }
    assert(project_save(2) == 0);
    puts("tracker: 5,000 mixed edits, rapid REC taps, track changes and external length changes remain bounded and saveable");
    return 0;
}
