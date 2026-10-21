/* SPDX-License-Identifier: MIT */
/* Compact wavetable scanning: five banks, four waves each, linear playback/morphing.
 * Four harmonic limits crossfaded at block rate. WARP tapers at high pitches;
 * this reduces aliasing without an oversampling buffer, but is not an alias-free guarantee. */
#include "wave_bank.h"
static int32_t wave_lookup(uint32_t w,uint32_t level,uint32_t ph)
{
    uint32_t i=ph>>24,j=(i+1u)&255u;
    int32_t a=WAVE_BANK[w][level][i],b=WAVE_BANK[w][level][j];
    return a+(((b-a)*(int32_t)((ph>>9)&32767u))>>15);
}
static void wave_note_on(track_t *t,voice_t *v)
{
    (void)t;
    v->s[0]=0;
    v->ph[2]=0;
}
static void wave_render(track_t *t,voice_t *v,int32_t *out,uint32_t n,const vmod_t *m)
{
    const int16_t *p=t->p;
    uint32_t ph=v->ph[0],sub=v->ph[1],motion=v->ph[2],i;
    int32_t scan=osc_sine(motion)*p[P_E3]/32768;
    int32_t pos=clamp(p[P_E1]+scan,0,127)*3*256/127;
    uint32_t bank=(uint32_t)p[P_E0];
    if(bank>=WAVE_NBANKS)bank=0;
    uint32_t w=bank*WAVE_WAVES_PER_BANK+(uint32_t)pos/256u;
    uint32_t next=w%4u==3u?w:w+1u,blend=(uint32_t)pos&255u;
    /* Harmonic room in Q8, capped before subsequent products. WARP fades out
     * above the upper octaves instead of folding a strongly distorted sine. */
    uint32_t room=m->inc?0x70000000u/((m->inc>>8)|1u):16384u;
    if(room>16384u)room=16384u;
    uint32_t warp=(uint32_t)p[P_E4]*(uint32_t)clamp((int32_t)room-512,0,1536)/1536u;
    uint32_t allowed=room*127u/(127u+3u*warp);
    allowed=allowed*((uint32_t)p[P_E6]+1u)/128u;
    static const uint16_t limits[]={8192,2048,512,256};
    uint32_t level=allowed>=8192u?0:allowed>=2048u?1:allowed>=512u?2:3;
    uint32_t lean_level=level<3u?level+1u:3u;
    /* Fade richer partials out BEFORE their harmonic limit reaches Nyquist.
     * Blending above the available harmonic count would itself cause aliasing. */
    uint32_t blend_level=(uint32_t)clamp(((int32_t)allowed-limits[level])*256/
                                      limits[level],0,256);
    int32_t lp=v->s[0],tone=clamp(3000+p[P_E7]*256+m->cutoff,3000,32767);
    for(i=0;i<n;i++) {
        uint32_t readph=ph;
        if(warp) readph+=(uint32_t)(osc_sine(ph)*(int32_t)warp)*128u;
        int32_t a=wave_lookup(w,level,readph),b=wave_lookup(next,level,readph);
        int32_t rich=a+((b-a)*(int32_t)blend>>8);
        a=wave_lookup(w,lean_level,readph);b=wave_lookup(next,lean_level,readph);
        int32_t lean=a+((b-a)*(int32_t)blend>>8);
        int32_t s=lean+((rich-lean)*(int32_t)blend_level>>8);
        s=mulq15(s,32767-p[P_E5]*128)+mulq15(osc_sine(sub),p[P_E5]*128);
        lp+=mulq15(s-lp,tone);
        out[i]+=voice_amp(lp,m,i);
        ph+=m->inc;sub+=m->inc>>1;
    }
    if(p[P_E2]) motion+=(uint32_t)p[P_E2]*(uint32_t)p[P_E2]*1024u;
    v->ph[0]=ph;v->ph[1]=sub;v->ph[2]=motion;v->s[0]=lp;
}
static const preset_t WAVE_PRESETS[]={
    {"MORPH PAD",{0,64,24,50,0,0,127,100},{45,90,100,75},0,0,FX(0,25,30,50),PAT(5)},
    {"GLASS WALK",{1,64,36,64,0,0,127,110},{0,80,100,60},0,0,FX(0,15,35,45),PAT(3)},
    {"WAVE SUB",{0,24,0,0,0,80,80,80},{0,70,100,35},0,1,FX(0,0,10,0),PAT(2)},
    {"PPG CHOIR",{2,84,14,28,0,0,102,92},{48,84,112,74},0,0,FX(0,26,24,46),PAT(5)},
    {"ESQ BRASS",{3,65,0,0,8,18,104,94},{7,63,90,38},0,0,FX(0,18,14,28),PAT(6)},
    {"VS AIR",{4,62,19,58,0,0,110,96},{72,94,116,84},0,0,FX(0,34,24,48),PAT(5)},
    {"PPG METAL",{2,116,24,42,26,0,124,112},{0,72,0,64},0,0,FX(0,10,24,38),PAT(7)},
    {"ESQ REED",{3,43,8,20,10,0,98,104},{3,70,103,38},0,1,FX(0,10,22,22),PAT(4)},
};
static const engine_t ENG_WAVE={
    .name="WAVE",.page_title={"SCAN","TONE"},
    .edit={{"BANK",F_ENUM,0,WAVE_NBANKS-1u,0,WAVE_BANK_NAMES,0},{"POS",F_PCT,0,127,64,0,0},
           {"RATE",F_PCT,0,127,0,0,0},{"SCAN",F_PCT,0,127,0,0,0},
           {"WARP",F_PCT,0,127,0,0,0},{"SUB",F_PCT,0,127,0,0,0},
           {"BRIT",F_PCT,0,127,127,0,0},{"TONE",F_PCT,0,127,127,0,0}},
    .presets=WAVE_PRESETS,.npresets=NELEM(WAVE_PRESETS),.knob={P_E0,P_E1,P_E2,P_E3},
    .keep=1,.note_on=wave_note_on,.render=wave_render,
};
