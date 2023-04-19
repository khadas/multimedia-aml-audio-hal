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

#define LOG_TAG "aml_audio_flac_dec"

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "aml_dec_api.h"
#include "audio_data_process.h"
#include "aml_malloc_debug.h"

#define FLAC_LIB_PATH "/usr/lib/libflac-aml.so"
#define FLAC_MAX_LENGTH (1024 * 64)
#define FLAC_REMAIN_BUFFER_SIZE (4096 * 20)

typedef struct flac_decoder_operations {
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
} flac_decoder_operations_t;

typedef struct {
    aml_dec_t  aml_decoder;
    aml_flac_config_t flac_config;
    unsigned long total_raw_size;
    unsigned long total_pcm_size;
    unsigned long total_time; // s
    int bit_rate;
    aml_dec_stream_info_t stream_info;
    flac_decoder_operations_t flac_operation;
    void *pdecoder_lib;
    char remain_data[FLAC_REMAIN_BUFFER_SIZE];
    int remain_size;
} flac_decoder_t;

static void dump_flac_data(void *buffer, int size, char *file_name)
{
    int flen = 0;

    if (property_get_bool("vendor.media.flac.dump", false)) {
        FILE *fp = fopen(file_name, "a+");
        if (fp) {
            flen = fwrite((char *)buffer, 1, size, fp);
            ALOGI("%s[%d]: buffer=%p, need dump data size=%d, actual dump size=%d\n", __FUNCTION__, __LINE__, buffer, size, flen);
            fclose(fp);
        }
    }
}

static int unload_flac_decoder_lib(flac_decoder_t *flac_decoder)
{
    flac_decoder_operations_t *flac_operation = &flac_decoder->flac_operation;

    if (flac_operation != NULL ) {
        flac_operation->init = NULL;
        flac_operation->decode = NULL;
        flac_operation->release = NULL;
        flac_operation->getinfo = NULL;
    }

    if (flac_decoder->pdecoder_lib) {
        dlclose(flac_decoder->pdecoder_lib);
        flac_decoder->pdecoder_lib = NULL;
    }

    return 0;
}

static int load_flac_decoder_lib(flac_decoder_t *flac_decoder)
{
    flac_decoder_operations_t *flac_operation = &flac_decoder->flac_operation;

    flac_decoder->pdecoder_lib = dlopen(FLAC_LIB_PATH, RTLD_NOW);
    if (flac_decoder->pdecoder_lib == NULL) {
        ALOGE("%s[%d]: open decoder (libflac-aml.so) failed, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: open decoder (libflac-aml.so) succeed", __FUNCTION__, __LINE__);
    }

    flac_operation->init = (int (*) (void *)) dlsym(flac_decoder->pdecoder_lib, "audio_dec_init");
    if (flac_operation->init == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load flac audio_dec_init function succeed", __FUNCTION__, __LINE__);
    }

    flac_operation->decode = (int (*)(void *, char *outbuf, int *outlen, char *inbuf, int inlen))dlsym(flac_decoder->pdecoder_lib, "audio_dec_decode");
    if (flac_operation->decode  == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1 ;
    } else {
        ALOGI("%s[%d]: load flac audio_dec_decode function succeed", __FUNCTION__, __LINE__);
    }

    flac_operation->release = (int (*)(void *)) dlsym(flac_decoder->pdecoder_lib, "audio_dec_release");
    if (flac_operation->release == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load flac audio_dec_release function succeed", __FUNCTION__, __LINE__);
    }

    flac_operation->getinfo = (int (*)(void *, AudioInfo *pAudioInfo)) dlsym(flac_decoder->pdecoder_lib, "audio_dec_getinfo");
    if (flac_operation->getinfo == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load flac audio_dec_getinfo function succeed", __FUNCTION__, __LINE__);
    }

    return 0;
}

static int flac_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t *dec_config)
{
    int ret = 0;
    flac_decoder_t *flac_decoder;
    dec_data_info_t *dec_pcm_data = NULL;

    if (dec_config == NULL) {
        ALOGE("%s[%d]: flac config is NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    flac_decoder = aml_audio_calloc(1, sizeof(flac_decoder_t));
    if (flac_decoder == NULL) {
        ALOGE("%s[%d]: calloc flac_decoder failed", __FUNCTION__, __LINE__);
        return -1;
    }

    memcpy(&flac_decoder->flac_config, &dec_config->flac_config, sizeof(aml_flac_config_t));

    dec_pcm_data = &flac_decoder->aml_decoder.dec_pcm_data;
    dec_pcm_data->buf_size = FLAC_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (dec_pcm_data->buf == NULL) {
        ALOGE("%s[%d]: calloc flac data buffer failed", __FUNCTION__, __LINE__);
        goto exit;
    }

    if (load_flac_decoder_lib(flac_decoder) == 0) {
        ret = flac_decoder->flac_operation.init((void *)&flac_decoder->flac_operation);
        if (ret != 0) {
           ALOGE("%s[%d]: flac decoder init failed !", __FUNCTION__, __LINE__);
           goto exit;
        }
    } else {
         goto exit;
    }

    *ppaml_dec = (aml_dec_t *)flac_decoder;
    flac_decoder->aml_decoder.status = 1;
    flac_decoder->remain_size = 0;
    flac_decoder->total_pcm_size = 0;
    flac_decoder->total_raw_size = 0;
    flac_decoder->total_time = 0;
    flac_decoder->bit_rate = 0;
    memset(&(flac_decoder->stream_info), 0x00, sizeof(flac_decoder->stream_info));
    memset(flac_decoder->remain_data, 0, FLAC_REMAIN_BUFFER_SIZE * sizeof(char));
    ALOGI("%s[%d]: success", __FUNCTION__, __LINE__);
    return 0;

exit:
    if (flac_decoder) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }

        aml_audio_free(flac_decoder);
    }
    *ppaml_dec = NULL;
    ALOGE("%s[%d]: failed", __FUNCTION__, __LINE__);
    return -1;
}

static int flac_decoder_release(aml_dec_t *aml_decoder)
{
    flac_decoder_t *flac_decoder = (flac_decoder_t *)aml_decoder;
    dec_data_info_t *dec_pcm_data = &aml_decoder->dec_pcm_data;
    flac_decoder_operations_t *flac_operation = &flac_decoder->flac_operation;

    if (aml_decoder != NULL) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }

        flac_operation->release((void *)flac_operation);
        unload_flac_decoder_lib(flac_decoder);
        aml_audio_free(aml_decoder);
    }

    ALOGI("%s[%d]: success", __FUNCTION__, __LINE__);
    return 0;
}

static int flac_decoder_process(aml_dec_t *aml_decoder, unsigned char *buffer, int bytes)
{
    int ret = 0;
    int used_size = 0;
    int decode_len = 0;
    int pcm_len = FLAC_MAX_LENGTH;
    int used_size_return = 0;
    int mark_remain_size = 0;
    AudioInfo pAudioInfo;
    flac_decoder_t *flac_decoder = (flac_decoder_t *)aml_decoder;
    aml_flac_config_t *flac_config = &flac_decoder->flac_config;
    flac_decoder_operations_t *flac_operation = &flac_decoder->flac_operation;
    dec_data_info_t *dec_pcm_data = &aml_decoder->dec_pcm_data;

    if (aml_decoder == NULL) {
        ALOGE("%s[%d]: aml_decoder is NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    ALOGV("%s[%d]: old size:%d, new add size:%d, all size:%d", __FUNCTION__, __LINE__, flac_decoder->remain_size, bytes, flac_decoder->remain_size + bytes);
    if ((flac_decoder->remain_size + bytes) > FLAC_REMAIN_BUFFER_SIZE) {
        ALOGE("%s:%d: all size %d is bigger than %d", __func__, __LINE__, flac_decoder->remain_size + bytes, FLAC_REMAIN_BUFFER_SIZE);
        return used_size_return;
    }
    mark_remain_size = flac_decoder->remain_size;
    if (bytes > 0) {
        memcpy(flac_decoder->remain_data + flac_decoder->remain_size, buffer, bytes);
        flac_decoder->remain_size += bytes;
    }

    dec_pcm_data->data_len = 0;
    decode_len = flac_operation->decode(flac_operation, (char *)(dec_pcm_data->buf + dec_pcm_data->data_len), &pcm_len,
        (char *)flac_decoder->remain_data, flac_decoder->remain_size);
    ALOGV("%s[%d]: decode_len %d, in %d, pcm_len %d", __FUNCTION__, __LINE__, decode_len, flac_decoder->remain_size, pcm_len);
    if (flac_decoder->remain_size < 0) {
        ALOGE("%s:%d: remain_size < 0", __func__, __LINE__, flac_decoder->remain_size);
        return used_size_return;
    }
    if (decode_len > 0) {
        used_size = decode_len;
        dec_pcm_data->data_len += pcm_len;
        if (dec_pcm_data->data_len > dec_pcm_data->buf_size) {
            ALOGE("%s[%d]: data len %d  > buf size %d ", __FUNCTION__, __LINE__, dec_pcm_data->data_len, dec_pcm_data->buf_size);
        }

        if (used_size >= bytes) {
            used_size_return = bytes;
            flac_decoder->remain_size -= used_size;
            memmove(flac_decoder->remain_data, flac_decoder->remain_data + used_size, flac_decoder->remain_size);
        } else {
            used_size_return = used_size - mark_remain_size;
            flac_decoder->remain_size = 0;
        }
    } else {
        used_size_return = bytes;
        ALOGV("%s[%d]: in %d, decode_len %d, pcm_len %d, used_size %d, flac_dec->remain_size %d", __FUNCTION__, __LINE__, bytes, decode_len, pcm_len, used_size, flac_decoder->remain_size);
    }
    flac_decoder->total_pcm_size += dec_pcm_data->data_len;
    flac_decoder->total_raw_size += used_size_return;
    flac_decoder->stream_info.stream_decode_num = flac_decoder->total_pcm_size/4;//2ch*2byte

    if ((flac_operation->channels == 1) && (dec_pcm_data->data_len > 0)) {
        int16_t *samples_data = (int16_t *)dec_pcm_data->buf;
        int i, samples_num, samples;
        samples_num = dec_pcm_data->data_len / sizeof(int16_t);
        for (i = 0; i < samples_num; i++) {
            samples = samples_data[samples_num - i -1] ;
            samples_data [2 * (samples_num - i -1)] = samples;
            samples_data [2 * (samples_num - i -1) + 1]= samples;
        }
        dec_pcm_data->data_len = dec_pcm_data->data_len * 2;
        flac_operation->channels = 2;
    }

    flac_operation->getinfo(flac_operation, &pAudioInfo);
    dec_pcm_data->data_sr = pAudioInfo.samplerate;
    dec_pcm_data->data_ch = pAudioInfo.channels;
    flac_decoder->stream_info.stream_ch = dec_pcm_data->data_ch;
    flac_decoder->stream_info.stream_sr = dec_pcm_data->data_sr;
    dec_pcm_data->data_format = flac_config->flac_format;
    dump_flac_data(dec_pcm_data->buf, dec_pcm_data->data_len, "/data/flac_output.pcm");
    ALOGV("%s[%d]: used_size_return %d, decoder pcm len %d, buffer len %d", __FUNCTION__, __LINE__, used_size_return, dec_pcm_data->data_len,
        dec_pcm_data->buf_size);

    return used_size_return;
}

static int flac_decoder_getinfo(aml_dec_t *aml_decoder, aml_dec_info_type_t info_type, aml_dec_info_t *dec_info)
{
    int ret = -1;
    flac_decoder_t *flac_decoder= (flac_decoder_t *)aml_decoder;

    switch (info_type) {
        case AML_DEC_REMAIN_SIZE:
            return 0;
        case AML_DEC_STREMAM_INFO:
            memset(&dec_info->dec_info, 0x00, sizeof(aml_dec_stream_info_t));
            memcpy(&dec_info->dec_info, &flac_decoder->stream_info, sizeof(aml_dec_stream_info_t));
            if (flac_decoder->stream_info.stream_sr != 0 && flac_decoder->total_time < 300) { //we only calculate bitrate in the first five minutes
                flac_decoder->total_time = flac_decoder->stream_info.stream_decode_num/flac_decoder->stream_info.stream_sr;
                if (flac_decoder->total_time != 0) {
                    flac_decoder->bit_rate = (int)(flac_decoder->total_raw_size/flac_decoder->total_time);
                }
            }
            dec_info->dec_info.stream_bitrate = flac_decoder->bit_rate;
            return 0;

        default:
            break;
    }
    return ret;
}

static int flac_decoder_config(aml_dec_t *aml_decoder, aml_dec_config_type_t config_type, aml_dec_config_t *dec_config)
{
    int ret = -1;
    flac_decoder_t *flac_decoder = (flac_decoder_t *)aml_decoder;

    if (flac_decoder == NULL) {
        return ret;
    }

    switch (config_type) {
        default:
            break;
    }

    return ret;
}

aml_dec_func_t aml_flac_func = {
    .f_init                 = flac_decoder_init,
    .f_release              = flac_decoder_release,
    .f_process              = flac_decoder_process,
    .f_config               = flac_decoder_config,
    .f_info                 = flac_decoder_getinfo,
};
