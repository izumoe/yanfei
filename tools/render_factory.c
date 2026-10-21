/* SPDX-License-Identifier: GPL-3.0-only */
/* Native audition/manifest tool using the actual firmware UI sound loader and
 * emulator mixer. Build: cc -DFELUCCA_CORE=1 -fwrapv -O2 -ffp-contract=off -w
 * -Ibuild/gen -Ifirmware/src tools/render_factory.c -lm -o build/host/render_factory
 * --list emits every factory sound's complete parameters and FM6 patch.
 * ENGINE PRESET OUT.wav plays a short original phrase at the preset's own level. */
#include "../web/emu/felucca_web.c"
#include <math.h>
static void audio(uint32_t frames) {while(frames){uint32_t n=frames>128?128:frames;web_render(n);frames-=n;}}
static void select_sound(uint32_t e,uint32_t p)
{set_engine_of(&trk[0],e);apply_preset_to(&trk[0],p);audio(4410);}
static void manifest(void)
{
    printf("{\"pCount\":%u,\"pE0\":%u,\"engines\":[",P_COUNT,P_E0);
    for(uint32_t e=0;e<NENGINES;e++)printf("%s\"%s\"",e?",":"",ENGINES[e]->name);
    printf("],\"labels\":[");
    for(uint32_t i=0;i<P_E0;i++)printf("%s\"%s\"",i?",":"",TP[i].label);
    printf("],\"patches\":[");uint32_t first=1;
    for(uint32_t n=0;n<NENG_SHOWN;n++) {
        uint32_t e=eng_vis(n);
        for(uint32_t p=0;p<ENGINES[e]->npresets;p++) {
            select_sound(e,p);
            printf("%s{\"engine\":%u,\"engineName\":\"%s\",\"preset\":%u,\"name\":\"%s\",\"category\":\"%s\",\"params\":[",first?"":",",e,ENGINES[e]->name,p,ENGINES[e]->presets[p].name,CAT_NAME[preset_cat(e,p)]);first=0;
            for(uint32_t i=0;i<P_COUNT;i++)printf("%s%d",i?",":"",trk[0].p[i]);
            printf("]");
            if(e==ENGI_FM6) {uint8_t b[FM6_PACKED];fm6_pack(fm6_patch[0],b);printf(",\"fm6\":[");for(uint32_t i=0;i<FM6_PACKED;i++)printf("%s%u",i?",":"",b[i]);printf("]");}
            printf("}");
        }
    }
    printf("]}\n");
}
int main(int argc,char **argv)
{
    web_nor_erase();web_boot();web_master(1023);audio(FS);
    if(argc==2 && !strcmp(argv[1],"--list")){manifest();return 0;}
    if(argc!=4)return 2;
    uint32_t e=(uint32_t)atoi(argv[1]),pr=(uint32_t)atoi(argv[2]);
    if(!eng_ok(e)||pr>=ENGINES[e]->npresets)return 2;
    select_sound(e,pr);FILE *f=fopen(argv[3],"wb");if(!f)return 2;
    const uint32_t count=FS*4u,bytes=count*4u;
    uint32_t hdr[]={0x46464952,36u+bytes,0x45564157,0x20746d66,16,0x00020001,FS,FS*4u,0x00100004,0x61746164,bytes};
    fwrite(hdr,4,11,f);
    uint32_t cat=preset_cat(e,pr);int held=0;double square=0,peak=0;uint32_t rails=0;
    static const uint8_t melody[]={60,64,67,72,67,64},bass[]={36,36,43,48,43,36};
    static const uint8_t beat[]={36,42,38,42,36,46,38,42,36,42,38,42};
    for(uint32_t at=0;at<count;at+=128u) {
        uint32_t n=count-at<128u?count-at:128u;
        if(e==ENGI_DRUM) {
            uint32_t pos=at/(FS/4u),prev=at>=128?(at-128)/(FS/4u):99;
            if(pos<12 && pos!=prev){web_midi(0x90,beat[pos],110);web_midi(0x80,beat[pos],0);}
        } else if(cat==CAT_PAD || cat==CAT_FX) {
            if(at==0){web_midi(0x90,48,104);if(cat==CAT_PAD){web_midi(0x90,55,104);web_midi(0x90,60,104);}}
            if(at>=FS*2u && at-128u<FS*2u){web_midi(0x80,48,0);web_midi(0x80,55,0);web_midi(0x80,60,0);}
        } else {
            uint32_t pos=at/(FS/2u),prev=at>=128?(at-128)/(FS/2u):99;
            if(pos!=prev){if(held)web_midi(0x80,(uint32_t)held,0);held=0;if(pos<6){held=cat==CAT_BASS?bass[pos]:melody[pos];web_midi(0x90,(uint32_t)held,108);}}
            if(held && at%(FS/2u)>FS*3u/8u){web_midi(0x80,(uint32_t)held,0);held=0;}
        }
        web_render(n);
        for(uint32_t i=0;i<n;i++)for(uint32_t k=0;k<2;k++) {
            double v=k?out_r[i]:out_l[i];if(!isfinite(v))return 3;
            if(fabs(v)>peak)peak=fabs(v);if(fabs(v)>=32767.0/32768.0)rails++;
            square+=v*v;int16_t q=(int16_t)(v>1?32767:v< -1?-32767:v*32767);fwrite(&q,2,1,f);
        }
    }
    fclose(f);printf("{\"engine\":\"%s\",\"name\":\"%s\",\"peak\":%.8f,\"rms\":%.8f,\"rail_samples\":%u}\n",ENGINES[e]->name,ENGINES[e]->presets[pr].name,peak,sqrt(square/(count*2u)),rails);
    return !(peak>0 && rails==0);
}
