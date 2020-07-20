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
#define LOG_TAG "aml_spdif_output"

#include <cutils/log.h>
#include <inttypes.h>
#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "dolby_lib_api.h"
#include "alsa_device_parser.h"
#include "spdifenc_wrap.h"
#include "alsa_config_parameters.h"
#include "aml_audio_spdif_output.h"

void  aml_audio_spdif_output_stop(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct pcm *pcm = aml_dev->pcm_handle[DIGITAL_DEVICE];
    if (pcm) {
        pcm_close(pcm);
        aml_tinymix_set_spdif_format(AUDIO_FORMAT_PCM_16_BIT, aml_out);
        aml_dev->pcm_handle[DIGITAL_DEVICE] = NULL;
        aml_dev->raw_to_pcm_flag = true;
        aml_dev->spdif_out_format = 0;
        aml_dev->spdif_out_rate   = 0;
        aml_dev->dual_spdifenc_inited = 0;
        ALOGI("%s done,pcm handle %p \n", __func__, pcm);
    }
}

int  aml_audio_spdif_output_start(struct audio_stream_out *stream, audio_format_t output_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct pcm *pcm = aml_dev->pcm_handle[DIGITAL_DEVICE];
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    struct pcm_config config;

    if (!pcm) {
        /* init pcm configs, no DDP case in dual output */
        memset(&config, 0, sizeof(struct pcm_config));
        config.channels = 2;
        if (eDolbyDcvLib == aml_dev->dolby_lib_type) {
            config.rate = aml_out->config.rate;
        } else {
            config.rate = MM_FULL_POWER_SAMPLING_RATE;
        }
        // reset the sample rate
        if (patch && (patch->input_src == AUDIO_DEVICE_IN_HDMI || patch->input_src == AUDIO_DEVICE_IN_SPDIF)) {
            if (patch->aformat == AUDIO_FORMAT_DTS ||
                patch->aformat == AUDIO_FORMAT_DTS_HD) {
                config.rate = aml_out->config.rate;
                //ALOGD("rate=%d setrate=%d\n",config.rate,aml_out->config.rate);
            }
        }
        /* SWPL-19631 increase spdif period_size to reduce spdifout delay when spdif/tdm dual output */
        config.period_size = DEFAULT_PLAYBACK_PERIOD_SIZE * 2 * 2;
        config.period_count = PLAYBACK_PERIOD_COUNT;
        config.start_threshold = DEFAULT_PLAYBACK_PERIOD_SIZE * 2 * PLAYBACK_PERIOD_COUNT;
        config.format = PCM_FORMAT_S16_LE;
        if ((aml_dev->ms12.dolby_ms12_enable) && !is_bypass_dolbyms12(stream)) {
            struct dolby_ms12_desc *ms12 = &(aml_dev->ms12);
            get_hardware_config_parameters(
                &(config)
                , output_format
                , audio_channel_count_from_out_mask(ms12->output_channelmask)
                , ms12->output_samplerate
                , aml_out->is_tv_platform
                , continous_mode(aml_dev));
        }
        aml_tinymix_set_spdif_format(output_format, aml_out);
        pthread_mutex_lock(&aml_dev->alsa_pcm_lock);
        unsigned int port = PORT_SPDIF;
        port = alsa_device_update_pcm_index(port, PLAYBACK);
        pcm = pcm_open(aml_out->card, port, PCM_OUT, &config);
        if (!pcm_is_ready(pcm)) {
            ALOGE("%s() cannot open pcm_out: %s,card %d,device %d", __func__, pcm_get_error(pcm), aml_out->card, DIGITAL_DEVICE);
            pcm_close(pcm);
            pthread_mutex_unlock(&aml_dev->alsa_pcm_lock);
            return -ENOENT;
        }
        ALOGI("%s open  output pcm handle %p,port %d", __func__, pcm, port);
        aml_dev->pcm_handle[DIGITAL_DEVICE] = pcm;
        aml_dev->spdif_out_format           = output_format;
        aml_dev->spdif_out_rate             = config.rate;
        pthread_mutex_unlock(&aml_dev->alsa_pcm_lock);

    }

    return 0;
}


ssize_t aml_audio_spdif_output_direct(struct audio_stream_out *stream,
                                      void *buffer, size_t byte, audio_format_t output_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct pcm *pcm = aml_dev->pcm_handle[DIGITAL_DEVICE];
    int ret = 0;
    if (aml_dev->spdif_out_format != output_format) {
        if (pcm) {
            pcm_close(pcm);
            aml_dev->pcm_handle[DIGITAL_DEVICE] = NULL;
            aml_dev->spdif_out_format = 0;
            aml_dev->spdif_out_rate   = 0;
            pcm = NULL;
        }
        ALOGI("%s ,format change from 0x%x to 0x%x,need reinit alsa\n", __func__, aml_dev->spdif_out_format, output_format);
    }

    if (!pcm) {
        ret = aml_audio_spdif_output_start(stream, output_format);
        pcm = aml_dev->pcm_handle[DIGITAL_DEVICE];
        if (ret != 0) {
            return ret;
        }
    }

    if (pcm) {
        {
            struct snd_pcm_status status;
            pcm_ioctl(pcm, SNDRV_PCM_IOCTL_STATUS, &status);
            if (status.state == PCM_STATE_XRUN) {
                ALOGW("%s alsa underrun", __func__);
            }
        }
        pthread_mutex_lock(&aml_dev->alsa_pcm_lock);
        ret = pcm_write(pcm, buffer, byte);
        pthread_mutex_unlock(&aml_dev->alsa_pcm_lock);
    }
    return 0;
}


ssize_t aml_audio_spdif_output(struct audio_stream_out *stream,
                               void *buffer, size_t byte, audio_format_t output_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct pcm *pcm = aml_dev->pcm_handle[DIGITAL_DEVICE];
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    int ret = 0;
    int codec_type = 0;
    bool need_reinit_encoder = false;
    if (aml_dev->debug_flag) {
        ALOGI("%s in format %x,size %d\n", __func__, output_format, byte);
    }
    // SWPL-412, when input source is DTV, and UI set "parental_control_av_mute" command to audio hal
    // we need to mute SPDIF audio output here
    if (aml_dev->patch_src == SRC_DTV && aml_dev->parental_control_av_mute) {
        memset(buffer, 0x0, byte);
    }
    if (spdifenc_get_format() != output_format) {
        need_reinit_encoder = true;
        if (pcm) {
            pcm_close(pcm);
            aml_dev->pcm_handle[DIGITAL_DEVICE] = NULL;
            pcm = NULL;
        }
        aml_dev->dual_spdifenc_inited = 0;
        ALOGI("%s ,format change from 0x%x to 0x%x,need reinit spdif encoder and alsa\n", __func__, spdifenc_get_format(), output_format);
    }
    if (!pcm) {
        ret = aml_audio_spdif_output_start(stream, output_format);
        if (ret != 0) {
            return ret;
        }
        pcm = aml_dev->pcm_handle[DIGITAL_DEVICE];
    }

    if (eDolbyDcvLib == aml_dev->dolby_lib_type) {

#ifdef ADD_AUDIO_DELAY_INTERFACE
        // spdif(RAW) delay process, frame size 2 ch * 2 Byte
        aml_audio_delay_process(AML_DELAY_OUTPORT_SPDIF, buffer, byte, AUDIO_FORMAT_IEC61937, 2);
#endif
        pthread_mutex_lock(&aml_dev->alsa_pcm_lock);
        ret = pcm_write(pcm, buffer, byte);
        pthread_mutex_unlock(&aml_dev->alsa_pcm_lock);
        if (ret < 0) {
            ALOGE("%s write failed,pcm handle %p err num %d", __func__, pcm, ret);
        }
        ALOGV("%s(), aml_alsa_output_write bytes = %zu", __func__, byte);

    } else {
        if (!aml_dev->dual_spdifenc_inited) {
            if ((eDolbyMS12Lib == aml_dev->dolby_lib_type) && (!aml_dev->is_TV)) {
                // when use ms12 on BOX case, we also need to allow EAC3 output from SPDIF port .zzz
                if (output_format != AUDIO_FORMAT_AC3 &&
                    output_format != AUDIO_FORMAT_E_AC3 &&
                    output_format != AUDIO_FORMAT_MAT) {
                    ALOGE("%s() not support, optical format = %#x",
                          __func__, output_format);
                    return -EINVAL;
                }
            }

            int init_ret = spdifenc_init(pcm, output_format);
            if (init_ret == 0) {
                aml_dev->dual_spdifenc_inited = 1;
                aml_tinymix_set_spdif_format(output_format, aml_out);
                spdifenc_set_mute(aml_out->offload_mute);
                //ALOGI("%s tinymix AML_MIXER_ID_SPDIF_FORMAT %d\n", __FUNCTION__, AML_DOLBY_DIGITAL);
            }
        }
        {
            struct snd_pcm_status status;
            pcm_ioctl(pcm, SNDRV_PCM_IOCTL_STATUS, &status);
            if (status.state == PCM_STATE_XRUN) {
                ALOGW("%s alsa underrun", __func__);
            }
        }
        ret = spdifenc_write(buffer, byte);
        ALOGV("%s(), spdif write bytes = %d", __func__, ret);
    }
    return ret;
}
