/* SPDX-License-Identifier: MIT */
#define main reference_host_main
#include "fe_host.c"
#undef main
#define ENG_MEM_PART 4096u
static uint64_t chip_mem[NPART][ENG_MEM_PART/8u];
#define ENG_MEM(p) ((void *)chip_mem[p])
#define ENG_MEM_FITS(name,size) _Static_assert((size)<=ENG_MEM_PART,name " too large")
#include "../src/eng_nes.c"
#include "../src/eng_dmg.c"
#include "../src/eng_psg.c"
#include "../src/eng_wave.c"
#include <assert.h>
#include <stdlib.h>
#include <math.h>

static void defaults(track_t *t,const engine_t *e)
{
    memset(t,0,sizeof *t);
    for(uint32_t i=0;i<8;i++)t->p[P_E0+i]=e->edit[i].def;
}
/* Hann-windowed projection onto folded harmonics. The high-note case catches
 * selection/crossfades that accidentally admit tables above their safe bandwidth. */
static double spectral_power(const int32_t *x,uint32_t n,double cycles)
{
    double re=0,im=0;
    for(uint32_t k=0;k<n;k++) {
        double a=6.283185307179586*k,window=.5-.5*cos(a/(n-1));
        re+=x[k]*window*cos(a*cycles);im+=x[k]*window*sin(a*cycles);
    }
    return re*re+im*im;
}
static void high_wave_spectrum(track_t *t,voice_t *v,uint32_t bank,uint32_t warp)
{
    static int32_t samples[8192];
    defaults(t,&ENG_WAVE);t->p[P_E0]=(int16_t)bank;t->p[P_E1]=127;t->p[P_E4]=(int16_t)warp;
    voice_on(t,&ENG_WAVE,v,117,0);
    vmod_t m={.pitch16=117*16,.inc=pitch_inc(117*16),.amp0=32767,.amp1=32767};
    for(uint32_t b=0;b<128+NELEM(samples)/CTL;b++) {
        int32_t out[CTL]={0};wave_render(t,v,out,CTL,&m);
        if(b>=128)memcpy(samples+(b-128)*CTL,out,sizeof out);
    }
    double f=m.inc/4294967296.0,base=spectral_power(samples,NELEM(samples),f);
    assert(base>1);
    for(uint32_t h=4;h<=12;h++)
        assert(spectral_power(samples,NELEM(samples),f*h)<base*1e-6); /* -60 dB */
}
int main(void)
{
    track_t *t=&trk[0]; voice_t *v=&t->v[0];
    /* The short and long handheld noise streams have different periods. */
    uint32_t l=0x7fff,short_start=0;
    for(uint32_t i=0;i<16;i++)l=chip_noise_step(l,1,1);
    short_start=l&127u;
    for(uint32_t i=0;i<127;i++)l=chip_noise_step(l,1,1);
    assert((l&127u)==short_start);
    l=0x7fff;for(uint32_t i=0;i<32767;i++)l=chip_noise_step(l,0,1);
    assert(l==0x7fff);
    defaults(t,&ENG_NES);t->p[P_E5]=1;
    voice_on(t,&ENG_NES,v,60,0);
    for(uint32_t i=0;i<100;i++)nes_amp(t,v,32767);
    assert(v->s[4]==0);
    defaults(t,&ENG_DMG);t->p[P_E5]=1;
    voice_on(t,&ENG_DMG,v,60,0);
    for(uint32_t i=0;i<100;i++)dmg_amp(t,v,32767);
    assert(v->s[4]>0); /* handheld envelope clock is slower */
    defaults(t,&ENG_NES);t->p[P_E5]=17;
    voice_on(t,&ENG_NES,v,60,0);
    for(uint32_t i=0;i<300;i++)nes_amp(t,v,32767);
    assert(v->s[4]>0);
    assert(nes_amp(t,v,0)==0); /* note-off cannot leave a looping envelope audible */
    /* D7's first step waits its full divider, just like later steps. */
    defaults(t,&ENG_NES);t->p[P_E5]=8;
    voice_on(t,&ENG_NES,v,60,0);
    for(uint32_t i=0;i<45;i++)nes_amp(t,v,32767);
    assert(v->s[4]==15);
    for(uint32_t i=0;i<2;i++)nes_amp(t,v,32767);
    assert(v->s[4]==14);
    defaults(t,&ENG_DMG);t->p[P_E5]=8;
    voice_on(t,&ENG_DMG,v,60,0);
    for(uint32_t i=0;i<170;i++)dmg_amp(t,v,32767);
    assert(v->s[4]==15);
    for(uint32_t i=0;i<3;i++)dmg_amp(t,v,32767);
    assert(v->s[4]==14);
    for(uint32_t pos=0;pos<16;pos++)
        assert(psg_env_level(3,pos,0)+psg_env_level(4,pos,0)==15);
    defaults(t,&ENG_PSG);
    voice_on(t,&ENG_PSG,v,60,0);
    v->s[4]=123456;v->s[3]=1;v->stage=2;v->active=1;
    voice_on(t,&ENG_PSG,v,60,0);
    assert(v->s[4]==0&&v->s[3]==0); /* buzzer envelope restarts even on a sounding retrigger */
    const engine_t *engs[]={&ENG_NES,&ENG_DMG,&ENG_PSG,&ENG_WAVE};
    uint32_t renders=0;
    for(uint32_t e=0;e<NELEM(engs);e++) {
        const engine_t *eng=engs[e];
        /* Pairwise extrema, every discrete voice/shape, low/high notes. */
        for(uint32_t mode=0;mode<8;mode++)for(uint32_t edge=0;edge<2;edge++)for(uint32_t note=24;note<=120;note+=96) {
            defaults(t,eng);
            for(uint32_t k=0;k<8;k++)t->p[P_E0+k]=edge?eng->edit[k].max:eng->edit[k].min;
            uint32_t which=e==2?4:0;
            t->p[P_E0+which]=mode%(eng->edit[which].max+1u);
            voice_on(t,eng,v,note,0);
            vmod_t m={.pitch16=(int32_t)note*16,.inc=pitch_inc(note*16),.amp0=32767,.amp1=32767};
            uint64_t energy=0;
            for(uint32_t b=0;b<128;b++) {
                int32_t out[CTL]={0};
                eng->render(t,v,out,CTL,&m);
                for(uint32_t k=0;k<CTL;k++) {
                    assert(out[k]>-131072&&out[k]<131072);
                    energy+=(uint32_t)(out[k]<0?-out[k]:out[k]);
                }
            }
            /* Some envelope shapes begin/end silent, but aggregate output stays bounded. */
            (void)energy;renders++;
        }
    }
    /* Fine tune and unison spread must survive chip arp/sweep pitch lookup. */
    for(uint32_t e=0;e<3;e++)for(int32_t fine=-40;fine<=40;fine+=80) {
        defaults(t,engs[e]);voice_on(t,engs[e],v,69,0);
        vmod_t m={.pitch16=69*16,.fine=fine,.amp0=32767,.amp1=32767};
        int32_t out[CTL]={0};
        engs[e]->render(t,v,out,CTL,&m);
        uint32_t inc=pitch_inc(69*16);
        inc+=(uint32_t)((int32_t)(inc>>12)*fine);
        assert(v->ph[0]==inc*CTL);
    }
    /* Above the usable harmonic room the warp tapers completely away. */
    int32_t clean[CTL]={0},warped[CTL]={0};
    defaults(t,&ENG_WAVE);t->p[P_E1]=127;
    voice_on(t,&ENG_WAVE,v,127,0);
    vmod_t high={.pitch16=127*16,.inc=pitch_inc(127*16),.amp0=32767,.amp1=32767};
    wave_render(t,v,clean,CTL,&high);
    defaults(t,&ENG_WAVE);t->p[P_E1]=127;t->p[P_E4]=127;
    voice_on(t,&ENG_WAVE,v,127,0);
    wave_render(t,v,warped,CTL,&high);
    assert(!memcmp(clean,warped,sizeof clean));
    for(uint32_t bank=0;bank<WAVE_NBANKS;bank++) {
        high_wave_spectrum(t,v,bank,0);high_wave_spectrum(t,v,bank,127);
    }
    /* All bank corners plus moving scans, low to high pitches, both warp extremes. */
    for(uint32_t bank=0;bank<WAVE_NBANKS;bank++)for(uint32_t corner=0;corner<4;corner++)
    for(uint32_t note=24;note<=120;note+=24)for(uint32_t warp=0;warp<=127;warp+=127) {
        defaults(t,&ENG_WAVE);t->p[P_E0]=(int16_t)bank;t->p[P_E1]=(int16_t)((corner*127+1)/3);
        t->p[P_E2]=127;t->p[P_E3]=127;t->p[P_E4]=(int16_t)warp;
        voice_on(t,&ENG_WAVE,v,note,0);
        vmod_t m={.pitch16=note*16,.inc=pitch_inc(note*16),.amp0=32767,.amp1=32767};
        uint64_t energy=0;
        for(uint32_t b=0;b<128;b++) {
            int32_t out[CTL]={0};wave_render(t,v,out,CTL,&m);
            for(uint32_t k=0;k<CTL;k++) {assert(abs(out[k])<131072);energy+=(uint32_t)abs(out[k]);}
        }
        assert(energy>0);
    }
    /* Playback and cross-wave interpolation wrap cleanly at both table ends. */
    for(uint32_t w=0;w<NELEM(WAVE_BANK);w++)for(uint32_t level=0;level<4;level++) {
        assert(wave_lookup(w,level,0)==WAVE_BANK[w][level][0]);
        int32_t a=wave_lookup(w,level,0xffffffffu),b=wave_lookup(w,level,0);
        assert(abs(a-b)<10);
    }
    printf("chip/wave: feedback periods, envelope clocks/release, %u bounded extreme renders, cyclic interpolation ok\n",renders);
}
