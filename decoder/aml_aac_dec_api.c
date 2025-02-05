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

#define LOG_TAG "aml_audio_faad_dec"
//#define LOG_NDEBUG 0

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "aml_dec_api.h"
#include "audio_data_process.h"
#include "aml_malloc_debug.h"
#include "audio_hw_utils.h"


#define FAAD_LIB_PATH "/vendor/lib/libfaad-ahal.so"
#define FAAD_LIB_PATH1 "/usr/lib/libfaad-ahal.so"//Linux Platform so is in /usr/lib/

#define AAC_MAX_LENGTH (1024 * 64)
#define AAC_REMAIN_BUFFER_SIZE (4096 * 10)
#define AAC_AD_NEED_CACHE_FRAME_COUNT  2

typedef struct faad_decoder_operations {
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
}faad_decoder_operations_t;

struct aac_dec_t {
    aml_dec_t  aml_dec;
    aml_faad_config_t aac_config;
    faad_decoder_operations_t faad_op;
    faad_decoder_operations_t ad_faad_op;
    aml_dec_stream_info_t stream_info;
    unsigned long total_raw_size;
    unsigned long total_pcm_size;
    unsigned long total_time; // s
    int bit_rate; // verage bit rate in the first five minutes
    void *pdecoder;
    char remain_data[AAC_REMAIN_BUFFER_SIZE];
    int remain_size;
    uint64_t remain_data_pts; //unit 90k;
    bool ad_mixing_enable;
    int advol_level;
    int  mixer_level;
    char ad_remain_data[AAC_REMAIN_BUFFER_SIZE];
    int ad_need_cache_frames;
    int ad_remain_size;
    unsigned char ad_fade;
    unsigned char ad_pan;
};

static  int unload_faad_decoder_lib(struct aac_dec_t *aac_dec)
{
    faad_decoder_operations_t *faad_op = &aac_dec->faad_op;
    faad_decoder_operations_t *ad_faad_op = &aac_dec->ad_faad_op;
    if (faad_op != NULL ) {
        faad_op->init = NULL;
        faad_op->decode = NULL;
        faad_op->release = NULL;
        faad_op->getinfo = NULL;

    }

    if (ad_faad_op != NULL ) {
        ad_faad_op->init = NULL;
        ad_faad_op->decode = NULL;
        ad_faad_op->release = NULL;
        ad_faad_op->getinfo = NULL;
    }
    if (aac_dec->pdecoder!= NULL) {
        dlclose(aac_dec->pdecoder);
        aac_dec->pdecoder = NULL;
    }
    return 0;
}


static  int load_faad_decoder_lib(struct aac_dec_t *aac_dec)
{
    faad_decoder_operations_t *faad_op = &aac_dec->faad_op;
    faad_decoder_operations_t *ad_faad_op = &aac_dec->ad_faad_op;
    aac_dec->pdecoder = dlopen(FAAD_LIB_PATH, RTLD_NOW);
    if (!aac_dec->pdecoder) {
        aac_dec->pdecoder = dlopen(FAAD_LIB_PATH1, RTLD_NOW);
        if (!aac_dec->pdecoder) {
            ALOGE("%s, failed to open (libfaad-ahal.so), %s\n", __FUNCTION__, dlerror());
            return -1;
        }
    }
    ALOGV("<%s::%d>--[faad_op->pdecoder]", __FUNCTION__, __LINE__);

    faad_op->init = ad_faad_op->init = (int (*) (void *)) dlsym(aac_dec->pdecoder, "audio_dec_init");
    if (faad_op->init == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d> audio_dec_init", __FUNCTION__, __LINE__);
    }

    faad_op->decode = ad_faad_op->decode = (int (*)(void *, char *outbuf, int *outlen, char *inbuf, int inlen))
                          dlsym(aac_dec->pdecoder, "audio_dec_decode");
    if (faad_op->decode  == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1 ;
    } else {
        ALOGV("<%s::%d>--[audio_dec_decode:]", __FUNCTION__, __LINE__);
    }

    faad_op->release = ad_faad_op->release =(int (*)(void *)) dlsym(aac_dec->pdecoder, "audio_dec_release");
    if ( faad_op->release== NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[audio_dec_release:]", __FUNCTION__, __LINE__);
    }

    faad_op->getinfo = ad_faad_op->getinfo = (int (*)(void *, AudioInfo *pAudioInfo)) dlsym(aac_dec->pdecoder, "audio_dec_getinfo");
    if (faad_op->getinfo == NULL) {
        ALOGI("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[audio_dec_getinfo:]", __FUNCTION__, __LINE__);
    }

    return 0;
}

static int faad_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct aac_dec_t *aac_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_faad_config_t *aac_config = NULL;
    dec_data_info_t *dec_aac_data = NULL;
    dec_data_info_t *ad_dec_pcm_data = NULL;

    if (dec_config == NULL) {
        ALOGE("AAC config is NULL\n");
        return -1;
    }

    aac_config = &dec_config->faad_config;

    /*if (aac_config->channel <= 0 || aac_config->channel > 8) {
        ALOGE("AAC config channel is invalid=%d\n", aac_config->channel);
        return -1;
    }

    if (aac_config->samplerate <= 0 ||aac_config->samplerate > 192000) {
        ALOGE("AAC config samplerate is invalid=%d\n", aac_config->samplerate);
        return -1;
    }*/

    aac_dec = aml_audio_calloc(1, sizeof(struct aac_dec_t));
    if (aac_dec == NULL) {
        ALOGE("malloc aac_dec failed\n");
        return -1;
    }
    aac_dec->total_pcm_size = 0;
    aac_dec->total_raw_size = 0;
    aac_dec->total_time = 0;
    aac_dec->bit_rate = 0;

    aml_dec = &aac_dec->aml_dec;

    memcpy(&aac_dec->aac_config, aac_config, sizeof(aml_faad_config_t));
    ALOGI("AAC format=%d samplerate =%d ch=%d\n", aac_config->aac_format,
          aac_config->samplerate, aac_config->channel);

    dec_aac_data = &aml_dec->dec_pcm_data;
    dec_aac_data->buf_size = AAC_MAX_LENGTH;
    dec_aac_data->buf = (unsigned char*) aml_audio_calloc(1, dec_aac_data->buf_size);
    if (!dec_aac_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto exit;
    }

    ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;

    ad_dec_pcm_data->buf_size = AAC_MAX_LENGTH;
    ad_dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, ad_dec_pcm_data->buf_size);
    if (!ad_dec_pcm_data->buf) {
        ALOGE("malloc ad buffer failed\n");
        goto exit;
    }

    ALOGI("ad_dec_pcm_data->buf %p", ad_dec_pcm_data->buf);
    if (load_faad_decoder_lib(aac_dec) == 0) {
       if (aac_config->aac_format == AUDIO_FORMAT_AAC_LATM) {
           aac_dec->faad_op.nAudioDecoderType = ACODEC_FMT_AAC_LATM;
           aac_dec->ad_faad_op.nAudioDecoderType = ACODEC_FMT_AAC_LATM;
       } else if (aac_config->aac_format == AUDIO_FORMAT_AAC) {
           aac_dec->faad_op.nAudioDecoderType = ACODEC_FMT_AAC;
           aac_dec->ad_faad_op.nAudioDecoderType = ACODEC_FMT_AAC;
       }
       int ret = aac_dec->faad_op.init((void *)&aac_dec->faad_op);
       if (ret != 0) {
           ALOGI("faad decoder init failed !");
           goto exit;
       }

       ret = aac_dec->ad_faad_op.init((void *)&aac_dec->ad_faad_op);
       if (ret != 0) {
           ALOGI("faad decoder init failed !");
           goto exit;
       }
    } else {
         goto exit;
    }
    aml_dec->status = 1;
    aac_dec->ad_need_cache_frames = AAC_AD_NEED_CACHE_FRAME_COUNT;
    *ppaml_dec = (aml_dec_t *)aac_dec;
    aac_dec->ad_mixing_enable = dec_config->ad_mixing_enable;
    aac_dec->mixer_level = dec_config->mixer_level;
    aac_dec->advol_level = dec_config->advol_level;
    aac_dec->ad_fade = dec_config->ad_fade;
    aac_dec->ad_pan = dec_config->ad_pan;
    aac_dec->remain_size = 0;
    memset(aac_dec->remain_data , 0 , AAC_REMAIN_BUFFER_SIZE * sizeof(char ));
    aac_dec->ad_remain_size = 0;
    memset(aac_dec->ad_remain_data , 0 , AAC_REMAIN_BUFFER_SIZE * sizeof(char ));
    ALOGE("%s success", __func__);
    return 0;

exit:
    if (aac_dec) {
        if (dec_aac_data->buf) {
            aml_audio_free(dec_aac_data->buf);
        }
        if (ad_dec_pcm_data) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        aml_audio_free(aac_dec);
    }
    *ppaml_dec = NULL;
    ALOGE("%s failed", __func__);
    return -1;
}

static int faad_decoder_release(aml_dec_t * aml_dec)
{
    dec_data_info_t *dec_pcm_data = NULL;
    dec_data_info_t *ad_dec_pcm_data = NULL;
    struct aac_dec_t *aac_dec = (struct aac_dec_t *)aml_dec;
    faad_decoder_operations_t *faad_op = &aac_dec->faad_op;
    faad_decoder_operations_t *ad_faad_op = &aac_dec->ad_faad_op;
    if (aml_dec != NULL) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        faad_op->release((void *)faad_op);

        ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;
        ALOGI("ad_dec_pcm_data->buf %p", ad_dec_pcm_data->buf);
        if (ad_dec_pcm_data->buf) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        ad_faad_op->release((void *)ad_faad_op);

        unload_faad_decoder_lib(aac_dec);
        aml_audio_free(aml_dec);
    }
    ALOGI("%s success", __func__);
    return 0;
}
static void dump_faad_data(void *buffer, int size, char *file_name)
{
   if (property_get_bool("vendor.audio.faad.outdump",false)) {
        FILE *fp1 = fopen(file_name, "a+");
        if (fp1) {
            int flen = fwrite((char *)buffer, 1, size, fp1);
            ALOGI("%s buffer %p size %d flen %d\n", __FUNCTION__, buffer, size,flen);
            fclose(fp1);
        }
    }
}
static int faad_decoder_process(aml_dec_t * aml_dec, struct audio_buffer *abuffer)
{
    struct aac_dec_t *aac_dec = NULL;
    aml_faad_config_t *aac_config = NULL;
    AudioInfo pAudioInfo,pADAudioInfo;
    int bytes = abuffer->size;
    const char * buffer = abuffer->buffer;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return -1;
    }

    aac_dec = (struct aac_dec_t *)aml_dec;
    aac_config = &aac_dec->aac_config;
    faad_decoder_operations_t *faad_op = &aac_dec->faad_op;
    faad_decoder_operations_t *ad_faad_op = &aac_dec->ad_faad_op;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;

    int used_size = 0;
    int used_size_return = 0;
    int mark_remain_size = aac_dec->remain_size;
    AM_LOGI_IF(aml_dec->debug_level, "remain_size %d bytes %d ad_mixing_enable %d advol_level %d mixer_level %d",
        aac_dec->remain_size ,bytes, aac_dec->ad_mixing_enable, aac_dec->advol_level,aac_dec->mixer_level );
    if (bytes > 0) {
        if (true == abuffer->b_pts_valid)
        {
            AM_LOGI_IF(aml_dec->debug_level, "remain_data_pts_aline 0x%"PRIx64" -> abuffer->pts 0x%"PRIx64" ,abuffer->b_pts_valid:%d", aac_dec->remain_data_pts, abuffer->pts, abuffer->b_pts_valid);
            aac_dec->remain_data_pts = abuffer->pts;
        }
        memcpy(aac_dec->remain_data + aac_dec->remain_size, buffer, bytes);
        aac_dec->remain_size += bytes;
    }
    dec_pcm_data->data_len = 0;

    while (aac_dec->remain_size > used_size) {
        int pcm_len = AAC_MAX_LENGTH;
        int decode_len = faad_op->decode(faad_op, (char *)(dec_pcm_data->buf + dec_pcm_data->data_len), &pcm_len, (char *)aac_dec->remain_data + used_size, aac_dec->remain_size  - used_size);

        if (decode_len > 0) {
            used_size += decode_len;
            dec_pcm_data->data_len += pcm_len;
            if (dec_pcm_data->data_len > dec_pcm_data->buf_size) {
                ALOGE("decode len %d  > buf_size %d ", dec_pcm_data->data_len, dec_pcm_data->buf_size);
                break;
            }
            AM_LOGI_IF(aml_dec->debug_level, "decode_len %d in %d pcm_len %d used_size %d", decode_len,  aac_dec->remain_size , pcm_len, used_size);
            faad_op->getinfo(faad_op,&pAudioInfo);
            aac_dec->stream_info.stream_sr = pAudioInfo.samplerate;
            aac_dec->stream_info.stream_ch = pAudioInfo.channels;
            aac_dec->stream_info.stream_error_num = pAudioInfo.error_num;
            aac_dec->stream_info.stream_drop_num = pAudioInfo.drop_num;
            aac_dec->stream_info.stream_decode_num = pAudioInfo.decode_num;

            if (dec_pcm_data->data_len) {
                aac_dec->remain_size = aac_dec->remain_size - used_size;
                dec_pcm_data->pts = aac_dec->remain_data_pts;
                if (used_size >= mark_remain_size) {
                    used_size_return = bytes;
                } else {
                    used_size_return = 0;
                    aac_dec->remain_size = mark_remain_size - used_size;
                }

                aac_dec->remain_data_pts = aac_dec->remain_data_pts + dec_pcm_data->data_len /( 2 * pAudioInfo.channels) * 1000 * 90 /pAudioInfo.samplerate;
                memmove(aac_dec->remain_data, aac_dec->remain_data + used_size, aac_dec->remain_size);
                break;
            }
        } else {
            if (aac_dec->remain_size > used_size) {
                if (0 == used_size) {
                    used_size = bytes; //remove broken data
                    AM_LOGE("remove broken data, aac_dec->remain_size %d, bytes:%d", aac_dec->remain_size , bytes);
                }
                aac_dec->remain_size = aac_dec->remain_size - used_size;
                if (aac_dec->remain_size > AAC_REMAIN_BUFFER_SIZE) {
                    AM_LOGE("aac_dec->remain_size %d > %d  ,overflow", aac_dec->remain_size , AAC_REMAIN_BUFFER_SIZE );
                    aac_dec->remain_size = 0;
                } else {
                    memmove(aac_dec->remain_data, aac_dec->remain_data + used_size, aac_dec->remain_size );
                }
            }
            else {
                memset(aac_dec->remain_data, 0x0, aac_dec->remain_size);
                aac_dec->remain_size = 0;
                AM_LOGE("aac_dec->remain_size %d, used_size:%d", aac_dec->remain_size , used_size);
            }
            used_size_return = bytes;
            AM_LOGI_IF(aml_dec->debug_level, "decode_len %d in %d pcm_len %d used_size %d aac_dec->remain_size %d", decode_len,  bytes, pcm_len, used_size, aac_dec->remain_size);
            break;
        }
    }

    aac_dec->total_raw_size += used_size_return;
    aac_dec->total_pcm_size += dec_pcm_data->data_len;

    if (pAudioInfo.channels == 1 && dec_pcm_data->data_len) {
            int16_t *samples_data = (int16_t *)dec_pcm_data->buf;
            int i = 0, samples_num,samples;
            samples_num = dec_pcm_data->data_len / sizeof(int16_t);
            for (; i < samples_num; i++) {
                samples = samples_data[samples_num - i -1] ;
                samples_data [ 2 * (samples_num - i -1) ] = samples;
                samples_data [ 2 * (samples_num - i -1) + 1]= samples;
            }
            dec_pcm_data->data_len  = dec_pcm_data->data_len * 2;
            pAudioInfo.channels = 2;
    }

    dec_pcm_data->data_sr  = pAudioInfo.samplerate;
    dec_pcm_data->data_ch  = pAudioInfo.channels;
    dec_pcm_data->data_format  = aac_config->aac_format;
    if (dec_pcm_data->data_len != ad_dec_pcm_data->data_len ) {
        ALOGV("dec_pcm_data->data_len %d ad_dec_pcm_data->data_len %d",dec_pcm_data->data_len ,ad_dec_pcm_data->data_len);
    }
    ad_dec_pcm_data->data_len  = 0;
    dump_faad_data(dec_pcm_data->buf, dec_pcm_data->data_len, "/data/faad_output.pcm");
    AM_LOGI_IF(aml_dec->debug_level, "pts: 0x%"PRIx64" (%"PRId64" ms) pcm len %d, buffer len %d, used_size_return %d",
        dec_pcm_data->pts, dec_pcm_data->pts/90, dec_pcm_data->data_len, dec_pcm_data->buf_size, used_size_return);
    return used_size_return;
}

static int faad_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{
    int ret = -1;
    struct aac_dec_t *aac_dec= (struct aac_dec_t *)aml_dec;

    switch (info_type) {
    case AML_DEC_REMAIN_SIZE:
        //dec_info->remain_size = ddp_dec->remain_size;
        return 0;
    case AML_DEC_STREMAM_INFO:
        memset(&dec_info->dec_info, 0x00, sizeof(aml_dec_stream_info_t));
        memcpy(&dec_info->dec_info, &aac_dec->stream_info, sizeof(aml_dec_stream_info_t));
        if (aac_dec->stream_info.stream_ch != 0 && aac_dec->stream_info.stream_sr != 0 && aac_dec->total_time < 300) { //we only calculate bitrate in the first five minutes
            aac_dec->total_time = aac_dec->total_pcm_size/(aac_dec->stream_info.stream_ch*2*(aac_dec->stream_info.stream_sr));
            if (aac_dec->total_time != 0) {
                aac_dec->bit_rate = (int)(aac_dec->total_raw_size/aac_dec->total_time);
            }
        }
        dec_info->dec_info.stream_bitrate = aac_dec->bit_rate;
        return 0;
    default:
        break;
    }
    return ret;
}

int faad_decoder_config(aml_dec_t * aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config)
{
    int ret = -1;
    struct aac_dec_t *aac_dec= (struct aac_dec_t *)aml_dec;

    if (aac_dec == NULL ) {
        return ret;
    }
    switch (config_type) {
    case AML_DEC_CONFIG_MIXER_LEVEL: {
        aac_dec->mixer_level = dec_config->mixer_level;
        ALOGI("dec_config->mixer_level %d",dec_config->mixer_level);
        break;
    }
    case AML_DEC_CONFIG_MIXING_ENABLE: {
        aac_dec->ad_mixing_enable = dec_config->ad_mixing_enable;
        ALOGI("dec_config->ad_mixing_enable %d",dec_config->ad_mixing_enable);
        break;
    }
    case AML_DEC_CONFIG_AD_VOL: {
        aac_dec->advol_level = dec_config->advol_level;
        ALOGI("dec_config->advol_level %d",dec_config->advol_level);
        break;
    }
    case AML_DEC_CONFIG_FADE: {
        aac_dec->ad_fade = dec_config->ad_fade;
        ALOGI("dec_config->ad_fade %d",dec_config->ad_fade);
        break;
    }
    case AML_DEC_CONFIG_PAN: {
        aac_dec->ad_pan = dec_config->ad_pan;
        ALOGI("dec_config->ad_pan %d",dec_config->ad_pan);
        break;
    }
    default:
        break;
    }

    return ret;
}


aml_dec_func_t aml_faad_func = {
    .f_init                 = faad_decoder_init,
    .f_release              = faad_decoder_release,
    .f_process              = faad_decoder_process,
    .f_config               = faad_decoder_config,
    .f_info                 = faad_decoder_getinfo,
};
