/*
 * Copyright (C) 2021 Amlogic Corporation.
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

#define LOG_TAG "aml_dec_api"

#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <cutils/log.h>

#include "aml_dec_api.h"
#include "aml_ddp_dec_api.h"
#include "aml_dtsx_dec_api.h"
#include "aml_pcm_dec_api.h"
#include "aml_adpcm_dec_api.h"
#include "aml_mpeg_dec_api.h"
#include "aml_aac_dec_api.h"
#include "aml_flac_dec_api.h"
#include "aml_vorbis_dec_api.h"
#include "audio_hw_utils.h"

#ifdef BUILD_LINUX
#define DTS_X_LIB_PATH_A        "/vendor/lib/libHwAudio_dtsx.so"
#define DTS_HD_LIB_PATH_A       "/usr/lib/libHwAudio_dtshd.so"
#else
#define DTS_X_LIB_PATH_A        "/odm/lib/libHwAudio_dtsx.so"
#define DTS_HD_LIB_PATH_A       "/odm/lib/libHwAudio_dtshd.so"
#endif
#define AML_DEC_FRAGMENT_FRAMES     (512)
#define AML_DEC_MAX_FRAMES          (AML_DEC_FRAGMENT_FRAMES * 4)
static eDTSLibType_t gDtsLibType = eDTSNull;

static aml_dec_func_t * get_decoder_function(audio_format_t format)
{
    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        return &aml_dcv_func;
    }
    case AUDIO_FORMAT_DOLBY_TRUEHD:
    case AUDIO_FORMAT_MAT:
        return NULL;
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD: {
        if (gDtsLibType == eDTSXLib) {
            return &aml_dtsx_func;
        } else if (gDtsLibType == eDTSHDLib) {
            return &aml_dca_func;
        } else {
            ALOGE("[%s:%d] Without any dts library", __func__, __LINE__);
            return NULL;
        }
    }
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_8_BIT:
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_LPCM_DVD:
    case AUDIO_FORMAT_PCM_LPCM_1394:
    case AUDIO_FORMAT_PCM_LPCM_BLURAY: {
        return &aml_pcm_func;
    }
    case AUDIO_FORMAT_MP3:
    case AUDIO_FORMAT_MP2: {
        return  &aml_mad_func;
    }
    case AUDIO_FORMAT_AAC:
    case AUDIO_FORMAT_AAC_LATM:
    case AUDIO_FORMAT_HE_AAC_V1:
    case AUDIO_FORMAT_HE_AAC_V2:{
        return  &aml_faad_func;
    }
    case AUDIO_FORMAT_FLAC: {
        return &aml_flac_func;
    }
    case AUDIO_FORMAT_VORBIS: {
        return &aml_vorbis_func;
    }
    case AUDIO_FORMAT_PCM_ADPCM_IMA_WAV: {
        return &aml_adpcm_func;
    }
    default:
        ALOGE("[%s:%d] doesn't support decoder format:%#x", __func__, __LINE__, format);
        return NULL;
    }

    return NULL;
}

int aml_decoder_init(aml_dec_t **ppaml_dec, audio_format_t format, aml_dec_config_t * dec_config)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    dec_fun = get_decoder_function(format);
    aml_dec_t *aml_dec_handel = NULL;
    if (dec_fun == NULL) {
        ALOGE("%s got dec_fun as NULL!\n", __func__);
        return -1;
    }

    ALOGD("[%s:%d] dec_fun->f_init=%p, format:%#x", __func__, __LINE__, dec_fun->f_init, format);
    if (dec_fun->f_init) {
        ret = dec_fun->f_init(ppaml_dec, dec_config);
        if (ret < 0) {
            return -1;
        }
    } else {
        return -1;
    }

    aml_dec_handel = *ppaml_dec;
    aml_dec_handel->frame_cnt = 0;
    aml_dec_handel->format = format;
    aml_dec_handel->fragment_left_size = 0;
    dec_config->advol_level = 100;
    dec_config->mixer_level = 0;
    dec_config->ad_fade = 0;
    dec_config->ad_pan = 0;
    aml_dec_handel->ad_data = NULL;
    aml_dec_handel->ad_size = 0;
    aml_dec_handel->out_frame_pts = -1;
    aml_dec_handel->last_in_frame_pts = -1;
    aml_dec_handel->debug_level = aml_audio_property_get_int("vendor.media.audio.hal.decoder", 0);
    return ret;

ERROR:
    if (dec_fun->f_release && aml_dec_handel) {
        dec_fun->f_release(aml_dec_handel);
    }

    return -1;

}
int aml_decoder_release(aml_dec_t *aml_dec)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL\n", __func__);
        return -1;
    }

    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        return -1;
    }

    if (dec_fun->f_release) {
        dec_fun->f_release(aml_dec);
    } else {
        return -1;
    }

    if (access(REPORT_DECODED_INFO, F_OK) == 0) {
        char info_buf[MAX_BUFF_LEN] = {0};
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        int val = 0;
        sprintf(info_buf, "bitrate %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "ch_num %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "samplerate %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "decoded_frames %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "decoded_err %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "decoded_drop %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf (info_buf, "ch_configuration %d", val);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
    }

    return ret;


}
int aml_decoder_set_config(aml_dec_t *aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL\n", __func__);
        return -1;
    }
    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        return -1;
    }

    if (dec_fun->f_config) {
        ret = dec_fun->f_config(aml_dec, config_type, dec_config);
    }

    return ret;
}

int aml_decoder_get_info(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL\n", __func__);
        return -1;
    }
    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        return -1;
    }

    if (dec_fun->f_info) {
        ret = dec_fun->f_info(aml_dec, info_type, dec_info);
    }

    return ret;
}


static void UpdateDecodeInfo_ChannelConfiguration(char *sysfs_buf, int ch_num) {
    switch (ch_num) {
        case 1:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_MONO);
            break;
        case 2:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_STEREO);
            break;
        case 3:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R);
            break;
        case 4:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_R_SL_RS);
            break;
        case 5:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_SL_SR);
            break;
        case 6:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_5_1);
            break;
        case 8:
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_7_1);
            break;
        /*default:
            ALOGE("unsupport yet");
            break;*/
    }
    sysfs_set_sysfs_str(REPORT_DECODED_INFO, sysfs_buf);
}

int aml_decoder_ad_process(struct audio_stream_out *stream, struct audio_buffer *abuffer, int *used_bytes)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    dec_fun = get_decoder_function(out->hal_format);
    *used_bytes = 0;
    if (dec_fun == NULL) {
        ALOGW("[%s:%d] get_decoder_function format:%#x is null", __func__, __LINE__, out->hal_format);
        return -1;
    }

    if (dec_fun->f_ad_process) {
        ret = dec_fun->f_ad_process(out->aml_dec, abuffer);
    } else {
        ALOGE("[%s:%d] f_ad_process is null", __func__, __LINE__);
        return -1;
    }
    if (ret >= 0 ) {
       *used_bytes = ret;
       return AML_DEC_RETURN_TYPE_OK;
    } else {
       return ret;
    }

}

int aml_decoder_process(aml_dec_t *aml_dec, struct audio_buffer *abuffer, int *used_bytes)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    int fill_bytes = 0;
    int parser_raw = 0;
    int offset = 0;
    int n_bytes_spdifdec_consumed = 0;
    void *payload_addr = NULL;
    int32_t n_bytes_payload = 0;
    unsigned char *spdif_src = NULL;
    int spdif_offset = 0;
    int frame_size = 0;
    int fragment_size = 0;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;

    *used_bytes = 0;
    if (aml_dec == NULL) {
        ALOGE("[%s:%d] aml_dec is null", __func__, __LINE__);
        return -1;
    }

    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        ALOGW("[%s:%d] get_decoder_function format:%#x is null", __func__, __LINE__, aml_dec->format);
        return -1;
    }
    /*if we have fragment size output*/
    if (aml_dec->fragment_left_size > 0) {
        ALOGI("[%s:%d] fragment_left_size=%d ", __func__, __LINE__, aml_dec->fragment_left_size);
        frame_size = audio_bytes_per_sample(dec_pcm_data->data_format) * dec_pcm_data->data_ch;
        fragment_size = AML_DEC_FRAGMENT_FRAMES * frame_size;
        memmove(dec_pcm_data->buf, (unsigned char *)dec_pcm_data->buf + fragment_size, aml_dec->fragment_left_size);
        memmove(dec_raw_data->buf, (unsigned char *)dec_raw_data->buf + fragment_size, aml_dec->fragment_left_size);

        if (aml_dec->fragment_left_size >= fragment_size) {
            dec_pcm_data->data_len = fragment_size;
            dec_raw_data->data_len = fragment_size;
            aml_dec->fragment_left_size -= fragment_size;
        } else {
            dec_pcm_data->data_len = aml_dec->fragment_left_size;
            dec_raw_data->data_len = aml_dec->fragment_left_size;
            aml_dec->fragment_left_size = 0;
        }
        *used_bytes = 0;
        return 0;
    }

    dec_pcm_data->data_len = 0;
    dec_raw_data->data_len = 0;
    raw_in_data->data_len = 0;

    if (dec_fun->f_process) {
        ret = dec_fun->f_process(aml_dec, abuffer);
    } else {
        ALOGE("[%s:%d] f_process is null", __func__, __LINE__);
        return -1;
    }

    if (access(REPORT_DECODED_INFO, F_OK) == 0) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        char info_buf[MAX_BUFF_LEN] = {0};
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);

        sprintf(info_buf, "bitrate %d", dec_info.dec_info.stream_bitrate);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "ch_num %d", dec_info.dec_info.stream_ch);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "samplerate %d", dec_info.dec_info.stream_sr);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "decoded_frames %d", dec_info.dec_info.stream_decode_num);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "decoded_err %d", dec_info.dec_info.stream_error_num);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        sprintf(info_buf, "decoded_drop %d", dec_info.dec_info.stream_drop_num);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
        memset(info_buf, 0x00, MAX_BUFF_LEN);
        UpdateDecodeInfo_ChannelConfiguration(info_buf, dec_info.dec_info.stream_ch);
    }

    frame_size = audio_bytes_per_sample(dec_pcm_data->data_format) * dec_pcm_data->data_ch;
    /*one decoded frame length is too big, we need separate it*/
    if ((dec_pcm_data->data_len >= AML_DEC_MAX_FRAMES * frame_size) &&
        (dec_raw_data->data_format == AUDIO_FORMAT_IEC61937) &&
        (dec_raw_data->data_len == dec_pcm_data->data_len)) {
        fragment_size = AML_DEC_FRAGMENT_FRAMES * frame_size;
        aml_dec->fragment_left_size = dec_pcm_data->data_len - fragment_size;
        dec_pcm_data->data_len = fragment_size;
        dec_raw_data->data_len = fragment_size;
    }

    if (ret >= 0 ) {
      *used_bytes = ret;
       return AML_DEC_RETURN_TYPE_OK;
    } else {
       *used_bytes = abuffer->size;
       return ret;
    }

}

void aml_decoder_calc_coefficient(unsigned char ad_fade,float * mix_coefficient,float * ad_coefficient)
{
            #define MAIN_MIXER_VAL (0.8709635900f)
            #define AD_MIXER_VAL (0.4897788194f)
            float mixing_coefficient = MAIN_MIXER_VAL;
            float ad_mixing_coefficient = AD_MIXER_VAL;
            if (ad_fade == 0)
            {
                //mixing_coefficient = 1.0f;
                //ad_mixing_coefficient = 1.0f;
            }
            else if (ad_fade == 0xFF)
            {
                mixing_coefficient = 0.0f;
                ad_mixing_coefficient = 0.0f;
            }
            else if ((ad_fade > 0) && (ad_fade < 0xff))
            {
                mixing_coefficient = (1.0f-(float)(ad_fade)/256)*MAIN_MIXER_VAL;
                ad_mixing_coefficient = (1.0f-(float)(ad_fade)/256)*AD_MIXER_VAL;
            }
            *mix_coefficient = mixing_coefficient;
            *ad_coefficient = ad_mixing_coefficient;
}
eDTSLibType_t detect_dts_lib_type(void)
{
    void *hDTSLibHanle = NULL;
    // the priority would be "DTSX > DTSHD" lib
    // DTSX is first priority
    if (access(DTS_X_LIB_PATH_A, R_OK) == 0) {
        // try to open lib see if it's OK?
        hDTSLibHanle = dlopen(DTS_X_LIB_PATH_A, RTLD_NOW);
        if (hDTSLibHanle != NULL) {
            dlclose(hDTSLibHanle);
            hDTSLibHanle = NULL;
            gDtsLibType = eDTSXLib;
            ALOGI("[%s:%d] Found libHwAudio_dtsx lib", __func__, __LINE__);
            return eDTSXLib;
        }
    }
    // DTSHD is second priority
    if (access(DTS_HD_LIB_PATH_A, R_OK) == 0) {
        // try to open lib see if it's OK?
        hDTSLibHanle = dlopen(DTS_HD_LIB_PATH_A, RTLD_NOW);
        if (hDTSLibHanle != NULL) {
            dlclose(hDTSLibHanle);
            hDTSLibHanle = NULL;
            gDtsLibType = eDTSHDLib;
            ALOGI("[%s:%d] Found libHwAudio_dtshd lib", __func__, __LINE__);
            return eDTSHDLib;
        }
    }
    ALOGW("[%s:%d] Failed to find libHwAudio_dtsx.so and libHwAudio_dtshd.so, %s", __FUNCTION__, __LINE__, dlerror());
    return eDTSNull;
}
eDTSLibType_t get_dts_lib_type(void)
{
    return gDtsLibType;
}