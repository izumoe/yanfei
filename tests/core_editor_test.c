/* SPDX-License-Identifier: GPL-3.0-only */
/* Core-specific protocol contract, including invalid IDs and all 96 factory loads. */
#define FELUCCA_CORE 1
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
#include <assert.h>
int main(void)
{
    reset();
    assert(request(ED_INFO,0,0)>0);
    assert(!memcmp(host_wire+5,"YANFEI ",7));
    assert(!user_preset_roundtrip());
    for(uint32_t n=0;n<NENG_SHOWN;n++)for(uint32_t pr=0;pr<ENGINES[eng_vis(n)]->npresets;pr++) {
        uint8_t a[2]={(uint8_t)eng_vis(n),(uint8_t)pr};
        assert(request(ED_PRESET,a,2)>0);
        assert(TSEL->eng_req==a[0] && TSEL->preset==pr);
        assert(request(ED_DUMP,a,0)>0);
    }
    set_engine_of(TSEL,ENGI_YM);
    assert(!strcmp(ed_tdesc(TSEL,P_FM1_ATK)->label,"AR"));
    assert(ed_tdesc(TSEL,P_FM1_ATK)->max==31 && ed_tdesc(TSEL,P_LN4)->min==-3);
    uint8_t dt[4]={0,P_LN4,125,63}; /* signed 14-bit -3 */
    assert(request(ED_SET,dt,4)>0 && TSEL->p[P_LN4]==-3);
    set_engine_of(TSEL,ENGI_PSG);TSEL->p[P_ED_FX]=1;
    assert(!strcmp(ed_tdesc(TSEL,P_E5)->label,"VOL"));
    assert(!strcmp(ed_tdesc(TSEL,P_ED_FX)->label,"CHIP"));
    assert(!strcmp(ed_tdesc(TSEL,P_E3)->label,"N-MIX"));
    TSEL->p[P_ED_FX]=0;
    assert(!strcmp(ed_tdesc(TSEL,P_E4)->label,"BUZZ"));
    assert(!strcmp(ed_tdesc(TSEL,P_E5)->label,"B-RATE"));
    set_engine_of(TSEL,ENGI_WAVE);
    const param_desc_t *bank=param_desc_of(ENGI_WAVE,P_E0);
    assert(bank->max==4 && !strcmp(bank->names[2],"PPG") &&
           !strcmp(bank->names[3],"ESQ") && !strcmp(bank->names[4],"VS"));
    for(uint32_t b=0;b<WAVE_NBANKS;b++) {
        uint8_t a[4]={0,P_E0,(uint8_t)b,64};
        assert(request(ED_SET,a,4)>0 && TSEL->p[P_E0]==(int16_t)b);
    }
    uint8_t patch[sizeof fm6_patch];memcpy(patch,fm6_patch,sizeof patch);
    uint32_t engine=TSEL->eng_req;
    for(uint32_t invalid=1;invalid<=11;invalid+=10) {
        uint8_t a[2]={(uint8_t)invalid,7};
        assert(!request(ED_PRESET,a,2));
        assert(TSEL->eng_req==engine && !memcmp(patch,fm6_patch,sizeof patch));
        up_rec_t rec=*up_rec(3);rec.engine=(uint8_t)invalid;
        assert(!up_valid(&rec) && up_put(4,&rec)==1);
    }
    /* A well-formed UP_PUT with a reserved engine cannot create a hidden sound. */
    uint8_t a[4+P_COUNT*2+32]={0,1,'X',0};
    for(uint32_t i=0;i<P_COUNT;i++)a[4+i*2+1]=64;
    up_rec_t out;uint32_t slot;
    assert(up_parse(a,sizeof a,&out,&slot)==1);
    puts("core editor: all factory loads, user preset values and reserved-ID rejection passed");
}
