/*
 * Copyright (C) 2020 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "aml_audio_adpcm_dec"

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "aml_dec_api.h"
#include "aml_malloc_debug.h"
#include "aml_adpcm_dec_api.h"
#include "audio_hw_utils.h"

#define PCM_MAX_LENGTH (8192*2*2)
#define ADPCM_REMAIN_BUFFER_SIZE (4096 * 10)
#define ADPCM_LIB_PATH "/usr/lib/libadpcm-aml.so"

typedef struct adpcm_decoder_operations {
    const char * name;
    int nAudioDecoderType;
    int nInBufSize;
    int nOutBufSize;
    int (*init)(void *);
    int (*decode)(void *, char *outbuf, int *outlen, char *inbuf, int inlen);
    int (*release)(void *);
    int (*getinfo)(void *, AudioInfo *pAudioInfo);
    void * priv_data;//point to audec
    void * priv_dec_data;//decoder private data
    void *pdecoder; // decoder instance
    int channels;
    unsigned long pts;
    int samplerate;
    int bps;
    int extradata_size;      ///< extra data size
    char extradata[4096];
    int NchOriginal;
    int lfepresent;
    unsigned int block_size;
} adpcm_decoder_operations_t;

struct adpcm_dec_t {
    aml_dec_t  aml_dec;
    aml_dec_stream_info_t stream_info;
    int bit_rate;
    aml_adpcm_config_t adpcm_config;
    adpcm_decoder_operations_t adpcm_operation;
    void *pdecoder_lib;
    int remain_size;
};

static int load_adpcm_decoder_lib(struct adpcm_dec_t *adpcm_decoder)
{
    adpcm_decoder_operations_t *adpcm_operation = &adpcm_decoder->adpcm_operation;

    adpcm_decoder->pdecoder_lib = dlopen(ADPCM_LIB_PATH, RTLD_NOW);
    if (adpcm_decoder->pdecoder_lib == NULL) {
        ALOGE("%s[%d]: open decoder (%s) failed, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror(), ADPCM_LIB_PATH);
        return -1;
    } else {
        ALOGI("%s[%d]: open decoder (%s) succeed", __FUNCTION__, __LINE__, ADPCM_LIB_PATH);
    }

    adpcm_operation->init = (int (*) (void *)) dlsym(adpcm_decoder->pdecoder_lib, "audio_dec_init");
    if (adpcm_operation->init == NULL) {
        ALOGE("%s[%d]: can not find decoder init, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load adpcm audio_dec_init(%p) function succeed", __FUNCTION__, __LINE__, adpcm_operation->init);
    }

    adpcm_operation->decode = (int (*)(void *, char *outbuf, int *outlen, char *inbuf, int inlen))dlsym(adpcm_decoder->pdecoder_lib, "audio_dec_decode");
    if (adpcm_operation->decode == NULL) {
        ALOGE("%s[%d]: can not find decoder decode, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1 ;
    } else {
        ALOGI("%s[%d]: load adpcm audio_dec_decode(%p) function succeed", __FUNCTION__, __LINE__, adpcm_operation->decode);
    }

    adpcm_operation->release = (int (*)(void *)) dlsym(adpcm_decoder->pdecoder_lib, "audio_dec_release");
    if (adpcm_operation->release == NULL) {
        ALOGE("%s[%d]: can not find decoder release, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load adpcm audio_dec_release(%p) function succeed", __FUNCTION__, __LINE__, adpcm_operation->release);
    }

    adpcm_operation->getinfo = (int (*)(void *, AudioInfo *pAudioInfo)) dlsym(adpcm_decoder->pdecoder_lib, "audio_dec_getinfo");
    if (adpcm_operation->getinfo == NULL) {
        ALOGE("%s[%d]: can not find decoder get info, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load adpcm audio_dec_getinfo(%p) function succeed", __FUNCTION__, __LINE__, adpcm_operation->getinfo);
    }

    return 0;
}


static inline short CLIP16(int r)
{
    return (r >  0x7fff) ? 0x7fff :
           (r < -0x8000) ? -0x8000 :
           r;
}

static void downmix_4ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 4;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25) ;
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25);
        }
    }
    return;
}

static void downmix_6ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 6;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 5] * 0.25);
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 4] * 0.25
                       + (int)src[channel*i + 5] * 0.25);
        }
    }
    return;
}

static void downmix_8ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 8;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 4] * 0.25
                       + (int)src[channel*i + 6] * 0.25) ;
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 5] * 0.25
                       + (int)src[channel*i + 7] * 0.25);
        }
    }
    return;
}

static void downmix_1ch_to_2ch(void *in_buf, int bytes, int audio_format) {
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        int16_t *samples_data = (int16_t *)in_buf;
        int i, samples_num;
        int16_t samples = 0;
        samples_num = bytes / sizeof(int16_t);
        for (i = 0; i < samples_num; i++) {
            samples = samples_data[samples_num - i -1] ;
            samples_data [2 * (samples_num - i -1)] = samples;
            samples_data [2 * (samples_num - i -1) + 1]= samples;
        }
    }
    return;
}


static int adpcm_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct adpcm_dec_t *adpcm_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_adpcm_config_t *adpcm_config = NULL;
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * raw_in_data = NULL;
    int ret = 0;

    if (dec_config == NULL) {
        ALOGE("PCM config is NULL\n");
        return -1;
    }
    adpcm_config = &dec_config->adpcm_config;

    if (adpcm_config->channel <= 0 || adpcm_config->channel > 8) {
        ALOGE("PCM config channel is invalid=%d\n", adpcm_config->channel);
        return -1;
    }

    if (adpcm_config->samplerate <= 0 || adpcm_config->samplerate > 192000) {
        ALOGE("PCM config samplerate is invalid=%d\n", adpcm_config->samplerate);
        return -1;
    }

    if (!audio_is_linear_pcm(adpcm_config->pcm_format)) {
        ALOGE("PCM config format is not supported =%d\n", adpcm_config->pcm_format);
        return -1;
    }

    adpcm_dec = aml_audio_calloc(1, sizeof(struct adpcm_dec_t));
    if (adpcm_dec == NULL) {
        ALOGE("malloc dec failed\n");
        return -1;
    }

    aml_dec = &adpcm_dec->aml_dec;
    memcpy(&adpcm_dec->adpcm_config, adpcm_config, sizeof(aml_adpcm_config_t));

    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_pcm_data->buf_size = PCM_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (!dec_pcm_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto exit;
    }
    memset(dec_pcm_data->buf, 0, PCM_MAX_LENGTH * sizeof(char));

    raw_in_data = &aml_dec->raw_in_data;
    raw_in_data->buf_size = ADPCM_REMAIN_BUFFER_SIZE;
    raw_in_data->buf = (unsigned char*) aml_audio_calloc(1, raw_in_data->buf_size);
    if (!raw_in_data->buf) {
        ALOGE("malloc buffer failed\n");
        return -1;
    }

    memset(&(adpcm_dec->stream_info), 0x00, sizeof(adpcm_dec->stream_info));
    adpcm_dec->adpcm_operation.channels = adpcm_config->channel;
    adpcm_dec->adpcm_operation.samplerate = adpcm_config->samplerate;
    adpcm_dec->adpcm_operation.nAudioDecoderType = CODEC_ID_ADPCM_IMA_WAV;
    adpcm_dec->adpcm_operation.block_size = adpcm_config->block_size;

    if (load_adpcm_decoder_lib(adpcm_dec) == 0) {
        ret = adpcm_dec->adpcm_operation.init((void *)&adpcm_dec->adpcm_operation);
        if (ret != 0) {
           ALOGE("%s[%d]: flac decoder init failed !", __FUNCTION__, __LINE__);
           goto exit;
        }
    } else {
        goto exit;
    }

    aml_dec->status = 1;
    *ppaml_dec = (aml_dec_t *)adpcm_dec;
    ALOGI("[%s:%d] success PCM format=%d, samplerate:%d, ch:%d", __func__, __LINE__,
        adpcm_config->pcm_format, adpcm_config->samplerate, adpcm_config->channel);
    return 0;

exit:
    if (adpcm_dec) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }
        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
            raw_in_data->buf = NULL;
        }
        aml_audio_free(adpcm_dec);
    }
    *ppaml_dec = NULL;
    ALOGE("[%s:%d] failed", __func__, __LINE__);
    return -1;
}

static int unload_adpcm_decoder_lib(struct adpcm_dec_t *adpcm_decoder)
{
    int ret = 0;
    adpcm_decoder_operations_t *adpcm_operation = &adpcm_decoder->adpcm_operation;

    if (adpcm_operation != NULL ) {
        adpcm_operation->init = NULL;
        adpcm_operation->decode = NULL;
        adpcm_operation->release = NULL;
        adpcm_operation->getinfo = NULL;
    }

    if (adpcm_decoder->pdecoder_lib) {
        ret = dlclose(adpcm_decoder->pdecoder_lib);
        adpcm_decoder->pdecoder_lib = NULL;
        ALOGD("[%s:%d]unload ret %d", __func__, __LINE__, ret);
    }

    return 0;
}

static int adpcm_decoder_release(aml_dec_t * aml_dec)
{
    struct adpcm_dec_t *adpcm_decoder = (struct adpcm_dec_t *)aml_dec;
    adpcm_decoder_operations_t *adpcm_operation = &adpcm_decoder->adpcm_operation;
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * raw_in_data = NULL;

    if (aml_dec && adpcm_operation && adpcm_operation->release) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        raw_in_data = &aml_dec->raw_in_data;
        if (dec_pcm_data && dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }
        if (raw_in_data && raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
            raw_in_data->buf = NULL;
        }
        adpcm_operation->release((void *)adpcm_operation);
        unload_adpcm_decoder_lib(adpcm_decoder);

        aml_audio_free(aml_dec);
    } else
        ALOGE("[%s:%d]aml_dec %p, op %p, release func %p", __func__, __LINE__, aml_dec, adpcm_operation, adpcm_operation->release);
    return 0;
}

static void dump_data(void *buffer, int size, char *file_name)
{
    int flen = 0;
    if (property_get_bool("vendor.media.audiohal.adpcm", 0)) {
        FILE *fp = fopen(file_name, "a+");
        if (fp) {
            flen = fwrite((char *)buffer, 1, size, fp);
            ALOGV("%s[%d]: buffer=%p, need dump data size=%d, actual dump size=%d\n", __FUNCTION__, __LINE__, buffer, size, flen);
            fclose(fp);
        }
    }
}

static int adpcm_decoder_process(aml_dec_t * aml_dec, struct audio_buffer *abuffer)
{
    struct adpcm_dec_t *adpcm_dec = NULL;
    aml_adpcm_config_t *adpcm_config = NULL;
    int bytes = abuffer->size;
    const char *buffer = abuffer->buffer;
    int in_bytes = bytes, pcm_len = PCM_MAX_LENGTH, decode_len = 0;;
    int src_channel = 0, mark_remain_size = 0, used_size_return = 0;;
    int dst_channel = 0, used_size = 0;
    float downmix_conf = 1.0;
    int downmix_size = 0;

    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    if (bytes <= 0) {
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    adpcm_dec = (struct adpcm_dec_t *)aml_dec;
    adpcm_decoder_operations_t *adpcm_operation = &adpcm_dec->adpcm_operation;
    if (adpcm_operation == NULL || adpcm_operation->decode == NULL) {
        ALOGE("%s operation is %p, dec func is %p", __func__, adpcm_operation, adpcm_operation->decode);
        return AML_DEC_RETURN_TYPE_FAIL;
    }
    adpcm_config = &adpcm_dec->adpcm_config;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * raw_in_data = &aml_dec->raw_in_data;

    dec_pcm_data->data_len = 0;
    raw_in_data->data_len = 0;

    decode_len = adpcm_operation->decode((void *)adpcm_operation, (char *)(dec_pcm_data->buf + dec_pcm_data->data_len), &pcm_len,
            (char *)buffer, bytes);

    ALOGV("[%s:%d] finish decode. decode_len %d, pcm_len %d", __func__, __LINE__, decode_len, pcm_len);

    if (decode_len > 0) {
        dump_data(dec_pcm_data->buf, pcm_len, "/data/adpcm_decout.pcm");
        used_size = decode_len ;
        dec_pcm_data->data_len += pcm_len;
        if (dec_pcm_data->data_len > dec_pcm_data->buf_size) {
            ALOGE("%s[%d]: data len %d  > buf size %d ", __FUNCTION__, __LINE__, dec_pcm_data->data_len, dec_pcm_data->buf_size);
        }
        /*downmix to 2ch*/
        src_channel = adpcm_config->channel;
        dst_channel = 2;
        downmix_conf = (float)(src_channel * 1.0) / dst_channel;
        downmix_size = pcm_len / downmix_conf;

        if (dec_pcm_data->buf_size < downmix_size) {
            ALOGV("[%s:%d]realloc outbuf_max_len  from %zu to %zu\n", __FUNCTION__, __LINE__, dec_pcm_data->buf_size, downmix_size);
            dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, downmix_size);
            if (dec_pcm_data->buf == NULL) {
                ALOGE("[%s:%d]realloc pcm buffer failed size %zu\n", __FUNCTION__, __LINE__, downmix_size);
                return AML_DEC_RETURN_TYPE_FAIL;
            }
            dec_pcm_data->buf_size = downmix_size;
        }

        if (adpcm_config->channel == 2) {

        } else if (adpcm_config->channel == 4) {
            downmix_4ch_to_2ch(dec_pcm_data->buf, dec_pcm_data->buf, pcm_len, AUDIO_FORMAT_PCM_16_BIT);
        } else if (adpcm_config->channel == 6) {
            downmix_6ch_to_2ch(dec_pcm_data->buf, dec_pcm_data->buf, pcm_len, AUDIO_FORMAT_PCM_16_BIT);
        } else if (adpcm_config->channel == 8) {
            downmix_8ch_to_2ch(dec_pcm_data->buf, dec_pcm_data->buf, pcm_len, AUDIO_FORMAT_PCM_16_BIT);
        } else if (adpcm_config->channel == 1) {
            downmix_1ch_to_2ch(dec_pcm_data->buf, pcm_len, AUDIO_FORMAT_PCM_16_BIT);
        } else {
            ALOGE("unsupport channel =%d", adpcm_config->channel);
            return AML_DEC_RETURN_TYPE_OK;
        }
        dec_pcm_data->data_len = downmix_size;
    } else {
        ALOGE("[%s:%d]decode_len %d", __func__, __LINE__, decode_len);
    }

    adpcm_dec->stream_info.stream_decode_num += dec_pcm_data->data_len/4;
    if (adpcm_dec->stream_info.stream_bitrate == 0) {
        adpcm_dec->stream_info.stream_bitrate = src_channel*2*adpcm_config->samplerate;
    }
    adpcm_dec->stream_info.stream_ch = src_channel;
    adpcm_dec->stream_info.stream_sr = adpcm_config->samplerate;
    dec_pcm_data->data_sr = adpcm_config->samplerate;
    dec_pcm_data->data_ch = 2;
    dec_pcm_data->data_format = adpcm_config->pcm_format;
    dec_pcm_data->pts = abuffer->pts;
    AM_LOGI_IF(aml_dec->debug_level, "pts: 0x%llx (%lld ms) pcm len %d, buffer len %d, used_size_return %d",
        dec_pcm_data->pts, dec_pcm_data->pts/90, dec_pcm_data->data_len, dec_pcm_data->buf_size, used_size_return);

    return used_size;
}

static int adpcm_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{
    int ret = -1;
    struct adpcm_dec_t *adpcm_dec= (struct adpcm_dec_t *)aml_dec;

    switch (info_type) {
    case AML_DEC_REMAIN_SIZE:
        //dec_info->remain_size = pcm_dec->remain_size;
        return 0;
    case AML_DEC_STREMAM_INFO:
        memset(&dec_info->dec_info, 0x00, sizeof(aml_dec_stream_info_t));
        memcpy(&dec_info->dec_info, &adpcm_dec->stream_info, sizeof(aml_dec_stream_info_t));
        return 0;
    default:
        break;
    }
    return ret;
}

aml_dec_func_t aml_adpcm_func = {
    .f_init                 = adpcm_decoder_init,
    .f_release              = adpcm_decoder_release,
    .f_process              = adpcm_decoder_process,
    .f_config               = NULL,
    .f_info                 = adpcm_decoder_getinfo,
};
