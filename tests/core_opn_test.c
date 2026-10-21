/* SPDX-License-Identifier: GPL-3.0-only */
/* Chip wrappers: musical pitch, envelopes, mode changes and part isolation. */
#define FELUCCA_CORE 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static void run_blocks(uint32_t n) { int32_t out[CTL*2];while(n--)mix_block(out,CTL); }
static track_t *sound(uint32_t e,uint32_t p)
{
    ui_power_on();song.master_q12=4096;
    set_engine_of(&trk[0],e);apply_preset_to(&trk[0],p);run_blocks(64);
    return &trk[0];
}
static void ym_sine(track_t *t)
{
    t->p[P_E0]=7;t->p[P_E1]=t->p[P_E2]=t->p[P_E3]=t->p[P_E4]=t->p[P_E5]=t->p[P_E6]=0;t->p[P_E7]=127;
    t->p[P_FTYPE]=0;t->p[P_ED_FX]=0;
    for(uint32_t op=0;op<4;op++) {
        t->p[P_LN0+op]=1;t->p[P_LN4+op]=0;
        uint32_t id=P_FM1_ATK+op*5u;
        t->p[id]=31;t->p[id+1]=0;t->p[id+2]=0;t->p[id+3]=15;t->p[id+4]=op?127:0;
    }
}
static uint32_t sine_crossings(track_t *t,uint32_t mul)
{
    t->p[P_LN0]=(int16_t)mul;
    voice_t *v=&t->v[0];voice_start(t,v,69,100,0);
    vmod_t m={.inc=pitch_inc(69*16),.pitch16=69*16,.amp0=32767,.amp1=32767,.shape=64<<8};
    int32_t last=0;uint32_t crosses=0;
    for(uint32_t b=0;b<FS/CTL+32u;b++) {
        int32_t out[CTL]={0};assert(!ym_done(t,v));ym_render(t,v,out,CTL,&m);
        for(uint32_t i=0;i<CTL;i++){if(b>=32 && last<=0 && out[i]>0)crosses++;last=out[i];}
    }
    v->gate=0;
    for(uint32_t b=0;b<1000 && !ym_done(t,v);b++){int32_t out[CTL]={0};ym_render(t,v,out,CTL,&m);}
    assert(ym_done(t,v));return crosses;
}
static void psg_home_controls(void)
{
    static const uint8_t ids[2][4]={{P_E0,P_E3,P_E4,P_E5},{P_E0,P_E3,P_E4,P_E7}};
    static const char *labels[2][4]={{"TONE2","N-MIX","BUZZ","B-RATE"},{"TONE2","N-MIX","NOISE","TONE"}};
    /* Enum values have five display characters; no truncated noise/hold names. */
    for(uint32_t i=0;i<8;i++)assert(strlen(PSG_SHAPES[i])<=5 && strlen(PSG_SEGA_NOISE[i])<=5);
    for(uint32_t mode=0;mode<2;mode++) {
        track_t *t=sound(ENGI_PSG,mode?2:0);
        go_home();ui_home_view=HV_SCOPE;ui.plk_src=0;
        for(uint32_t k=0;k<4;k++) {
            int16_t *vp,was[P_COUNT];
            const param_desc_t *d=home_param(k,&vp);
            uint32_t id=ids[mode][k];
            assert(vp==&t->p[id] && !strcmp(d->label,labels[mode][k]));
            assert(lock_id(k)==id);
            memcpy(was,t->p,sizeof was);
            ui.ml.on=1;turn(EN_K1+k,*vp==d->max?-1:1);
            assert(t->p[id]!=was[id] && ui.ml.pick && ui.ml.id==id && ui.ml.trk==0);
            for(uint32_t i=0;i<P_COUNT;i++)if(i!=id)assert(t->p[i]==was[i]);
            ml_off();
        }
        /* Switching profiles changes the map immediately, including step locks. */
        t->p[P_ED_FX]=(int16_t)!mode;
        int16_t *vp;
        home_param(3,&vp);assert(vp==&t->p[ids[!mode][3]] && lock_id(3)==ids[!mode][3]);
        /* The mixer-style HOME view still controls the four part levels. */
        ui_home_view=HV_LEVELS;
        for(uint32_t k=0;k<4;k++){home_param(k,&vp);assert(vp==&trk[k].p[P_LEVEL]);}
    }
    /* Engines without a mode hook retain their existing HOME map. */
    for(uint32_t n=0;n<NENG_SHOWN;n++) {
        uint32_t e=eng_vis(n);trk[0].eng_req=(uint8_t)e;
        if(e!=ENGI_PSG)for(uint32_t k=0;k<4;k++)assert(home_knob_id(&trk[0],k)==ENGINES[e]->knob[k]);
    }
    puts("PSG HOME: both modes, physical knob edits, labels, MIDI learn, step-lock IDs and LEVELS view passed");
}
int main(void)
{
    track_t *t=sound(ENGI_YM,0);
    ym_sine(t);
    uint32_t c=sine_crossings(t,1);assert(c>=438 && c<=442);
    c=sine_crossings(t,0);assert(c>=218 && c<=222); /* MULT 0 is half rate */
    c=sine_crossings(t,2);assert(c>=878 && c<=882);
    /* Generic ADSR release must not terminate a chip's slow operator release. */
    ym_sine(t);t->p[P_REL]=0;t->p[P_FM1_REL]=0;
    trk_note_on(t,69,100);run_blocks(32);trk_note_off(t,69);run_blocks(1);
    assert(t->v[0].active && !t->v[0].gate);
    run_blocks(4096); /* next factory load resets this deliberately long tail */
    clean_engine_of(t);
    for(uint32_t i=0;i<P_COUNT;i++) {
        const param_desc_t *d=param_desc_of(ENGI_YM,i);
        assert(t->p[i]>=d->min && t->p[i]<=d->max);
    }
    /* Exercise every route and SSG shape through real chip envelopes, including
     * release from inverted/held stages and the maximum feedback setting. */
    for(uint32_t alg=0;alg<8;alg++)for(uint32_t ssg=0;ssg<=8;ssg++) {
        apply_preset_to(t,2);t->p[P_E0]=(int16_t)alg;t->p[P_E3]=(int16_t)ssg;t->p[P_E1]=7;
        for(uint32_t op=0;op<4;op++)t->p[P_FM1_REL+op*5u]=15;
        voice_t *v=&t->v[0];voice_start(t,v,60,127,0);
        vmod_t m={.inc=pitch_inc(60*16),.pitch16=60*16,.amp0=32767,.amp1=32767,.shape=64<<8};
        uint32_t b;
        for(b=0;b<2000;b++) {
            if(b==1000)v->gate=0;
            if(ym_done(t,v))break;
            int32_t out[CTL]={0};ym_render(t,v,out,CTL,&m);
            for(uint32_t i=0;i<CTL;i++)assert(out[i]>-65536 && out[i]<65536);
            ym_voice_t *y=ym_voice_of(t,v);
            for(uint32_t op=0;op<4;op++)assert(y->chip.ch[0].op[op].env<=1023);
        }
        assert(b<2000 && !v->gate);
    }
    /* Shared workspace capacity and distinct instances, including native buffers. */
    assert(ENG_YM.mem<=ENG_MEM_PART && ENG_PSG.mem<=ENG_MEM_PART);
    assert(ym_voice_of(t,&t->v[0])!=ym_voice_of(t,&t->v[1]));
    t=sound(ENGI_PSG,0);t->p[P_ED_FX]=1;
    voice_t *v=&t->v[0];voice_start(t,v,69,100,0);
    vmod_t m={.inc=pitch_inc(69*16),.pitch16=69*16,.amp0=32767,.amp1=32767};
    for(uint32_t mode=0;mode<8;mode++) {
        t->p[P_E4]=(int16_t)mode;t->p[P_E2]=0;
        int32_t out[CTL]={0};psg_render(t,v,out,CTL,&m);
        segapsg_t *s=psg_sega_of(t,v);
        assert(s->reg[0]>=253 && s->reg[0]<=255); /* A4, real 10-bit divider */
        assert(s->reg[6]==(int32_t)((mode&3u)|(mode<4u?4u:0u)));
        int32_t period=s->period[3];t->p[P_E2]=31;psg_render(t,v,out,CTL,&m);
        if((mode&3u)!=3u)assert(s->period[3]==period);
        else assert(s->period[3]>period && s->reg[4]<=1023);
    }
    t->p[P_E5]=0;memset(v->s,0,sizeof v->s); /* silence plus mode reinitialization */
    int32_t out[CTL]={0};psg_render(t,v,out,CTL,&m);
    for(uint32_t i=0;i<CTL;i++)assert(out[i]==0);
    t->p[P_ED_FX]=0;psg_render(t,v,out,CTL,&m);assert(!v->s[6]);
    assert(!strcmp(track_desc(t,P_E5)->label,"B-RATE"));
    t->p[P_ED_FX]=1;assert(!strcmp(track_desc(t,P_E5)->label,"VOL"));
    psg_home_controls();
    puts("OPN instruments: pitch/multipliers, 72 algorithm/SSG releases, bounded output, Sega registers/clocks/mute and AY switching passed");
}
