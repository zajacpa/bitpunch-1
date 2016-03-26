/*
This file is part of BitPunch
Copyright (C) 2015 Frantisek Uhrecky <frantisek.uhrecky[what here]gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "hybrid.h"
#include <bitpunch/bitpunch.h>

#if defined(BPU_CONF_MECS_HYBRID) 
#include <bitpunch/debugio.h>
#include <bitpunch/math/gf2.h>
#include <bitpunch/crypto/mecsbasic/mecsbasic.h>
#include <bitpunch/crypto/aes/aes.h>
#include <bitpunch/crypto/kdf/pbkdf2.h>
#include <bitpunch/crypto/mac/mac.h>
#include <bitpunch/crypto/padding/padding.h>

#ifdef BPU_CONF_ENCRYPTION
int BPU_hybridEncrypt(BPU_T_GF2_Vector *out, const BPU_T_GF2_Vector *in, const BPU_T_Mecs_Ctx *ctx) {
    BPU_T_GF2_Vector *key_enc,*key_auth,*mac_salt, *enc_salt,*ct_dem, *iv_dem,*mac,*pt_kem,*pt_kem_pad;
    int err = 0;
    int pt_kem_size = 0;
    int pt_kem_pad_size = 0;

    //Alloc memory for mac
    BPU_gf2VecMalloc(&mac,512);
    //Alloc memory for IV
    BPU_gf2VecMalloc(&iv_dem,256);

    //TODO: CT_DEM should be divisible by 16, otherwise padding
    BPU_gf2VecMalloc(&ct_dem,976);

    //Alloc memory for keys
    BPU_gf2VecMalloc(&key_enc,256);
    BPU_gf2VecMalloc(&key_auth,256);

    //Must be defined as protocol constant
    BPU_gf2VecMalloc(&enc_salt,512);
    BPU_gf2VecMalloc(&mac_salt,512);
    BPU_gf2ArraytoVector(enc_salt,encsalt);
    BPU_gf2ArraytoVector(mac_salt,macsalt);

    //Compute keys for enc and mac
    BPU_gf2VecKDF(key_enc,ctx->code_ctx->e, enc_salt, 256);
    BPU_gf2VecKDF(key_auth,ctx->code_ctx->e, mac_salt,256);

    //DEM encryption
    err += BPU_gf2VecAesEnc(ct_dem,in,key_enc,iv_dem);
    BPU_printGf2Vec(ct_dem);
    //MAC computation
    BPU_gf2VecComputeHMAC(mac,ct_dem, key_auth);

    //Concatination of AES_ENC(M) and HMAC(m)
    pt_kem_size = ct_dem->len + mac->len;
    BPU_gf2VecMalloc(&pt_kem,pt_kem_size);
    BPU_gf2VecConcat(pt_kem,ct_dem,mac);

    //ADD padding if required, beaware 1498 len pre dane params MECS
    pt_kem_pad_size = 1498 - pt_kem_size;
    BPU_gf2VecMalloc(&pt_kem_pad, 1498);
    BPU_padAdd(pt_kem_pad,pt_kem,pt_kem_pad_size);

    fprintf(stderr, "MECS encryption...\n");
    if (BPU_mecsBasicEncrypt(out, pt_kem_pad, ctx,0)) {
        BPU_printError("Encryption error");
        BPU_gf2VecFree(&ctx);
        BPU_gf2VecFree(&pt_kem_pad);
        BPU_gf2VecFree(&pt_kem);
        return 1;
    }

    BPU_gf2VecFree(&iv_dem);
    BPU_gf2VecFree(&ct_dem);
    BPU_gf2VecFree(&key_auth);
    BPU_gf2VecFree(&key_enc);
    BPU_gf2VecFree(&mac_salt);
    BPU_gf2VecFree(&enc_salt);

	return 0;
}
#endif // BPU_CONF_ENCRYPTION

#ifdef BPU_CONF_DECRYPTION
int BPU_hybridDecrypt(BPU_T_GF2_Vector *out, const BPU_T_GF2_Vector *in, const BPU_T_Mecs_Ctx *ctx) {
    int err = 0;
    BPU_T_GF2_Vector *pt_kem_dec_pad,*pt_kem_dec,*enc_salt,*mac_salt,*mac_a,*mac_b,*key_enc,*key_auth,*ct_dem, *iv_dem;
    //Alloc memory for decrypted MECS, TODO: nerobim to natrvdo
    BPU_gf2VecMalloc(&pt_kem_dec_pad, 1498);
    BPU_gf2VecMalloc(&pt_kem_dec, 1488);
    BPU_gf2VecMalloc(&ct_dem, 976);
    BPU_gf2VecMalloc(&iv_dem,256);

    //Allocation of memory for macs
    BPU_gf2VecMalloc(&mac_a, 512);
    BPU_gf2VecMalloc(&mac_b, 512);

    fprintf(stderr, "MECS decryption...\n");
     if (BPU_mecsBasicDecrypt(pt_kem_dec_pad, in, ctx)) {
         BPU_printError("Decryption error");
         BPU_gf2VecFree(&ctx);
         BPU_gf2VecFree(&pt_kem_dec_pad);
         return 1;
     }

     //Remove padding
    BPU_padDel(pt_kem_dec,pt_kem_dec_pad);

    //Alloc memory for keys
    BPU_gf2VecMalloc(&key_enc,256);
    BPU_gf2VecMalloc(&key_auth,256);

    //Must be defined as protocol constant
    BPU_gf2VecMalloc(&enc_salt,512);
    BPU_gf2VecMalloc(&mac_salt,512);
    BPU_gf2ArraytoVector(enc_salt,encsalt);
    BPU_gf2ArraytoVector(mac_salt,macsalt);

    //Compute keys for enc and mac
    BPU_gf2VecKDF(key_enc,ctx->code_ctx->e, enc_salt, 256);
    BPU_gf2VecKDF(key_auth,ctx->code_ctx->e, mac_salt,256);

    //TODO: zas to nerezat natvrdo
     BPU_gf2VecCrop(ct_dem,pt_kem_dec,0,976);
     BPU_gf2VecCrop(mac_a,pt_kem_dec,976,512);

     BPU_gf2VecComputeHMAC(mac_b,ct_dem, key_auth);

     if(BPU_gf2VecCmp(mac_a,mac_b) == 0){
         fprintf(stderr, "\nMACs matches\n");
     }
    BPU_printGf2Vec(ct_dem);
     err += BPU_gf2VecAesDec(out,ct_dem,key_enc,iv_dem);

	return 0;
}

#endif // BPU_CONF_DECRYPTION

#endif // BPU_CONF_MECS_HYBRID

