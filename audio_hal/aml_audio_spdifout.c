/*
 * Copyright (C) 2021 The Android Open Source Project
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

#define LOG_TAG "audio_hw_output_spdif"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <cutils/log.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>

#include "audio_hw.h"
#include "alsa_device_parser.h"
#include "aml_audio_spdifout.h"
#include "dtv_patch.h"
#include "spdif_encoder_api.h"
#include "audio_hw_utils.h"
#include "alsa_manager.h"
#include "dolby_lib_api.h"
#include "amlAudioMixer.h"

typedef enum {
    MC_POST_PROCESS_NONE            = 0,
    MC_POST_PROCESS_SWAP_C_LFE      = 1,
    MC_POST_PROCESS_EXTEND_CHANNEL  = 2,
    MC_POST_PROCESS_COMPACT_CHANNEL = 3,
    MC_POST_PROCESS_MAX             = 4
} mc_post_process_type_e;

typedef struct spdifout_handle {
    int device_id; /*used for refer aml_dev->alsa_handle*/
    int spdif_port;
    audio_format_t audio_format;
    bool need_spdif_enc;
    bool spdif_enc_init;
    void *spdif_enc_handle;
    bool b_mute;
    int  out_data_ch;        // it is used for alsa channel
    int  in_data_ch;         //save the input data channel
    audio_channel_mask_t channel_mask;
    bool spdif_mute;
    uint32_t sample_rate;
    mc_post_process_type_e post_process_type;
    size_t buf_size;
    void * temp_buf;
    bool restore_hdmitx_selection;
} spdifout_handle_t;

typedef size_t (*mc_post_process_func)(void *input, size_t size, void *output);

static size_t mc_swap_c_lfe(void *input, size_t bytes, void *output);
static size_t mc_compact_channel(void *input, size_t bytes, void *output);

static mc_post_process_func mc_post_process_funcs[MC_POST_PROCESS_MAX] = {
    NULL,
    mc_swap_c_lfe,
    NULL,
    mc_compact_channel
};

static int select_digital_device(struct spdifout_handle *phandle) {
    int device_id = DIGITAL_DEVICE;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    /*
     *  DIGITAL_DEVICE  --> spdif_a(spdif)
     *  DIGITAL_DEVICE2 --> spdif_b,
     *  TV and STB has different hardware config for spdif/spdif_b
     *
     */

    AM_LOGI("dual_spdif_support:%d", aml_dev->dual_spdif_support);
    if (!aml_dev->is_TV) {
        if (aml_dev->dual_spdif_support) {
            if (phandle->audio_format == AUDIO_FORMAT_AC3 ||
                phandle->audio_format == AUDIO_FORMAT_DTS) {
                /*if it is dd/dts, we use spdif_a, then it can output to spdif/hdmi at the same time*/
                device_id = DIGITAL_DEVICE;
            } else {
                /*for ddp, we need use spdif_b, then select hdmi to spdif_b, then spdif can output dd*/
                device_id = DIGITAL_DEVICE2;
            }
        } else {
            /*defaut we only use spdif_a to output spdif/hdmi*/
            device_id = DIGITAL_DEVICE;
        }
        if (audio_is_linear_pcm(phandle->audio_format)) {
            if (phandle->channel_mask == AUDIO_CHANNEL_OUT_5POINT1 || phandle->channel_mask == AUDIO_CHANNEL_OUT_7POINT1)
                device_id = TDM_DEVICE; /* TDM_DEVICE <-> i2stohdmi only support multi-channel */
            else
                device_id = DIGITAL_DEVICE;
        }
    } else {
        if (aml_dev->dual_spdif_support) {
            int device_index = alsa_device_update_pcm_index(PORT_EARC, PLAYBACK);
            AM_LOGI("device_index:%d, audio_format:0x%x, in_data_ch:%d optical_format:0x%x", device_index, phandle->audio_format, phandle->in_data_ch, aml_dev->optical_format);
            /*we have arc/earc port*/
            if (device_index != -1) {
                /*TV spdif_a support arc/spdif, spdif_b only support spdif
                 *ddp always used spdif_a
                 */
                if (phandle->audio_format == AUDIO_FORMAT_E_AC3 ||
                    phandle->audio_format == AUDIO_FORMAT_MAT ||
                    (audio_is_linear_pcm(phandle->audio_format) && (phandle->in_data_ch == 8 || phandle->in_data_ch == 6))) {
                    device_id = EARC_DEVICE;
                } else if (phandle->audio_format == AUDIO_FORMAT_AC3) {
                    if (aml_dev->optical_format == AUDIO_FORMAT_E_AC3) {
                        /*it has dual output, then dd use spdif_b for spdif only*/
                        device_id = DIGITAL_DEVICE2;
                    } else {
                        /*it doesn't have dual output, then dd use spdif_a for arc/spdif*/
                        device_id = DIGITAL_DEVICE;
                    }
                } else {
                    device_id = DIGITAL_DEVICE;
                }
            } else {
                /*TV spdif_a support arc/spdif, spdif_b only support spdif
                 *ddp always used spdif_a
                 */
                if (phandle->audio_format == AUDIO_FORMAT_E_AC3) {
                    device_id = DIGITAL_DEVICE;
                } else if (phandle->audio_format == AUDIO_FORMAT_AC3) {
                    if (aml_dev->optical_format == AUDIO_FORMAT_E_AC3) {
                        /*it has dual output, then dd use spdif_b for spdif only*/
                        device_id = DIGITAL_DEVICE2;
                    } else {
                        /*it doesn't have dual output, then dd use spdif_a for arc/spdif*/
                        device_id = DIGITAL_DEVICE;
                    }
                } else {
                    device_id = DIGITAL_DEVICE;
                }
            }

        } else {
            /*defaut we only use spdif_a to output spdif/arc*/
            device_id = DIGITAL_DEVICE;
            int device_index = alsa_device_update_pcm_index(PORT_EARC, PLAYBACK);
            if (device_index != -1) {
                /* TV which supports earc prefers it as output device */
                if (phandle->audio_format == AUDIO_FORMAT_E_AC3 ||
                    phandle->audio_format == AUDIO_FORMAT_MAT ||
                    (audio_is_linear_pcm(phandle->audio_format) && (phandle->in_data_ch == 8 || phandle->in_data_ch == 6)) ||
                    (phandle->audio_format == AUDIO_FORMAT_DTS_HD && (phandle->in_data_ch == 8))) {
                    device_id = EARC_DEVICE;
                }
            }
            AM_LOGI("device_id:%d, device_index:%d", device_id, device_index);
        }
    }
    AM_LOGI("return device_id:%d", device_id);
    return device_id;
}

static eMixerEARC_Channel_Allocation convert_chmask_to_ca(int channel_mask) {
    eMixerEARC_Channel_Allocation earc_ca = AML_EARC_CHANNEL_REFER_TO_CHANNEL_INDEX;
    switch (channel_mask) {
        case AUDIO_CHANNEL_OUT_STEREO: {
            earc_ca = AML_EARC_CHANNEL_FL_FR;
            break;
        }
        case AUDIO_CHANNEL_OUT_2POINT1: {
            earc_ca = AML_EARC_CHANNEL_FL_FR_LFE1;
            break;
        }
        case AUDIO_CHANNEL_OUT_TRI: {
            earc_ca = AML_EARC_CHANNEL_FL_FR_FC;
            break;
        }
        case AUDIO_CHANNEL_OUT_3POINT1: {
            earc_ca = AML_EARC_CHANNEL_FL_FR_LFE1_FC;
            break;
        }
        case AUDIO_CHANNEL_OUT_5POINT1: {
            earc_ca = AML_EARC_CHANNEL_FL_FR_LFE1_FC_LS_RS;
            break;
        }
        case AUDIO_CHANNEL_OUT_7POINT1: {
            earc_ca = AML_EARC_CHANNEL_FL_FR_LFE1_FC_LS_RS_RLC_RRC;
            break;
        }
        default:
            ALOGE("%s unsupport channel mask = 0x%x", __func__, channel_mask);
            break;
    }
    return earc_ca;
}

int aml_audio_get_spdif_port(eMixerSpdif_Format spdif_format)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int pcm_index = -1;
    int spdif_port = PORT_SPDIF;

    /*
     * there are 3 cases:
       1. Soc deosn't support dual spdif, and its spdif port name is PORT_SPDIF
       2. Soc supports dual spdif, but it doesn't have spdif interface
       3. Soc supports dual spdif, and it has spdif interface
          spdif_a can be connected to spdif & hdmi, its name is PORT_SPDIF
          spdif_b only can be conncted to hdmi, its name is PORT_SPDIFB
     */
    if (aml_dev->dual_spdif_support) {
        /*it means we have spdif_a & spdif_b & spdif out interface*/
        if ((spdif_format == AML_DOLBY_DIGITAL) ||
            (spdif_format == AML_DTS)) {
            /*these data can be transfer to spdif*/
            spdif_port = PORT_SPDIF;
        } else {
            spdif_port = PORT_SPDIFB;
        }
    } else {
        /*we try to get the right spdif pcm alsa port*/
        pcm_index = alsa_device_update_pcm_index(PORT_SPDIF, PLAYBACK);
        if (pcm_index != -1) {
            /*we have spdif*/
            spdif_port = PORT_SPDIF;
        } else {
            spdif_port = -1;
        }
    }
    return spdif_port;
}


int aml_audio_get_spdifa_port(void)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int pcm_index = -1;
    int spdif_port = PORT_SPDIF;

    pcm_index = alsa_device_update_pcm_index(PORT_SPDIF, PLAYBACK);
    if (pcm_index != -1) {
        /*we have spdif*/
        spdif_port = PORT_SPDIF;
    } else {
        spdif_port = -1;
    }

    return spdif_port;
}

void aml_audio_set_spdif_format(int spdif_port, eMixerSpdif_Format aml_spdif_format, struct aml_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int spdif_format_ctr_id = AML_MIXER_ID_SPDIF_FORMAT;
    int spdif_to_hdmi_select = AML_SPDIF_A_TO_HDMITX;
    if ((spdif_port != PORT_SPDIF) && (spdif_port != PORT_SPDIFB) && spdif_port != PORT_I2S2HDMI) {
        return;
    }

    /* i2s2hdmi multi-ch use spdifa fmt too */
    if (spdif_port == PORT_SPDIF) {
        spdif_format_ctr_id = AML_MIXER_ID_SPDIF_FORMAT;
    } else if (spdif_port == PORT_SPDIFB) {
        spdif_format_ctr_id = AML_MIXER_ID_SPDIF_B_FORMAT;
    } else if (spdif_port == PORT_I2S2HDMI) {
        spdif_format_ctr_id = AML_MIXER_ID_I2S2HDMI_FORMAT;
    }

    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, spdif_format_ctr_id, aml_spdif_format);

    /*use same source for normal pcm case*/
    if (aml_spdif_format == AML_STEREO_PCM) {
        //aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_AUDIO_SRC_TO_HDMI,  AML_SPDIF_A_TO_HDMITX);
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, aml_spdif_format);
        //aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, aml_spdif_format);
    } else if (aml_spdif_format == AML_MULTI_CH_LPCM) {
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_I2S2HDMI_FORMAT, aml_spdif_format);
    }
    ALOGI("%s tinymix spdif_port:%d, SPDIF_FORMAT:%d", __func__, spdif_port, aml_spdif_format);
    return;
}

void aml_audio_select_spdif_to_hdmi(int src_select)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    if (src_select >= AML_INVALID_TO_HDMITX) {
        ALOGE("%s invalid hdmi src =%d", __func__, src_select);
        return;
    }
    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_AUDIO_SRC_TO_HDMI,  src_select);

    return;
}

static int spdifout_support_format(audio_format_t audio_format)
{
    switch (audio_format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD:
    case AUDIO_FORMAT_MAT:
    case AUDIO_FORMAT_DOLBY_TRUEHD:
    case AUDIO_FORMAT_IEC61937:
    case AUDIO_FORMAT_PCM_16_BIT:
        return true;
    default:
        return false;
    }
}

int aml_audio_spdifout_open(void **pphandle, spdif_config_t *spdif_config)
{
    int ret = -1;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    void *alsa_handle = NULL;
    struct spdifout_handle *phandle = NULL;
    int device_id = 0;
    int aml_spdif_format = AML_STEREO_PCM;
    audio_format_t audio_format = AUDIO_FORMAT_PCM_16_BIT;
    int aml_arc_format = AML_AUDIO_CODING_TYPE_STEREO_LPCM;

    if (spdif_config == NULL) {
        ALOGE("%s spdif_config is NULL", __func__);
        return -1;
    }

    if (!spdifout_support_format(spdif_config->audio_format)) {
        ALOGE("%s format not support =0x%x", __FUNCTION__, audio_format);
        return -1;
    }

    phandle = (struct spdifout_handle *) aml_audio_calloc(1, sizeof(struct spdifout_handle));
    if (phandle == NULL) {
        ALOGE("%s malloc failed\n", __FUNCTION__);
        goto error;
    }

    if (spdif_config->audio_format == AUDIO_FORMAT_IEC61937) {
        phandle->need_spdif_enc = 0;
        audio_format = spdif_config->sub_format;
    } else if (audio_is_linear_pcm(spdif_config->audio_format)) {
        phandle->need_spdif_enc = 0;
        audio_format = spdif_config->audio_format;
    } else {
        phandle->need_spdif_enc = 1;
        audio_format = spdif_config->audio_format;
    }
    phandle->audio_format = audio_format;
    phandle->channel_mask = spdif_config->channel_mask;
    phandle->sample_rate = spdif_config->rate;

    if (!phandle->spdif_enc_init && phandle->need_spdif_enc) {
        ret = aml_spdif_encoder_open(&phandle->spdif_enc_handle, phandle->audio_format);
        if (ret) {
            ALOGE("%s() aml_spdif_encoder_open failed", __func__);
            goto error;
        }
        phandle->spdif_enc_init = true;
    }

    device_id = select_digital_device(phandle);

    alsa_handle = aml_dev->alsa_handle[device_id];

    if (!alsa_handle) {
        aml_stream_config_t stream_config;
        aml_device_config_t device_config;
        memset(&stream_config, 0, sizeof(aml_stream_config_t));
        memset(&device_config, 0, sizeof(aml_device_config_t));
        /*config stream info*/
        stream_config.config.channel_mask = spdif_config->channel_mask;
        /*earc only supports 8 channel multi channel, if the channel is not 2 and 8, we need convert it to 8 channel*/
        if (EARC_DEVICE == device_id) {
            if (spdif_config->data_ch == 2 || spdif_config->data_ch == 8) {
                stream_config.config.channel_mask = audio_channel_out_mask_from_count(spdif_config->data_ch);
                if (spdif_config->data_ch == 8) {
                    phandle->post_process_type = MC_POST_PROCESS_SWAP_C_LFE;
                }
            } else if (spdif_config->data_ch > 2 && spdif_config->data_ch < 8) {
                stream_config.config.channel_mask = audio_channel_out_mask_from_count(8);
                phandle->out_data_ch = 8;
                phandle->post_process_type = MC_POST_PROCESS_EXTEND_CHANNEL;
            } else {
                ALOGE("%s EARC not support channel %d", __func__, spdif_config->data_ch);
                goto error;
            }

        } else if (TDM_DEVICE == device_id) {
            /* MC PCM output from MS12 is hard coded to 8ch */
            if (spdif_config->data_ch == 8) {
                phandle->post_process_type = MC_POST_PROCESS_COMPACT_CHANNEL;
            }
        }
        stream_config.config.sample_rate  = spdif_config->rate;
        stream_config.config.format       = spdif_config->audio_format;
        stream_config.config.offload_info.format = audio_format;

        device_config.device_port = alsa_device_get_port_index(device_id);
        phandle->spdif_port       = device_config.device_port;

        aml_spdif_format = halformat_convert_to_spdif(audio_format, stream_config.config.channel_mask);
        aml_arc_format   = halformat_convert_to_arcformat(audio_format, stream_config.config.channel_mask);

        /*for dts cd , we can't set the format as dts, we should set it as pcm*/
        if (aml_spdif_format == AML_DTS && spdif_config->is_dtscd) {
            aml_spdif_format = AML_STEREO_PCM;
        }
        AM_LOGI("channel =0x%x rate=%d spdif format =0x%x spdif_port=0x%x",spdif_config->channel_mask, spdif_config->rate, aml_spdif_format, phandle->spdif_port);
        /*set spdif format*/
        if (phandle->spdif_port == PORT_SPDIF) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, aml_spdif_format);
            ALOGI("%s set spdif format 0x%x", __func__, aml_spdif_format);
        } else if (phandle->spdif_port == PORT_I2S2HDMI) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_I2S2HDMI_FORMAT, aml_spdif_format);
            aml_audio_select_spdif_to_hdmi(AML_TDM_C_TO_HDMITX);
            phandle->restore_hdmitx_selection = 1;
            ALOGI("%s set i2s to hdmi format 0x%x", __func__, aml_spdif_format);
        } else if (phandle->spdif_port == PORT_SPDIFB) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, aml_spdif_format);
            aml_audio_select_spdif_to_hdmi(AML_SPDIF_B_TO_HDMITX);
            phandle->restore_hdmitx_selection = 1;
            ALOGI("%s set spdif_b format 0x%x", __func__, aml_spdif_format);
        } else if (phandle->spdif_port == PORT_EARC) {

            if (aml_arc_format == AML_AUDIO_CODING_TYPE_DTS && spdif_config->is_dtscd) {
                aml_arc_format = AML_AUDIO_CODING_TYPE_STEREO_LPCM;
            }
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_EARC_TX_AUDIO_TYPE, aml_arc_format);
            if (aml_arc_format == AML_AUDIO_CODING_TYPE_MULTICH_8CH_LPCM) {
                eMixerEARC_Channel_Allocation earc_ca = convert_chmask_to_ca(spdif_config->channel_mask);
                ALOGI("%s earc channel mask =0x%x earc_ca =0x%x", __func__, spdif_config->channel_mask, earc_ca);
                aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_EARC_TX_CA, earc_ca);
            }

            ALOGI("%s set EARC/ARC format 0x%x", __func__, aml_arc_format);
        } else {
            ALOGI("%s not set spdif format", __func__);
        }

        /*open output alsa*/
        ret = aml_alsa_output_open_new(&alsa_handle, &stream_config, &device_config);
        if (phandle->spdif_port == PORT_SPDIF) {
            /*we have different output for hdmi and spdif, we choose tdm b to hdmi*/
            ALOGI("optical =0x%x sink =0x%x", aml_dev->optical_format, aml_dev->sink_format);
            if (aml_dev->optical_format != aml_dev->sink_format && aml_dev->sink_format == AUDIO_FORMAT_PCM_16_BIT) {
                if (aml_dev->spdif_independent) {
                    aml_audio_select_spdif_to_hdmi(aml_dev->hdmitx_src);
                    phandle->restore_hdmitx_selection = 1;
                }
                aml_dev->raw_to_pcm_flag = true;
            }
        }

        if (ret != 0) {
            goto error;
        }
        aml_dev->alsa_handle[device_id] = alsa_handle;
        ALOGI("dev alsa handle device id=%d handle=%p", device_id, alsa_handle);

    }

    phandle->device_id = device_id;

    *pphandle = (void *)phandle;
    ALOGI("%s success ret=%d format =0x%x", __func__, ret, audio_format);
    return ret;

error:
    if (phandle) {
        if (phandle->spdif_enc_handle) {
            aml_spdif_encoder_close(phandle->spdif_enc_handle);
        }
        if (phandle->spdif_port == PORT_SPDIF) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, AML_STEREO_PCM);
        } else if (phandle->spdif_port == PORT_SPDIFB) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, AML_STEREO_PCM);
            aml_audio_select_spdif_to_hdmi(AML_SPDIF_A_TO_HDMITX);
        }
        aml_audio_free(phandle);
    }
    *pphandle = NULL;
    return -1;


}

/* insert mute frames */
int aml_audio_spdifout_insert_pause(void *phandle, int frames)
{
    int ret = -1;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int device_id = -1;
    void *alsa_handle = NULL;
    void *tmp;

    if (phandle == NULL) {
        return -1;
    }
    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];

    tmp = aml_audio_calloc(1, frames * 4);
    if (tmp) {
        unsigned short *p = (unsigned short *)tmp;
        p[0] = 0xf872;    /* Pa */
        p[1] = 0x4e1f;    /* Pb */
        p[2] = 3;         /* Pc (PAUSE) */
        p[3] = 4;         /* Payload of 32 bits */
                          /* with unspecified gap length */
        ret = aml_alsa_output_write_new(alsa_handle, tmp, frames * 4);
        aml_audio_free(tmp);
    }

    return ret;
}

/* 8ch -> 8ch with C/LFE swap, must support in-place operation */
static size_t mc_swap_c_lfe(void *input, size_t bytes, void *output)
{
    /* input is 8ch AUDIO_FORMAT_PCM_16_BIT */
    short *in = (short *)input;
    short *out = (short *)output;
    int frames = bytes / 8 / sizeof(short);
    int i;

    for (i = 0; i < frames; i++) {
        short t;
        *out++ = *in++;
        *out++ = *in++;
        t = *in++;
        *out++ = *in++;
        *out++ = t;
        *out++ = *in++;
        *out++ = *in++;
        *out++ = *in++;
        *out++ = *in++;
    }

    return frames * 8 * sizeof(short);
}

/* 8ch -> 6ch conversion with C/LFE swap, must support in-place operation */
static size_t mc_compact_channel(void *input, size_t bytes, void *output)
{
    /* input is 8ch AUDIO_FORMAT_PCM_16_BIT */
    short *in = (short *)input;
    short *out = (short *)output;
    int frames = bytes / 8 / sizeof(short);
    int i;

    for (i = 0; i < frames; i++, in+=2) {
        short t;
        *out++ = *in++;
        *out++ = *in++;
        t = *in++;
        *out++ = *in++;
        *out++ = t;
        *out++ = *in++;
        *out++ = *in++;
    }

    return frames * 6 * sizeof(short);
}

int aml_audio_spdifout_process(void *phandle, const void *buffer, size_t byte)
{
    int ret = -1;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    void * output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int device_id = -1;
    bool b_mute = false;
    void *write_p;
    size_t write_size;

    void *alsa_handle = NULL;
    if (phandle == NULL) {
        return -1;
    }

    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];

    if (spdifout_phandle->need_spdif_enc) {
        ret = aml_spdif_encoder_process(spdifout_phandle->spdif_enc_handle, buffer, byte, &output_buffer, &output_buffer_bytes);
        if (ret != 0) {
            ALOGE("%s: spdif encoder process error", __func__);
            return ret;
        }
    } else {
        output_buffer = buffer;
        output_buffer_bytes = byte;
    }
#if 0
    {
        output_info_t output_info = { 0 };
        aml_alsa_output_getinfo(alsa_handle, OUTPUT_INFO_DELAYFRAME, &output_info);
        ALOGI("delay frame =%d\n", output_info.delay_frame);

    }
#endif

#ifdef ADD_AUDIO_DELAY_INTERFACE
    ALOGV("spdif/arc raw delay: format=0x%x, sample rate=%d", spdifout_phandle->audio_format, spdifout_phandle->sample_rate);
    if (spdifout_phandle->audio_format == AUDIO_FORMAT_AC3) {
        aml_audio_delay_process(AML_DELAY_OUTPORT_SPDIF_RAW,
                (void *) output_buffer, output_buffer_bytes, spdifout_phandle->audio_format, spdifout_phandle->sample_rate);
    } else if (spdifout_phandle->audio_format == AUDIO_FORMAT_E_AC3) {
        aml_audio_delay_process(AML_DELAY_OUTPORT_SPDIF_B_RAW,
                (void *) output_buffer, output_buffer_bytes, spdifout_phandle->audio_format, spdifout_phandle->sample_rate);
    } else if (spdifout_phandle->audio_format == AUDIO_FORMAT_MAT) {
        aml_audio_delay_process(AML_DELAY_OUTPORT_SPDIF_B_RAW,
                (void *) output_buffer, output_buffer_bytes, spdifout_phandle->audio_format, spdifout_phandle->sample_rate);
    }/* else {
        ALOGI("Only support spdif/hdmi/arc DD/DDP/MAT raw delay, format=0x%x", spdifout_phandle->audio_format);
    }*/
#endif

    if ((aml_dev->patch_src == SRC_DTV && aml_dev->audio_patch) &&
        (aml_dev->discontinue_mute_flag || aml_dev->start_mute_flag ||
         aml_dev->tv_mute)) {
        b_mute = true;
    }

    if (aml_dev->debug_flag) {
        ALOGI("size =%zu format=%x mute =%d %d",
            output_buffer_bytes, spdifout_phandle->audio_format, b_mute, spdifout_phandle->b_mute);
    }

    if (b_mute || spdifout_phandle->b_mute) {
        memset(output_buffer, 0, output_buffer_bytes);
    }

    write_p = output_buffer;
    write_size = output_buffer_bytes;

    if ((spdifout_phandle->post_process_type >= 0) && (spdifout_phandle->post_process_type < MC_POST_PROCESS_MAX) && mc_post_process_funcs[spdifout_phandle->post_process_type]) {
        switch (spdifout_phandle->post_process_type) {
            case MC_POST_PROCESS_EXTEND_CHANNEL:
                /* TODO: prepare output buffer */
                break;
            case MC_POST_PROCESS_SWAP_C_LFE:
            case MC_POST_PROCESS_COMPACT_CHANNEL:
            default:
                /* in-place */
                break;
        }
        write_size = mc_post_process_funcs[spdifout_phandle->post_process_type](output_buffer, output_buffer_bytes, write_p);
    }

    if (write_size) {
        ret = aml_alsa_output_write_new(alsa_handle, write_p, write_size);
    }

    return ret;
}

int aml_audio_spdifout_close(void *phandle)
{
    int ret = -1;
    int device_id = -1;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    void *alsa_handle = NULL;
    if (phandle == NULL) {
        return -1;
    }

    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];

    if (alsa_handle) {
        ALOGI("%s close spdif output bitstream id=%d handle %p", __func__, device_id, alsa_handle);
        /*when spdif is closed, we need set raw to pcm flag, othwer spdif pcm may have problem*/
        aml_alsa_output_close_new(alsa_handle);
        aml_dev->alsa_handle[device_id] = NULL;
        aml_dev->raw_to_pcm_flag        = true;
    }

    /*it is spdif a output*/
    if (spdifout_phandle->spdif_port == PORT_SPDIF) {
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, AML_STEREO_PCM);
    } else if (spdifout_phandle->spdif_port == PORT_I2S2HDMI) {
        /*i2s to  hdmitx case */
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_I2S2HDMI_FORMAT, AML_STEREO_PCM);
    }
    if (spdifout_phandle->restore_hdmitx_selection) {
        aml_audio_select_spdif_to_hdmi(AML_SPDIF_A_TO_HDMITX);
    }

    if (aml_dev->useSubMix) {
        subMixingOutputRestart(aml_dev);
        ALOGI("%s reset submix", __func__);
    }

    if (spdifout_phandle) {
        if (spdifout_phandle->spdif_enc_handle) {
            ret = aml_spdif_encoder_close(spdifout_phandle->spdif_enc_handle);
        }
        aml_audio_free(spdifout_phandle);
    }
    return ret;
}

int aml_audio_spdifout_mute(void *phandle, bool b_mute) {
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    if (phandle == NULL) {
        ALOGE("[%s:%d] invalid param, phandle:%p", __func__, __LINE__, phandle);
        return -1;
    }

    spdifout_phandle->b_mute = b_mute;
    ALOGV("%s mute:%d", __FUNCTION__, b_mute);
    return 0;
}

int aml_audio_spdifout_pause(void *phandle) {
    int ret = 0;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int device_id = -1;
    void *alsa_handle = NULL;

    if (phandle == NULL) {
        ALOGE("[%s:%d] invalid param, phandle:%p", __func__, __LINE__, phandle);
        return -1;
    }
    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];
    ret = aml_alsa_output_pause_new(alsa_handle);

    return ret;
}

int aml_audio_spdifout_resume(void *phandle) {
    int ret = 0;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int device_id = -1;
    void *alsa_handle = NULL;

    if (phandle == NULL) {
        ALOGE("[%s:%d] invalid param, phandle:%p", __func__, __LINE__, phandle);
        return -1;
    }
    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];

    ret = aml_alsa_output_resume_new(alsa_handle);

    return ret;
}

int aml_audio_spdifout_get_delay(void *phandle) {
    int ret = 0;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int device_id = -1;
    void *alsa_handle = NULL;
    int delay_ms = 0;
    if (phandle == NULL) {
        ALOGE("[%s:%d] invalid param, phandle:%p", __func__, __LINE__, phandle);
        return -1;
    }
    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];

    ret = aml_alsa_output_getinfo(alsa_handle, OUTPUT_INFO_DELAYFRAME, (alsa_output_info_t *)&delay_ms);
    if (ret < 0) {
        return -1;
    }
    return delay_ms;
}

int aml_audio_spdifout_config_earc_ca(void *phandle, int channel_mask) {
    int ret = 0;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int device_id = -1;
    void *alsa_handle = NULL;
    if (phandle == NULL) {
        ALOGE("[%s:%d] invalid param, phandle:%p", __func__, __LINE__, phandle);
        return -1;
    }
    if ((spdifout_phandle->channel_mask != channel_mask)
        && (spdifout_phandle->spdif_port == PORT_EARC)
        && (spdifout_phandle->out_data_ch == 8)) {
        eMixerEARC_Channel_Allocation earc_ca = convert_chmask_to_ca(channel_mask);
        ALOGI("%s earc new channel mask =0x%x earc_ca =0x%x", __func__, channel_mask, earc_ca);
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_EARC_TX_CA, earc_ca);
        spdifout_phandle->channel_mask = channel_mask;
    }

    return ret;
}

