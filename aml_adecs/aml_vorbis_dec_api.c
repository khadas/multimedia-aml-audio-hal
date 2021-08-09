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

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "aml_dec_api.h"
#include "audio_data_process.h"
#include "aml_malloc_debug.h"

#define LOG_TAG "aml_audio_vorbis_dec"
#define VORBIS_LIB_PATH "/usr/lib/libvorbis-aml.so"
#define VORBIS_MAX_LENGTH (1024 * 64)
#define VORBIS_REMAIN_BUFFER_SIZE (4096 * 10)
//#define LOG_NDEBUG 0

typedef struct _audio_info {
    int bitrate;
    int samplerate;
    int channels;
    int file_profile;
    int error_num;
} AudioInfo;

typedef struct vorbis_decoder_operations {
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
} vorbis_decoder_operations_t;

typedef struct {
    aml_dec_t  aml_decoder;
    aml_vorbis_config_t vorbis_config;
    vorbis_decoder_operations_t vorbis_operation;
    vorbis_decoder_operations_t ad_vorbis_operation;
    void *pdecoder_lib;
    char remain_data[VORBIS_REMAIN_BUFFER_SIZE];
    int remain_size;
    bool ad_decoder_supported;
    bool ad_mixing_enable;
    int advol_level;
    int mixer_level;
    char ad_remain_data[VORBIS_REMAIN_BUFFER_SIZE];
    int ad_remain_size;
} vorbis_decoder_t;

static void dump_vorbis_data(void *buffer, int size, char *file_name)
{
    int flen = 0;

    if (property_get_bool("vendor.media.vorbis.dump", false)) {
        FILE *fp1 = fopen(file_name, "a+");
        if (fp1) {
            flen = fwrite((char *)buffer, 1, size, fp1);
            ALOGI("%s[%d]: buffer=%p, need dump data size=%d, actual dump size=%d\n", __FUNCTION__, __LINE__, buffer, size, flen);
            fclose(fp1);
        }
    }
}

static int unload_vorbis_decoder_lib(vorbis_decoder_t *vorbis_decoder)
{
    vorbis_decoder_operations_t *vorbis_operation = &vorbis_decoder->vorbis_operation;
    vorbis_decoder_operations_t *ad_vorbis_operation = &vorbis_decoder->ad_vorbis_operation;

    if (vorbis_operation != NULL ) {
        vorbis_operation->init = NULL;
        vorbis_operation->decode = NULL;
        vorbis_operation->release = NULL;
        vorbis_operation->getinfo = NULL;
    }

    if (ad_vorbis_operation != NULL ) {
        ad_vorbis_operation->init = NULL;
        ad_vorbis_operation->decode = NULL;
        ad_vorbis_operation->release = NULL;
        ad_vorbis_operation->getinfo = NULL;
    }

    if (vorbis_decoder->pdecoder_lib) {
        dlclose(vorbis_decoder->pdecoder_lib);
        vorbis_decoder->pdecoder_lib = NULL;
    }

    return 0;
}

static int load_vorbis_decoder_lib(vorbis_decoder_t *vorbis_decoder)
{
    vorbis_decoder_operations_t *vorbis_operation = &vorbis_decoder->vorbis_operation;
    vorbis_decoder_operations_t *ad_vorbis_operation = &vorbis_decoder->ad_vorbis_operation;

    vorbis_decoder->pdecoder_lib = dlopen(VORBIS_LIB_PATH, RTLD_NOW);
    if (vorbis_decoder->pdecoder_lib == NULL) {
        ALOGE("%s[%d]: open decoder (libvorbis-aml.so) failed, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: open decoder (libvorbis-aml.so) succeed", __FUNCTION__, __LINE__);
    }

    vorbis_operation->init = ad_vorbis_operation->init = (int (*) (void *)) dlsym(vorbis_decoder->pdecoder_lib, "audio_dec_init");
    if (vorbis_operation->init == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load vorbis audio_dec_init function succeed", __FUNCTION__, __LINE__);
    }

    vorbis_operation->decode = ad_vorbis_operation->decode = (int (*)(void *, char *outbuf, int *outlen, char *inbuf, int inlen))
                          dlsym(vorbis_decoder->pdecoder_lib, "audio_dec_decode");
    if (vorbis_operation->decode  == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1 ;
    } else {
        ALOGI("%s[%d]: load vorbis audio_dec_decode function succeed", __FUNCTION__, __LINE__);
    }

    vorbis_operation->release = ad_vorbis_operation->release = (int (*)(void *)) dlsym(vorbis_decoder->pdecoder_lib, "audio_dec_release");
    if (vorbis_operation->release == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load vorbis audio_dec_release function succeed", __FUNCTION__, __LINE__);
    }

    vorbis_operation->getinfo = ad_vorbis_operation->getinfo = (int (*)(void *, AudioInfo *pAudioInfo)) dlsym(vorbis_decoder->pdecoder_lib, "audio_dec_getinfo");
    if (vorbis_operation->getinfo == NULL) {
        ALOGE("%s[%d]: can not find decoder lib, dlerror:%s\n", __FUNCTION__, __LINE__, dlerror());
        return -1;
    } else {
        ALOGI("%s[%d]: load vorbis audio_dec_getinfo function succeed", __FUNCTION__, __LINE__);
    }

    return 0;
}

static int vorbis_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t *dec_config)
{
    int ret = 0;
    vorbis_decoder_t *vorbis_decoder;
    dec_data_info_t *dec_pcm_data = NULL;
    dec_data_info_t *ad_dec_pcm_data = NULL;

    if (dec_config == NULL) {
        ALOGE("%s[%d]: vorbis config is NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    vorbis_decoder = aml_audio_calloc(1, sizeof(vorbis_decoder_t));
    if (vorbis_decoder == NULL) {
        ALOGE("%s[%d]: calloc vorbis_decoder failed", __FUNCTION__, __LINE__);
        return -1;
    }

    memcpy(&vorbis_decoder->vorbis_config, &dec_config->vorbis_config, sizeof(aml_vorbis_config_t));

    dec_pcm_data = &vorbis_decoder->aml_decoder.dec_pcm_data;
    dec_pcm_data->buf_size = VORBIS_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (dec_pcm_data->buf == NULL) {
        ALOGE("%s[%d]: calloc vorbis data buffer failed", __FUNCTION__, __LINE__);
        goto exit;
    }

    ad_dec_pcm_data = &vorbis_decoder->aml_decoder.ad_dec_pcm_data;
    ad_dec_pcm_data->buf_size = VORBIS_MAX_LENGTH;
    ad_dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, ad_dec_pcm_data->buf_size);
    if (ad_dec_pcm_data->buf == NULL) {
        ALOGE("%s[%d]: calloc vorbis data buffer failed", __FUNCTION__, __LINE__);
        goto exit;
    }

    if (load_vorbis_decoder_lib(vorbis_decoder) == 0) {
        ret = vorbis_decoder->vorbis_operation.init((void *)&vorbis_decoder->vorbis_operation);
        if (ret != 0) {
           ALOGE("%s[%d]: vorbis decoder init failed !", __FUNCTION__, __LINE__);
           goto exit;
        }

        ret = vorbis_decoder->ad_vorbis_operation.init((void *)&vorbis_decoder->ad_vorbis_operation);
        if (ret != 0) {
            ALOGE("%s[%d]: vorbis ad decoder init failed !", __FUNCTION__, __LINE__);
            goto exit;
        }
    } else {
         goto exit;
    }

    *ppaml_dec = (aml_dec_t *)vorbis_decoder;
    vorbis_decoder->aml_decoder.status = 1;
    vorbis_decoder->ad_decoder_supported = dec_config->ad_decoder_supported;
    vorbis_decoder->ad_mixing_enable = dec_config->ad_mixing_enable;
    vorbis_decoder->mixer_level = dec_config->mixer_level;
    vorbis_decoder->advol_level = dec_config->advol_level;
    vorbis_decoder->remain_size = 0;
    memset(vorbis_decoder->remain_data, 0, VORBIS_REMAIN_BUFFER_SIZE * sizeof(char));
    vorbis_decoder->ad_remain_size = 0;
    memset(vorbis_decoder->ad_remain_data , 0 ,VORBIS_REMAIN_BUFFER_SIZE * sizeof(char));
    ALOGI("%s[%d]: vorbis_decoder->ad_decoder_supported = %d", __FUNCTION__, __LINE__, vorbis_decoder->ad_decoder_supported);
    ALOGE("%s[%d]: success", __FUNCTION__, __LINE__);
    return 0;

exit:
    if (vorbis_decoder) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (ad_dec_pcm_data) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        aml_audio_free(vorbis_decoder);
    }
    *ppaml_dec = NULL;
    ALOGE("%s[%d]: failed", __FUNCTION__, __LINE__);
    return -1;
}

static int vorbis_decoder_release(aml_dec_t *aml_decoder)
{
    dec_data_info_t *dec_pcm_data = &aml_decoder->dec_pcm_data;
    dec_data_info_t *ad_dec_pcm_data = &aml_decoder->ad_dec_pcm_data;
    vorbis_decoder_t *vorbis_decoder = (vorbis_decoder_t *)aml_decoder;
    vorbis_decoder_operations_t *vorbis_operation = &vorbis_decoder->vorbis_operation;
    vorbis_decoder_operations_t *ad_vorbis_operation = &vorbis_decoder->ad_vorbis_operation;

    if (aml_decoder != NULL) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        vorbis_operation->release((void *)vorbis_operation);

        if (ad_dec_pcm_data->buf) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        ad_vorbis_operation->release((void *)ad_vorbis_operation);
        unload_vorbis_decoder_lib(vorbis_decoder);
        aml_audio_free(aml_decoder);
    }

    ALOGE("%s[%d]: success", __FUNCTION__, __LINE__);
    return 0;
}

static int vorbis_ad_decoder_process(aml_dec_t *aml_decoder, unsigned char *buffer, int bytes)
{
    int used_size = 0;
    vorbis_decoder_t *vorbis_decoder = (vorbis_decoder_t *)aml_decoder;
    aml_vorbis_config_t *vorbis_config = &vorbis_decoder->vorbis_config;
    vorbis_decoder_operations_t *ad_vorbis_operation = &vorbis_decoder->ad_vorbis_operation;
    dec_data_info_t *ad_dec_pcm_data = &aml_decoder->ad_dec_pcm_data;


    if (aml_decoder->ad_size > 0) {
        if ((vorbis_decoder->ad_remain_size + aml_decoder->ad_size) > VORBIS_REMAIN_BUFFER_SIZE) {
            vorbis_decoder->ad_remain_size = 0;
            memset(vorbis_decoder->ad_remain_data, 0, VORBIS_REMAIN_BUFFER_SIZE);
        }
        memcpy(vorbis_decoder->ad_remain_data + vorbis_decoder->ad_remain_size, aml_decoder->ad_data, aml_decoder->ad_size);
        vorbis_decoder->ad_remain_size += aml_decoder->ad_size;
        aml_decoder->ad_size = 0;
    }

    ad_dec_pcm_data->data_len = 0;
    ALOGV("%s[%d]: vorbis_decoder->ad_remain_size %d", __FUNCTION__, __LINE__, vorbis_decoder->ad_remain_size);
    while (vorbis_decoder->ad_remain_size > used_size) {
        int pcm_len = VORBIS_MAX_LENGTH;
        int decode_len = ad_vorbis_operation->decode(ad_vorbis_operation, (char *)(ad_dec_pcm_data->buf + ad_dec_pcm_data->data_len), &pcm_len,
            (char *)vorbis_decoder->ad_remain_data + used_size, vorbis_decoder->ad_remain_size - used_size);
        ALOGV("%s[%d]: ad decode_len %d, in %d, pcm_len %d, used_size %d", __FUNCTION__, __LINE__, decode_len, vorbis_decoder->ad_remain_size,
            pcm_len, used_size);

        if (decode_len > 0) {
            used_size += decode_len;
            if (pcm_len > 0) {
                ad_dec_pcm_data->data_len += pcm_len;
                if (ad_dec_pcm_data->data_len > ad_dec_pcm_data->buf_size) {
                    ALOGE("%s[%d]: decode len %d  > buf_size %d ", __FUNCTION__, __LINE__, ad_dec_pcm_data->data_len, ad_dec_pcm_data->buf_size);
                    break;
                }

                if (vorbis_decoder->ad_remain_size > used_size) {
                    vorbis_decoder->ad_remain_size -= used_size;    //The remaining unconverted data size
                    memmove(vorbis_decoder->ad_remain_data, vorbis_decoder->ad_remain_data + used_size, vorbis_decoder->ad_remain_size);
                } else {
                    vorbis_decoder->ad_remain_size = 0;    //All conversion
                }
                break;
            }
        } else {
            if (vorbis_decoder->ad_remain_size > used_size) {
                vorbis_decoder->ad_remain_size -= used_size;
                memmove(vorbis_decoder->ad_remain_data, vorbis_decoder->ad_remain_data + used_size, vorbis_decoder->ad_remain_size);
            }

            ALOGV("%s[%d]: ad vorbis_decoder->ad_remain_size %d ad_dec_pcm_data->data_len %d used_size %d", __FUNCTION__, __LINE__,
                vorbis_decoder->ad_remain_size, ad_dec_pcm_data->data_len, used_size);
            break;
        }
    }

    dump_vorbis_data(ad_dec_pcm_data->buf, ad_dec_pcm_data->data_len, "/data/vorbis_ad_output.pcm");
    return 0;
}

static int vorbis_decoder_process(aml_dec_t *aml_decoder, unsigned char *buffer, int bytes)
{
    int ret = 0;
    int used_size = 0;
    int decode_len = 0;
    int pcm_len = VORBIS_MAX_LENGTH;
    int used_size_return = 0;
    int mark_remain_size = 0;
    AudioInfo pAudioInfo;
    vorbis_decoder_t *vorbis_decoder = (vorbis_decoder_t *)aml_decoder;
    aml_vorbis_config_t *vorbis_config = &vorbis_decoder->vorbis_config;
    vorbis_decoder_operations_t *vorbis_operation = &vorbis_decoder->vorbis_operation;
    dec_data_info_t *dec_pcm_data = &aml_decoder->dec_pcm_data;

    if (aml_decoder == NULL) {
        ALOGE("%s[%d]: aml_decoder is NULL", __FUNCTION__, __LINE__);
        return -1;
    }

    ALOGV("%s[%d]: old size:%d, new add size:%d, all size:%d", __FUNCTION__, __LINE__, vorbis_decoder->remain_size, bytes, vorbis_decoder->remain_size + bytes);
    mark_remain_size = vorbis_decoder->remain_size;
    if (bytes > 0) {
        memcpy(vorbis_decoder->remain_data + vorbis_decoder->remain_size, buffer, bytes);
        vorbis_decoder->remain_size += bytes;
    }

    dec_pcm_data->data_len = 0;
    decode_len = vorbis_operation->decode(vorbis_operation, (char *)(dec_pcm_data->buf + dec_pcm_data->data_len), &pcm_len,
        (char *)vorbis_decoder->remain_data, vorbis_decoder->remain_size);
    ALOGV("%s[%d]: decode_len %d, in %d, pcm_len %d", __FUNCTION__, __LINE__, decode_len, vorbis_decoder->remain_size, pcm_len);

    if (decode_len > 0) {
        used_size = decode_len;
        dec_pcm_data->data_len += pcm_len;
        if (dec_pcm_data->data_len > dec_pcm_data->buf_size) {
            ALOGE("%s[%d]: data len %d  > buf size %d ", __FUNCTION__, __LINE__, dec_pcm_data->data_len, dec_pcm_data->buf_size);
        }

        if (used_size >= bytes) {
            used_size_return = bytes;
            vorbis_decoder->remain_size -= used_size;
            memmove(vorbis_decoder->remain_data, vorbis_decoder->remain_data + used_size, vorbis_decoder->remain_size);
        } else {
            used_size_return = used_size - mark_remain_size;
            vorbis_decoder->remain_size = 0;
        }
    } else {
        used_size_return = bytes;
        ALOGV("%s[%d]: in %d, decode_len %d, pcm_len %d, used_size %d, vorbis_dec->remain_size %d", __FUNCTION__, __LINE__, bytes, decode_len, pcm_len, used_size, vorbis_decoder->remain_size);
    }

    if ((vorbis_operation->channels == 1) && (dec_pcm_data->data_len > 0)) {
        int16_t *samples_data = (int16_t *)dec_pcm_data->buf;
        int i, samples_num, samples;
        samples_num = dec_pcm_data->data_len / sizeof(int16_t);
        for (i = 0; i < samples_num; i++) {
            samples = samples_data[samples_num - i -1] ;
            samples_data [2 * (samples_num - i -1)] = samples;
            samples_data [2 * (samples_num - i -1) + 1]= samples;
        }
        dec_pcm_data->data_len = dec_pcm_data->data_len * 2;
        vorbis_operation->channels = 2;
    }

    vorbis_operation->getinfo(vorbis_operation, &pAudioInfo);
    dec_pcm_data->data_sr = pAudioInfo.samplerate;
    dec_pcm_data->data_ch = pAudioInfo.channels;
    dec_pcm_data->data_format = vorbis_config->vorbis_format;
    dump_vorbis_data(dec_pcm_data->buf, dec_pcm_data->data_len, "/data/vorbis_output.pcm");
    ALOGV("%s[%d]: used_size_return %d, decoder pcm len %d, buffer len %d", __FUNCTION__, __LINE__, used_size_return, dec_pcm_data->data_len,
        dec_pcm_data->buf_size);

    return used_size_return;
}

static int vorbis_decoder_getinfo(aml_dec_t *aml_decoder, aml_dec_info_type_t info_type, aml_dec_info_t *dec_info)
{
    int ret = -1;
    vorbis_decoder_t *vorbis_decoder= (vorbis_decoder_t *)aml_decoder;

    switch (info_type) {
        case AML_DEC_REMAIN_SIZE:
            //dec_info->remain_size = ddp_dec->remain_size;
            return 0;
        default:
            break;
    }
    return ret;
}

static int vorbis_decoder_config(aml_dec_t *aml_decoder, aml_dec_config_type_t config_type, aml_dec_config_t *dec_config)
{
    int ret = -1;
    vorbis_decoder_t *vorbis_decoder = (vorbis_decoder_t *)aml_decoder;

    if (vorbis_decoder == NULL) {
        return ret;
    }

    switch (config_type) {
        case AML_DEC_CONFIG_MIXER_LEVEL: {
            vorbis_decoder->mixer_level = dec_config->mixer_level;
            ALOGI("%s[%d]: dec_config->mixer_level %d", __FUNCTION__, __LINE__, dec_config->mixer_level);
            break;
        }
        case AML_DEC_CONFIG_MIXING_ENABLE: {
            vorbis_decoder->ad_mixing_enable = dec_config->ad_mixing_enable;
            ALOGI("%s[%d]: dec_config->ad_mixing_enable %d", __FUNCTION__, __LINE__, dec_config->ad_mixing_enable);
            break;
        }
        case AML_DEC_CONFIG_AD_VOL: {
            vorbis_decoder->advol_level = dec_config->advol_level;
            ALOGI("%s[%d]: dec_config->advol_level %d", __FUNCTION__, __LINE__, dec_config->advol_level);
            break;
        }

        default:
            break;
    }

    return ret;
}

aml_dec_func_t aml_vorbis_func = {
    .f_init                 = vorbis_decoder_init,
    .f_release              = vorbis_decoder_release,
    .f_process              = vorbis_decoder_process,
    .f_config               = vorbis_decoder_config,
    .f_info                 = vorbis_decoder_getinfo,
};
