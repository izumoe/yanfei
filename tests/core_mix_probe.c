/* SPDX-License-Identifier: GPL-3.0-only */
/* A fresh process per case isolates the full audio path and effect tails. */
#define FELUCCA_CORE 1
#include "../web/emu/felucca_web.c"
#include <math.h>
int main(int argc,char **argv)
{
    if(argc<4)return 2;
    uint32_t e=(uint32_t)atoi(argv[1]),pr=(uint32_t)atoi(argv[2]),notes=(uint32_t)atoi(argv[3]);
    if(!eng_ok(e) || pr>=ENGINES[e]->npresets || (notes!=1 && notes!=8))return 2;
    web_nor_erase();web_boot();web_master(1023);
    for(uint32_t n=0;n<44100;n+=128)web_render(128);
    set_engine_of(&trk[0],e);apply_preset_to(&trk[0],pr);
    if(argc>4)trk[0].p[P_E4]=(int16_t)atoi(argv[4]);
    if(argc>5) {
        int bank=atoi(argv[5]),pos=argc>6?atoi(argv[6]):64;
        if(e!=ENGI_WAVE || bank<0 || bank>=(int)WAVE_NBANKS || pos<0 || pos>127)return 2;
        trk[0].p[P_E0]=(int16_t)bank;trk[0].p[P_E1]=(int16_t)pos;
        trk[0].p[P_E2]=trk[0].p[P_E3]=0;
    }
    for(uint32_t n=0;n<4410;n+=128)web_render(128);
    static const uint8_t chord[8]={48,52,55,59,60,64,67,71},drums[8]={36,38,39,42,46,45,37,56};
    for(uint32_t n=0;n<notes;n++)web_midi(0x90,e==ENGI_DRUM?drums[n]:notes==1?69:chord[n],110);
    double sum[2]={0},square[2]={0},peak=0;uint32_t count=0,rails=0;
    for(uint32_t n=0;n<44100*3;n+=128) {
        web_render(128);
        for(uint32_t i=0;i<128;i++) {
            float a[2]={out_l[i],out_r[i]};
            for(uint32_t k=0;k<2;k++) {
                double v=a[k];if(!isfinite(v))return 3;
                if(fabs(v)>peak)peak=fabs(v);
                if(fabs(v)>=32767.0/32768.0)rails++;
                if(n>=44100*2) {sum[k]+=v;square[k]+=v*v;}
            }
            if(n>=44100*2)count++;
        }
    }
    printf("{\"engine\":\"%s\",\"preset\":\"%s\",\"id\":%u,\"pr\":%u,\"notes\":%u,\"peak\":%.8f,\"rail_samples\":%u,\"mean_l\":%.8f,\"rms_l\":%.8f,\"mean_r\":%.8f,\"rms_r\":%.8f}\n",ENGINES[e]->name,ENGINES[e]->presets[pr].name,e,pr,notes,peak,rails,sum[0]/count,sqrt(square[0]/count),sum[1]/count,sqrt(square[1]/count));
    return 0;
}
