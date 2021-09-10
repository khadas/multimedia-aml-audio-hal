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
/*
** @file
 * Vorbis I decoder
 * @author amlogic
 *
 * This file is part of audio_codec.
*/
#define LOG_TAG "aml_vorbis_dec"

#include "vorbisdec.h"
#include <cutils/log.h>

#define audio_codec_print printf
static int kDefaultChannelCount = 2;
static int kDefaultSamplingRate = 48000;
static unsigned int error_num = 0;
static unsigned int decode_num = 0;

int _vorbis_unpack_books(vorbis_info *vi,oggpack_buffer *opb);
int _vorbis_unpack_info(vorbis_info *vi,oggpack_buffer *opb);
int _vorbis_unpack_comment(vorbis_comment *vc,oggpack_buffer *opb);

static void makeBitReader(
        const void *data, size_t size,
        ogg_buffer *buf, ogg_reference *ref, oggpack_buffer *bits) {
    //ALOGE("%s,%d,size %d",__func__,__LINE__, size);
    if (data && buf && ref && bits) {
        buf->data = (uint8_t *)data;
        buf->size = size;
        buf->refcount = 1;
        buf->ptr.owner = NULL;

        ref->buffer = buf;
        ref->begin = 0;
        ref->length = size;
        ref->next = NULL;

        oggpack_readinit(bits, ref);
    }
}

int audio_dec_init(audio_decoder_operations_t *adec_ops)
{
    ALOGI("\n\n[%s]BuildDate--%s  BuildTime--%s", __FUNCTION__, __DATE__, __TIME__);
    audio_codec_print("[aml_vorbis_dec] %s,%d\n", __func__, __LINE__);

    mInputBufferCount = 0;
    mSignalledError = false;
    mState = NULL;
    mVi = NULL;
    error_num = 0;
    decode_num = 0;
    if (adec_ops != NULL) {
        adec_ops->channels = kDefaultChannelCount;
        adec_ops->samplerate = kDefaultSamplingRate;
    }

    return 0;
}

int audio_dec_decode(audio_decoder_operations_t *adec_ops, char *outbuf, int *outlen, char *inbuf, int inlen)
{
    //ALOGE("[%s %d]outbuf:%p, outlen %d, inbuf %s, inlen %d, mSignalledError %d\n", __FUNCTION__, __LINE__, *outlen, inbuf, inlen, mSignalledError);

    if (adec_ops && inbuf && outbuf && outlen) {
        const uint8_t *data = inbuf;
        size_t size = inlen;
        //ALOGV("%s,%d, mInputBufferCount %d, size %d, data0 %d, data1 %d, data2 %d, data3 %d, data4 %d, data5 %d, data6 %d", __func__, __LINE__, mInputBufferCount, inlen, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

        // decode header
        // Assume the very first 2 buffers are always codec config (in this case mState is NULL)
        // After flush, handle CSD
        if (mInputBufferCount < 2 ) {
            if (size < 7) {
                ALOGE("Too small input buffer: %zu bytes", size);
                mSignalledError = true;
                *outlen = 0;
                return 0;
            }

            ogg_buffer buf;
            ogg_reference ref;
            oggpack_buffer bits;

            makeBitReader((const uint8_t *)data + 7, size - 7, &buf, &ref, &bits);

            // Assume very first frame is identification header - or reset identification
            // header after flush, but allow only specifying setup header after flush if
            // identification header was already set up.
            if (mInputBufferCount == 0 && data[0] == 1 /* identification header */) {

                // remove any prior state
                if (mVi != NULL) {
                    // also clear mState as it may refer to the old mVi
                    if (mState != NULL) {
                        vorbis_dsp_clear(mState);
                        free(mState);
                        mState = NULL;
                    }
                    vorbis_info_clear(mVi);
                    free(mVi);
                    mVi = NULL;
                }

                mVi = (vorbis_info *)malloc(sizeof(vorbis_info));
                if (mVi == NULL) {
                    *outlen = 0;
                    ALOGE("%s,%d, malloc fail!", __func__, __LINE__);
                    return 0;
                }
                vorbis_info_init(mVi);

                int ret = _vorbis_unpack_info(mVi, &bits);
                if (ret != 0) {
                    mSignalledError = true;
                    *outlen = 0;
                    ALOGE("%s,%d, mSignalledError!", __func__, __LINE__);
                    return 0;
                }
                if (mVi->rate != kDefaultSamplingRate || mVi->channels != kDefaultChannelCount) {
                    ALOGI("vorbis: rate/channels changed: %ld/%d", mVi->rate, mVi->channels);
                }
                adec_ops->channels = mVi->channels;
                adec_ops->samplerate = mVi->rate;
                adec_ops->bps = mVi->bitrate_nominal;
                mInputBufferCount = 1;
            } else if (data[0] == 5 /* codebook header */) {
                // remove any prior state
                if (mState != NULL) {
                    vorbis_dsp_clear(mState);
                    free(mState);
                    mState = NULL;
                }

                int ret = _vorbis_unpack_books(mVi, &bits);
                if (ret != 0) {
                    mSignalledError = true;
                    ALOGE("%s,%d, mSignalledError!", __func__, __LINE__);
                    *outlen = 0;
                    return 0;
                }

                mState = (struct vorbis_dsp_state *)malloc(sizeof(struct vorbis_dsp_state));
                if (mState == NULL) {
                    *outlen = 0;
                    ALOGE("%s,%d, malloc fail!", __func__, __LINE__);
                    return 0;
                }
                vorbis_dsp_init(mState, mVi);

                mInputBufferCount = 2;
            }

            *outlen = 0;
            return inlen;
        }

        // decode data
        ogg_buffer buf;
        buf.data = inbuf ? inbuf : NULL;
        buf.size = inbuf ? inlen : 0;
        buf.refcount = 1;
        buf.ptr.owner = NULL;

        ogg_reference ref;
        ref.buffer = &buf;
        ref.begin = 0;
        ref.length = buf.size;
        ref.next = NULL;

        ogg_packet pack;
        pack.packet = &ref;
        pack.bytes = ref.length;
        pack.b_o_s = 0;
        pack.e_o_s = 0;
        pack.granulepos = 0;
        pack.packetno = 0;

        int numFrames = 0;

        if (mState == NULL || mVi == NULL) {
            mSignalledError = true;
            *outlen = 0;
            ALOGE("%s,%d, input does not have CSD", __func__, __LINE__);
            return 0;
        }

        int err = vorbis_dsp_synthesis(mState, &pack, 1);
        if (err != 0) {
            error_num++;
            // FIXME temporary workaround for log spam
#if !defined(__arm__) && !defined(__aarch64__)
            ALOGE("%s,%d, vorbis_dsp_synthesis returned %d", __func__, __LINE__, err);
#else
            ALOGE("%s,%d, vorbis_dsp_synthesis returned %d", __func__, __LINE__, err);
#endif
        } else {
            decode_num++;
            size_t numSamplesPerBuffer = kMaxNumSamplesPerBuffer;
            if (numSamplesPerBuffer > *outlen / sizeof(int16_t)) {
                numSamplesPerBuffer = *outlen / sizeof(int16_t);
            }
            numFrames = vorbis_dsp_pcmout(
                                    mState, (int16_t *)outbuf,
                                    (numSamplesPerBuffer / mVi->channels));

            if (numFrames < 0) {
                ALOGE("vorbis_dsp_pcmout returned %d", numFrames);
                numFrames = 0;
            }
        }
        *outlen = numFrames * sizeof(int16_t) * mVi->channels;
        //ALOGV("%s,%d, numframes %d, outlen %d!", __func__, __LINE__, numFrames, *outlen);
        return inlen;

    } else {
        ALOGE("%s,%d, input params check err: adec_ops/inbuf/outbuf/outlen is null!", __func__, __LINE__);
        *outlen = 0;
        return inlen;
    }

}

int audio_dec_release(audio_decoder_operations_t *adec_ops)
{
    if (adec_ops) {
        ALOGI("vorbis %s, %d, name:%s\n", __func__, __LINE__, adec_ops->name);
        audio_codec_print("[aml_vorbis_dec] %s,%d\n", __func__, __LINE__);
    }

    if (mState != NULL) {
       // Make sure that the next buffer output does not still
       // depend on fragments from the last one decoded.
       vorbis_dsp_restart(mState);
    }
    if (mState != NULL) {
        vorbis_dsp_clear(mState);
        free(mState);
        mState = NULL;
    }

    if (mVi != NULL) {
        vorbis_info_clear(mVi);
        free(mVi);
        mVi = NULL;
    }
    mSignalledError = false;
    mInputBufferCount = 0;
    return 0;
}

int audio_dec_getinfo(audio_decoder_operations_t *adec_ops, void *pAudioInfo)
{
    //audio_codec_print("[aml_vorbis_dec] %s,%d\n", __func__, __LINE__);
    //ALOGV("vorbis %s,%d\n", __func__, __LINE__);
    if (adec_ops != NULL && pAudioInfo != NULL) {
        ((AudioInfo *)pAudioInfo)->channels = adec_ops->channels > 2 ? 2 : adec_ops->channels;;
        ((AudioInfo *)pAudioInfo)->samplerate = adec_ops->samplerate;
        ((AudioInfo *)pAudioInfo)->bitrate = adec_ops->bps;
        ((AudioInfo *)pAudioInfo)->error_num = error_num;
        ((AudioInfo *)pAudioInfo)->drop_num = error_num;
        ((AudioInfo *)pAudioInfo)->decode_num = decode_num;
    }
    #if 0
    ALOGI("%s:%d stream_sr(%d) stream_ch(%d) stream_bitrate(%d) stream_error_num(%d) stream_drop_num(%d) stream_decode_num(%d)",
        __FUNCTION__, __LINE__, ((AudioInfo *)pAudioInfo)->samplerate, ((AudioInfo *)pAudioInfo)->channels,
        ((AudioInfo *)pAudioInfo)->bitrate, ((AudioInfo *)pAudioInfo)->error_num,
        ((AudioInfo *)pAudioInfo)->drop_num, ((AudioInfo *)pAudioInfo)->decode_num);
    ALOGI("%s:%d stream_sr(%d) stream_ch(%d) stream_bitrate(%d) stream_error_num(%d) stream_drop_num(%d) stream_decode_num(%d)",
        __FUNCTION__, __LINE__, adec_ops->channels , adec_ops->samplerate,
        adec_ops->bps, error_num,
        error_num, decode_num);
    #endif
    return 0;
}

