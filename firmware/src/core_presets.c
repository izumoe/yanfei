/* SPDX-License-Identifier: GPL-3.0-only */
/* Extra common controls and full operator patches for the compact factory
 * records. Main-loop sound loads only; no audio allocation or new save fields.
 * Original entries 0..2 retain their settings and stored slot IDs. */
static void core_preset_details(track_t *t,uint32_t e,uint32_t pi)
{
    if(e==ENGI_YM)ym_preset_values(t,pi);
    if(e==ENGI_PSG) {
        static const uint8_t sega[8]={0,0,1,0,0,1,1,1};
        t->p[P_ED_FX]=sega[pi%NELEM(sega)];
    }
    if(e==ENGI_FM6 && pi>=3u && pi<3u+FM6_NINSPIRED) {
        uint8_t v[FP_SIZE+1u];
        fm6_unpack(FM6_INSPIRED[pi-3u],v);
        uint32_t tr=(uint32_t)(t-trk);
        fm6_set_patch(tr,v);fm6_adopt(tr);
    }
    if(e==0u || e==6u) {
        t->p[P_LD_SHP]=0;t->p[P_LRATE]=60;t->p[P_LWAVE]=0;
        if(e==0u) {
            t->p[P_FTYPE]=pi==7u?1:0;
            if(pi==3u) {t->p[P_LD_SHP]=26;t->p[P_LRATE]=26;t->p[P_LWAVE]=1;}
            if(pi==4u) {t->p[P_ED_SHP]=26;}
        } else if(pi==3u || pi==7u) {
            t->p[P_LD_SHP]=pi==3u?20:12;t->p[P_LRATE]=pi==3u?30:20;t->p[P_LWAVE]=1;
        }
    }
}
