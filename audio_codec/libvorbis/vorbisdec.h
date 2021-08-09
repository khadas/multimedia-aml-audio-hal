/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Description: adpcm decoder
 */
 #ifndef _VORBISDEC_H_
#define _VORBISDEC_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <Tremolo/codec_internal.h>

#include <adec-armdec-mgt.h>

struct vorbis_dsp_state;
struct vorbis_info;

enum {
    kNumBuffers = 4,
    kMaxNumSamplesPerBuffer = 65536
};

size_t mInputBufferCount;
vorbis_dsp_state *mState;
vorbis_info *mVi;
bool mSignalledError;


int audio_dec_init(audio_decoder_operations_t *adec_ops);
int audio_dec_decode(audio_decoder_operations_t *adec_ops, char *outbuf, int *outlen, char *inbuf, int inlen);
int audio_dec_release(audio_decoder_operations_t *adec_ops);
int audio_dec_getinfo(audio_decoder_operations_t *adec_ops, void *pAudioInfo);
#endif
