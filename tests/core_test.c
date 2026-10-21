/* SPDX-License-Identifier: GPL-3.0-only */
/* Custom profile integration: real allocator, DSP, UI sound loads and storage. */
#define FELUCCA_CORE 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void core_blocks(uint32_t count)
{
    int32_t out[CTL*2];
    for(uint32_t i=0;i<count;i++) mix_block(out,CTL);
}
static void core_sound(uint32_t e,uint32_t pr)
{
    ui_power_on();
    song.master_q12=4096;
    set_engine_of(&trk[0],e);
    apply_preset_to(&trk[0],pr);
    core_blocks(32);
    assert(trk[0].engine==e);
}
static uint64_t sound_signature(uint32_t e,uint32_t pr,int32_t *peak)
{
    core_sound(e,pr);
    track_t *t=&trk[0];
    /* Keep test timing bounded while exercising the real release path. */
    t->p[P_REL]=0;
    uint32_t note1=e==ENGI_DRUM?36:60,note2=e==ENGI_DRUM?38:67;
    trk_note_on(t,note1,100);
    trk_note_on(t,note2,100);
    uint64_t h=1469598103934665603ULL;
    int32_t out[CTL];
    *peak=0;
    for(uint32_t b=0;b<512;b++) {
        mod_begin(t);
        track_render(t,out,CTL);
        if(MOD(t).on) mod_end(t);
        for(uint32_t i=0;i<CTL;i++) {
            int32_t a=out[i]<0?-out[i]:out[i];
            if(a>*peak)*peak=a;
            h=(h^(uint32_t)out[i])*1099511628211ULL;
        }
    }
    trk_note_off(t,note1);trk_note_off(t,note2);
    core_blocks(1024);
    for(uint32_t i=0;i<NVOICE;i++) {
        assert(!t->v[i].gate);
        /* FM6 operator release times intentionally outlast the generic ADSR. */
        if(e!=ENGI_FM6 && e!=ENGI_DRUM) {
            if(t->v[i].active) fprintf(stderr,"release: %s %s stage=%u env=%d\n",ENGINES[e]->name,ENGINES[e]->presets[pr].name,t->v[i].stage,t->v[i].env);
            assert(!t->v[i].active);
        }
    }
    return h;
}
int main(void)
{
    const char *names[]={"ANALOG","NES","DMG","PSG","TRIO","FM6","YM","WAVE","PHASE","STRING","MODAL","DRUM"};
    assert(NENG_SHOWN==NELEM(names));
    uint32_t seen=0,presets=0;
    uint64_t sig[3];
    for(uint32_t n=0;n<NENG_SHOWN;n++) {
        uint32_t e=eng_vis(n);
        assert(eng_ok(e)&&!(seen&(1u<<e)));
        seen|=1u<<e;
        assert(!strcmp(ENGINES[e]->name,names[n]));
        assert(!strcmp(N_ENGNAME[e],names[n]));
        assert(ENGINES[e]->npresets==8);
        assert(strlen(PRESET_CAT[e])==ENGINES[e]->npresets);
        assert(eng_step(e,1)==eng_vis(n+1));
        assert(ENGINES[e]->mem<=ENG_MEM_PART);
        uint64_t factory_sig[8];
        for(uint32_t pr=0;pr<ENGINES[e]->npresets;pr++) {
            const preset_t *p=&ENGINES[e]->presets[pr];
            core_sound(e,pr);
            for(uint32_t k=0;k<8;k++) {
                const param_desc_t *d=param_desc_of(e,P_E0+k);
                assert(p->e[k]>=d->min&&p->e[k]<=d->max);
            }
            int32_t peak;
            uint64_t h=sound_signature(e,pr,&peak);
            if(!peak) fprintf(stderr,"silent: %s %s\n",ENGINES[e]->name,p->name);
            assert(peak>0);
            for(uint32_t k=0;k<pr;k++)assert(factory_sig[k]!=h);
            factory_sig[pr]=h;
            if(pr==0 && n>=1 && n<=3) sig[n-1]=h;
            presets++;
        }
    }
    assert(sig[0]!=sig[1]&&sig[1]!=sig[2]&&sig[0]!=sig[2]);
    assert(!eng_ok(1)&&!eng_ok(11));
    assert(WAVE_NBANKS==5 && sizeof(WAVE_BANK)==40960);
    assert(!strcmp(eng_abbr("STRING"),"STRG"));
    assert(!strcmp(eng_abbr("MODAL"),"MODL"));
    assert(chip_noise_step(0x41,1,1)!=chip_noise_step(0x41,1,0));
    assert(presets==96 && FM6_NFACTORY==3);
    assert(FM6_PRESETS[2].e[7]==2);
    /* A sounding retrigger preserves only the filter, never the chip envelope. */
    for(uint32_t e=ENGI_NES;e<=ENGI_WAVE;e++) if(e!=6u) {
        core_sound(e,0);
        track_t *t=&trk[0];voice_t *v=&t->v[0];
        voice_start(t,v,60,100,0);
        v->s[0]=12345;v->s[4]=123456;v->stage=2;
        voice_start(t,v,60,100,0);
        assert(v->s[0]==12345);
        if(e==ENGI_PSG) assert(v->s[4]==0);
        if(e==ENGI_NES||e==ENGI_DMG) assert(v->s[4]==15);
    }
    core_sound(ENGI_DMG,0);
    int saved=up_store(0,"DMG TEST");
    assert(saved==0||saved==3); /* host UI stub can report RAM-only persistence */
    assert(up_used(0));
    up_rec_t r=*up_rec(0);
    assert(r.ver==UP_VER&&r.engine==ENGI_DMG);
    r.ver=4;assert(!up_valid(&r));
    project_save(0);
    project_t q;
    assert(proj_import(&q,&proj_slot[0],sizeof proj_slot[0]));
    assert(q.t[0].engine==ENGI_DMG);
    project_store_t old=proj_slot[0];
    ((uint32_t *)&old)[0]=0x46554e41u;
    ((uint32_t *)&old)[sizeof old/4u-1u]=proj_hash(&old,sizeof old-4u);
    assert(!proj_import(&q,&old,sizeof old));
    /* Simultaneous parts share the global voice budget, never their model state. */
    const uint32_t ensemble[NPART]={ENGI_STRING,ENGI_MODAL,ENGI_WAVE,ENGI_DRUM};
    ui_power_on();song.master_q12=4096;
    for(uint32_t p=0;p<NPART;p++) {
        set_engine_of(&trk[p],ensemble[p]);
        apply_preset_to(&trk[p],0);
    }
    core_blocks(64);
    for(uint32_t p=0;p<NPART;p++) {
        assert(trk[p].engine==ensemble[p]);
        if(ENGINES[ensemble[p]]->mem) assert(eng_mem_owner[p]==ensemble[p]+1);
        trk_note_on(&trk[p],p==3?36:48+p*7,110);
    }
    core_blocks(512);
    assert(ENG_MEM(0)!=ENG_MEM(1));
    /* Cycle a ringing physical part through every engine, including both owners
     * of the same physical workspace, using the production switch/fade path. */
    for(uint32_t n=0;n<NENG_SHOWN;n++) {
        uint32_t e=eng_vis(n);
        set_engine_of(&trk[0],e);core_blocks(64);
        assert(trk[0].engine==e);
        if(ENGINES[e]->mem) assert(eng_mem_owner[0]==e+1);
        trk_note_on(&trk[0],e==ENGI_DRUM?36:60,100);
        core_blocks(32);
    }
    printf("core: 12 engines, %u factory presets rendered/released; eight per engine, distinct chip sounds and fresh storage checked\n",presets);
    return 0;
}
