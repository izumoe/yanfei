/* SPDX-License-Identifier: GPL-3.0-only */
/* Real audio ISR, engine handoffs and shared voice budget under row sound changes. */
#define FELUCCA_CORE 1
#include "../web/emu/felucca_web.c"
#include <assert.h>
#include <math.h>
int main(void)
{
    web_nor_erase(); web_boot(); web_master(1023);
    for (uint32_t n=0;n<44100;n+=128) web_render(128);
    assert(!ts_activate()); memset(ts_section, 0, sizeof ts_section);
    for (uint32_t id=0;id<NENG_SHOWN;id++) {
        set_engine_of(&trk[0],eng_vis(id)); apply_preset_to(&trk[0],3); ts_capture_sound(0,id);
    }
    for (uint32_t k=0;k<NTRK;k++) {
        uint32_t id=ts_bank.edit[k]; ts_phrase_t *p=&ts_bank.phrase[id];
        for(uint32_t r=0;r<TS_ROWS;r++) {
            uint32_t sound=(r+k*3u)%NENG_SHOWN; step_t *s=&p->step[r];
            *s=(step_t){.note={48,55,60,64},.n=4,.time=ST_NOTE,.vel=96};
            if(ts_bank.instrument[sound].engine==ENGI_DRUM) {s->hit=255;s->n=0;s->acc=85;}
            p->instrument[r]=(uint8_t)sound;p->gate[r]=r&1u?48:0;
            step_set_ratchet(s,1+r%4u);
        }
        memcpy(trk[k].step,p->step,sizeof p->step); ts_load_sound(k,k);
        ts_run.cache[k]=ts_cache_hash(k);ts_run.patch[k]=ts_patch_hash(k);
    }
    assert(!project_save_as(0,"HANDOFF"));
    double peak=0;uint32_t starts=0,rails=0;
    for(uint32_t block=0;block<6200;block++) {
        if(!song.playing) {assert(!ts_prepare(TS_SONG,0,0,0));seq_start();starts++;}
        web_render(128);
        if(voices_busy()>NVOICE) {
            fprintf(stderr,"block %u voices %u\n",block,voices_busy());
            for(uint32_t k=0;k<NTRK;k++) {fprintf(stderr,"part %u engine %u req %u xf %u/%u queue %u:",k,trk[k].engine,trk[k].eng_req,trk[k].xf_on,trk[k].xf,trk[k].xp_n);
                for(uint32_t v=0;v<NVOICE;v++)if(trk[k].v[v].active)fprintf(stderr," %u:%u/%u",v,trk[k].v[v].note,trk[k].v[v].stage);fprintf(stderr,"\n");}
            assert(voices_busy()<=NVOICE);
        }
        for(uint32_t k=0;k<NTRK;k++) assert(trk[k].xp_n<=NELEM(trk[k].xp_note));
        for(uint32_t i=0;i<128;i++) {
            double a=fabs(out_l[i]),b=fabs(out_r[i]); assert(isfinite(a)&&isfinite(b));
            if(a>peak)peak=a;if(b>peak)peak=b;rails+=a>=32767.0/32768.0 || b>=32767.0/32768.0;
        }
    }
    seq_stop(); for(uint32_t n=0;n<22050;n+=128) web_render(128);
    for(uint32_t k=0;k<NTRK;k++) assert(!trk[k].seq_n && !trk[k].xp_n);
    assert(starts>=2 && peak>0 && peak<1 && !rails);
    printf("tracker audio: %u sections, all 12 engines, row instruments/chords/eight-lane drums, no stuck queues, voices<=8, peak %.6f, zero rails\n",starts,peak);
}
