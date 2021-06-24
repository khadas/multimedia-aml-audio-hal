/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <linux/ioctl.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include <aml_android_utils.h>

#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "aml_audio_stream.h"
#include "aml_avsync_tuning.h"
#include "alsa_manager.h"
#include "dolby_lib_api.h"


/**
 * store src_buffer data to spker_tuning_rbuf,
 * retrieve former saved data after latency requirement is met.
 */
int tuning_spker_latency(struct aml_audio_device *adev,
                         int16_t *sink_buffer, int16_t *src_buffer, size_t bytes)
{
    ring_buffer_t *rbuf = &(adev->spk_tuning_rbuf);
    uint latency_bytes = adev->spk_tuning_lvl;
    uint ringbuf_lvl = 0;
    uint ret = 0;

    if (!sink_buffer || !src_buffer) {
        ALOGE("%s(), NULL pointer!", __func__);
        return 0;
    }

    /* clear last sink buffers */
    memset((void*)sink_buffer, 0, bytes);
    /* get buffered data from ringbuf head */
    ringbuf_lvl = get_buffer_read_space(rbuf);
    if (ringbuf_lvl == latency_bytes) {
        if (ringbuf_lvl >= bytes) {
            ret = ring_buffer_read(rbuf, (unsigned char*)sink_buffer, bytes);
            if (ret != (uint)bytes) {
                ALOGE("%s(), ringbuf read fail.", __func__);
                goto err;
            }
            ret = ring_buffer_write(rbuf, (unsigned char*)src_buffer, bytes, UNCOVER_WRITE);
            if (ret != (uint)bytes) {
                ALOGE("%s(), ringbuf write fail.", __func__);
                goto err;
            }
        } else {
            ret = ring_buffer_write(rbuf, (unsigned char*)src_buffer, bytes, UNCOVER_WRITE);
            if (ret != (uint)bytes) {
                ALOGE("%s(), ringbuf write fail..", __func__);
                goto err;
            }
            ret = ring_buffer_read(rbuf, (unsigned char*)sink_buffer, bytes);
            if (ret != (uint)bytes) {
                ALOGE("%s(), ringbuf read fail..", __func__);
                goto err;
            }
        }
    } else if (ringbuf_lvl < latency_bytes) {
        if (ringbuf_lvl + bytes < latency_bytes) {
            /* all data need to store in tmp buf */
            ret = ring_buffer_write(rbuf, (unsigned char*)src_buffer, bytes, UNCOVER_WRITE);
            if (ret != (uint)bytes) {
                ALOGE("%s(), ringbuf write fail...", __func__);
                goto err;
            }
        } else {
            /* output former data, and store new */
            uint out_bytes = ringbuf_lvl + bytes - latency_bytes;
            uint offset = bytes - out_bytes;

            if (ringbuf_lvl >= out_bytes) {
                /* first fetch data and then store the new */
                ret = ring_buffer_read(rbuf, (unsigned char*)sink_buffer + offset, out_bytes);
                if (ret != (uint)out_bytes) {
                    ALOGE("%s(), ringbuf read fail...", __func__);
                    goto err;
                }
                ret = ring_buffer_write(rbuf, (unsigned char*)src_buffer, bytes, UNCOVER_WRITE);
                if (ret != (uint)bytes) {
                    ALOGE("%s(), ringbuf write fail...", __func__);
                    goto err;
                }
            } else {
                /* first store the data then fetch */
                ret = ring_buffer_write(rbuf, (unsigned char*)src_buffer, bytes, UNCOVER_WRITE);
                if (ret != (uint)bytes) {
                    ALOGE("%s(), ringbuf write fail", __func__);
                    goto err;
                }
                ret = ring_buffer_read(rbuf, (unsigned char*)sink_buffer + offset, out_bytes);
                if (ret != (uint)out_bytes) {
                    ALOGE("%s(), ringbuf read fail....", __func__);
                    goto err;
                }
            }
        }
    } else {
        ALOGE("%s(), abnormal case, CHECK data flow please!", __func__);
        goto err;
    }
    return 0;

err:
    memcpy(sink_buffer, src_buffer, bytes);
    return ret;
}

static void check_skip_frames(struct aml_audio_device *aml_dev)
{
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    snd_pcm_sframes_t frames = 0;
    struct pcm *pcm_handle = aml_dev->pcm_handle[I2S_DEVICE];
    struct pcm *pcm_handle_spdif = aml_dev->pcm_handle[DIGITAL_DEVICE];

    int alsa_out_i2s_ltcy = -1;
    patch->skip_frames = 0;
    /* for spk, check i2s device latency */
    if (pcm_handle && patch && patch->need_do_avsync == true &&
        patch->is_avsync_start == false && aml_dev->bHDMIARCon == 0) {
        if (pcm_ioctl(pcm_handle, SNDRV_PCM_IOCTL_DELAY, &frames) >= 0) {
            alsa_out_i2s_ltcy = frames / SAMPLE_RATE_MS;

            /* start to skip the output frames to reduce alsa out latency */
            if (alsa_out_i2s_ltcy >= AVSYNC_ALSA_OUT_MAX_LATENCY) {
                patch->skip_frames = 1;
            }
        }
    }

    int alsa_out_spdif_ltcy = -1;
    /* for arc, check spdif device latency */
    if (pcm_handle_spdif && patch && patch->need_do_avsync == true &&
        patch->is_avsync_start == false && aml_dev->bHDMIARCon == 1) {
        if (pcm_ioctl(pcm_handle_spdif, SNDRV_PCM_IOCTL_DELAY, &frames) >= 0) {
            alsa_out_spdif_ltcy = calc_frame_to_latency(frames, aml_dev->sink_format);

            /* start to skip the output frames to reduce alsa out latency */
            if (alsa_out_spdif_ltcy >= AVSYNC_ALSA_OUT_MAX_LATENCY_ARC) {
                patch->skip_frames = 1;
            }
        }
    }
    if (patch->skip_frames > 0) {
        ALOGI("spdif latency:%d i2s latency:%d skip frame:%d",
                alsa_out_spdif_ltcy, alsa_out_i2s_ltcy, patch->skip_frames);
    }
}

int calc_frame_to_latency(int frames, audio_format_t format)
{
    int latency = frames / SAMPLE_RATE_MS;

    if (format == AUDIO_FORMAT_E_AC3) {
        latency /= EAC3_MULTIPLIER;
    } else if (format == AUDIO_FORMAT_MAT ||
            (format == AUDIO_FORMAT_DOLBY_TRUEHD)) {
        latency /= MAT_MULTIPLIER;
    }
    return latency;
}

static int calc_latency_to_frame(int latency, audio_format_t format)
{
    int frames = 0;
    int coefficient = 1;

    if (format == AUDIO_FORMAT_E_AC3) {
        coefficient = EAC3_MULTIPLIER;
    } else if (format == AUDIO_FORMAT_MAT || (format == AUDIO_FORMAT_DOLBY_TRUEHD)) {
        coefficient = MAT_MULTIPLIER;
    }

    frames = latency * SAMPLE_RATE_MS * coefficient;

    return frames;
}

static int ringbuffer_seek(struct aml_audio_patch *patch, int tune_val)
{
    int space = 0, seek_space = 0, frame_size = 0;

    frame_size = CHANNEL_CNT * audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
    space = calc_latency_to_frame(tune_val, patch->aformat) * frame_size;

    /* only reduce audio latency now */
    if (space < 0) {
        ALOGV("%s tune_val %d space %d\n", __func__, tune_val, space);
        space = 0;
    }

    seek_space = ring_buffer_seek(&patch->aml_ringbuffer, space);

    if (seek_space == space) {
        ALOGV("  --tunning audio ringbuffer %dms sucessfully!\n", tune_val);
    } else {
        ALOGV("  --tunning audio ringbuffer requre %d vs actual seek %d\n", space, seek_space);
        tune_val = calc_frame_to_latency(seek_space/frame_size, patch->aformat);
    }

    return tune_val;
}

int aml_dev_sample_audio_path_latency(struct aml_audio_device *aml_dev, char *latency_details)
{
    struct aml_stream_in *in = aml_dev->active_input;
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    int rbuf_ltcy = 0, spk_tuning_ltcy = 0, ms12_ltcy = 0, alsa_in_ltcy = 0;
    int alsa_out_i2s_ltcy = 0, alsa_out_spdif_ltcy = 0, alsa_output_latency = 0;;
    int whole_path_ltcy = 0, in_path_ltcy = 0, out_path_ltcy = 0;
    snd_pcm_sframes_t frames = 0;
    int frame_size = 0;
    int ret = 0;
    size_t rbuf_avail = 0;

    if (!patch) {
        return 0;
    }

    frame_size = CHANNEL_CNT * audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
    rbuf_avail = get_buffer_read_space(&patch->aml_ringbuffer);
    frames = rbuf_avail / frame_size;
    rbuf_ltcy = calc_frame_to_latency(frames, patch->aformat);
    patch->audio_latency.ringbuffer_latency = rbuf_ltcy;
    ALOGV("  audio ringbuf latency = %d", rbuf_ltcy);

    if (aml_dev->spk_tuning_lvl) {
        rbuf_avail = get_buffer_read_space(&aml_dev->spk_tuning_rbuf);
        frames = rbuf_avail / frame_size;
        spk_tuning_ltcy = frames / SAMPLE_RATE_MS;
        patch->audio_latency.user_tune_latency = spk_tuning_ltcy;
        ALOGV("  audio spk tuning latency = %d", spk_tuning_ltcy);
    } else {
        patch->audio_latency.user_tune_latency = 0;
    }

    if ((eDolbyMS12Lib == aml_dev->dolby_lib_type) &&
        aml_dev->ms12.dolby_ms12_enable &&
        aml_dev->ms12_out) {

        audio_format_t format = aml_dev->ms12_out->hal_internal_format;
        int ms12_latency_decoder = MS12_DECODER_LATENCY;
        int ms12_latency_pipeline = MS12_PIPELINE_LATENCY;
        int ms12_latency_dap = MS12_DAP_LATENCY;
        int ms12_latency_encoder = MS12_ENCODER_LATENCY;

        ms12_ltcy += ms12_latency_pipeline;

        if (!audio_is_linear_pcm(format)) {
            if ((format == AUDIO_FORMAT_AC3) || (format == AUDIO_FORMAT_E_AC3))
                ms12_ltcy += ms12_latency_decoder + MS12_DD_DDP_BUFERR_LATENCY;
            else if ((format == AUDIO_FORMAT_MAT) || (format == AUDIO_FORMAT_DOLBY_TRUEHD))
                ms12_ltcy += ms12_latency_decoder + MS12_MAT_BUFERR_LATENCY;
        }

        if (aml_dev->sink_format == AUDIO_FORMAT_PCM_16_BIT) {
            ms12_ltcy += ms12_latency_dap;
        } else if ((aml_dev->optical_format == AUDIO_FORMAT_AC3) ||
                   (aml_dev->optical_format == AUDIO_FORMAT_E_AC3)) {
            ms12_ltcy += ms12_latency_encoder;
        }

        /* if arc is connected and format setting is "Passthrough", bypass MS12 */
        if (aml_dev->bHDMIARCon == 1 && aml_dev->hdmi_format == BYPASS)
            ms12_ltcy = 0;

        patch->audio_latency.ms12_latency = ms12_ltcy;
        ALOGV("  audio ms12 latency = %d, format = %x", ms12_ltcy, format);
    } else {
        patch->audio_latency.ms12_latency = 0;
    }

    if (aml_dev->pcm_handle[I2S_DEVICE]) {
        ret = pcm_ioctl(aml_dev->pcm_handle[I2S_DEVICE], SNDRV_PCM_IOCTL_DELAY, &frames);
        if (ret >= 0) {
            alsa_out_i2s_ltcy = frames / SAMPLE_RATE_MS;
        }

        patch->audio_latency.alsa_i2s_out_latency = alsa_out_i2s_ltcy;
        ALOGV("  audio_hw_primary audio i2s latency = %d", alsa_out_i2s_ltcy);
    } else {
        patch->audio_latency.alsa_i2s_out_latency = 0;
    }

    if (aml_dev->alsa_handle[DIGITAL_DEVICE]) {
        aml_alsa_output_getinfo(aml_dev->alsa_handle[DIGITAL_DEVICE],
                                OUTPUT_INFO_DELAYFRAME,
                                (alsa_output_info_t *)&alsa_out_spdif_ltcy);
        patch->audio_latency.alsa_spdif_out_latency = alsa_out_spdif_ltcy;
        ALOGV("  audio spdif latency = %d", alsa_out_spdif_ltcy);
    } else {
        patch->audio_latency.alsa_spdif_out_latency = 0;
    }

    if (in && in->pcm) {
        ret = pcm_ioctl(in->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
        if (ret >= 0) {
            frames = frames%(in->config.period_size*in->config.period_count);
            alsa_in_ltcy = calc_frame_to_latency(frames, patch->aformat);
        }
        patch->audio_latency.alsa_in_latency = alsa_in_ltcy;
        ALOGV("  audio alsa in latency = %d", alsa_in_ltcy);
    } else {
        patch->audio_latency.alsa_in_latency = 0;
    }

    if (aml_dev->sink_format == AUDIO_FORMAT_PCM_16_BIT) {
        out_path_ltcy = alsa_out_i2s_ltcy + spk_tuning_ltcy;
        alsa_output_latency = alsa_out_i2s_ltcy;
        /* In AVR case, for PCM only AVR or UI setting to PCM */
        /* add more 10ms AVR latency */
        if (aml_dev->active_outport == OUTPORT_HDMI_ARC) {
            alsa_output_latency += AVR_LATENCY_PCM;
        }
    } else if (aml_dev->sink_format == AUDIO_FORMAT_AC3 ||
            aml_dev->sink_format == AUDIO_FORMAT_E_AC3 ||
            aml_dev->sink_format == AUDIO_FORMAT_MAT) {
        /* For raw data output of AVR, add more 60ms latency */
        out_path_ltcy = alsa_out_spdif_ltcy + AVR_LATENCY;
        alsa_output_latency = alsa_out_spdif_ltcy;
    }

    /* calc whole path latency considering with format */
    in_path_ltcy = alsa_in_ltcy + rbuf_ltcy + ms12_ltcy;
    whole_path_ltcy = in_path_ltcy + out_path_ltcy;

    if (latency_details) {
        sprintf(latency_details, "alsa in:%d rbuf:%d ms12:%d alsa out:%d "
                "speak tuning rbuf:%d",
                alsa_in_ltcy, rbuf_ltcy, ms12_ltcy, alsa_output_latency,
                spk_tuning_ltcy);
    }
    patch->audio_latency.total_latency = whole_path_ltcy;
    return whole_path_ltcy;
}

int aml_dev_sample_video_path_latency(struct aml_audio_patch *patch)
{
    int vltcy = 0;
    int max_vltcy = patch->max_video_latency;

    if (!max_vltcy) {
        max_vltcy = aml_sysfs_get_int("/sys/class/video/hdmin_delay_max_ms");
        patch->max_video_latency = max_vltcy;
    }

    vltcy = aml_sysfs_get_int("/sys/class/video/vframe_walk_delay");
    /*if (vltcy > max_vltcy || vltcy <= 0) {
        return -EINVAL;
    }*/

    return vltcy;
}

static inline int calc_diff(int altcy, int vltcy)
{
    return altcy - vltcy;
}

int aml_dev_avsync_diff_in_path(struct aml_audio_patch *patch, int *Vltcy, int *Altcy, char *latency_details)
{
    struct aml_audio_device *aml_dev;
    int altcy = 0, vltcy = 0;
    int src_diff_err = 0;
    int ret = 0;

    if (!patch || !Vltcy || !Altcy) {
        ret = -EINVAL;
        goto err;
    }
    aml_dev = (struct aml_audio_device *)patch->dev;

    /* if i2s output and input are not ready, skip avsync*/
    struct pcm *pcm_handle_i2s = aml_dev->pcm_handle[I2S_DEVICE];
    struct aml_stream_in *in = aml_dev->active_input;
    void *pcm_handle_spdif = aml_dev->alsa_handle[DIGITAL_DEVICE];

    if (!pcm_handle_i2s || !in->pcm) {
        ret = -EINVAL;
        goto err;
    }

    if (!pcm_handle_spdif && aml_dev->bHDMIARCon == 1 &&
        !audio_is_linear_pcm(aml_dev->optical_format)) {
        ret = -EINVAL;
        goto err;
    }

    check_skip_frames(aml_dev);

    vltcy = aml_dev_sample_video_path_latency(patch);
    if (vltcy < 0) {
        ret = -EINVAL;
        goto err;
    }
    *Vltcy = vltcy;

    if (patch->input_src == AUDIO_DEVICE_IN_LINE) {
        src_diff_err = -30;
    }
    altcy = aml_dev_sample_audio_path_latency(aml_dev, latency_details);
    *Altcy = altcy + src_diff_err;

    // FIXME: should be careful of overflow of latency_details
    int len = strlen(latency_details);
    sprintf(&latency_details[len], " video:%d", vltcy);
    ALOGV("  altcy: %dms, vltcy: %dms\n\n", altcy, vltcy);

    return 0;

err:
    return ret;
}

static inline void aml_dev_accumulate_avsync_diff(struct aml_audio_patch *patch, int vltcy, int altcy)
{
    patch->vltcy += vltcy;
    patch->altcy += altcy;
    patch->avsync_sample_accumed++;
    patch->average_vltcy = patch->vltcy / patch->avsync_sample_accumed;
    patch->average_altcy = patch->altcy / patch->avsync_sample_accumed;

    ALOGV("  latency status[%d]: average average_vltcy = %dms, average_altcy = %dms\n\n",
            patch->avsync_sample_accumed, patch->average_vltcy, patch->average_altcy);
}

static int aml_dev_tune_video_path_latency(unsigned int video_val)
{
    char tmp[32];
    int ret = 0;

    memset(tmp, 0, 32);
    snprintf(tmp, 32, "%d", video_val);
    ret = aml_sysfs_set_str("/sys/class/video/hdmin_delay_duration", tmp);
    if (ret < 0) {
        ALOGE("Error: %s() set [/sys/class/video/hdmin_delay_duration] fail,  err: %s",
            __func__, strerror(errno));
        goto err;
    }
    ret = aml_sysfs_set_str("/sys/class/video/hdmin_delay_start", "1");
    if (ret < 0) {
        ALOGE("Error: %s() set [/sys/class/video/hdmin_delay_start] fail,  err: %s",
            __func__, strerror(errno));
        goto err;
    }

    return 0;
err:
    return ret;
}

static inline void aml_dev_avsync_reset(struct aml_audio_patch *patch)
{
    patch->avsync_sample_accumed = 0;
    patch->vltcy = 0;
    patch->average_vltcy = 0;
    patch->need_do_avsync = false;
    patch->is_avsync_start = false;
    patch->skip_avsync_cnt = 0;
    patch->altcy = 0;
    patch->average_altcy = 0;
}

static void aml_check_pic_mode(struct aml_audio_patch *patch)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)patch->dev;;

    /* picture mode setting changed */
    if (patch->game_mode != aml_dev->game_mode) {
        ALOGD("%s(), pic mode changes from %d to %d",
            __func__, patch->game_mode, aml_dev->game_mode);
        aml_dev->mode_reconfig_in = true;
        aml_dev->mode_reconfig_out = true;

        patch->game_mode = aml_dev->game_mode;
    }
}

int aml_dev_try_avsync(struct aml_audio_patch *patch)
{
    int vltcy = 0, factor, altcy = 0;
    struct aml_audio_device *aml_dev;
    struct audio_stream_in *in;
    int ret = 0;

    if (!patch) {
        return 0;
    }
    aml_dev = (struct aml_audio_device *)patch->dev;
    in = (struct audio_stream_in *)aml_dev->active_input;
    factor = (patch->aformat == AUDIO_FORMAT_E_AC3) ? 2 : 1;

    aml_check_pic_mode(patch);

    char latency_details[256] = {0};
    ret = aml_dev_avsync_diff_in_path(patch, &vltcy, &altcy, latency_details);
    if (ret < 0) {
        return 0;
    }

    /* skip some frames to wait video stability*/
    if (patch->skip_avsync_cnt < AVSYNC_SKIP_CNT) {
        patch->skip_avsync_cnt++;
        return 0;
    }

    if (patch->is_avsync_start == false) {
        patch->is_avsync_start = true;
        /*flush the alsa input buffer*/
        aml_alsa_input_flush(in);
        ALOGV("  /* start calc the average latency of audio & video */");
    }

    aml_dev_accumulate_avsync_diff(patch, vltcy, altcy);

    int tune_val = patch->average_altcy;
    int avDiff = calc_diff(patch->average_altcy, patch->average_vltcy);
    int user_tune_val = aml_audio_get_src_tune_latency(aml_dev->patch_src);
    tune_val += user_tune_val;
    int seek_space_ret = 0;
    int seek_space = tune_val;
    int delay_video = 0;

    if (patch->avsync_sample_accumed >= AVSYNC_SAMPLE_MAX_CNT) {
        ALOGD("  --start avsync, user tuning latency = [%dms], total tuning latency = [%dms]",
                user_tune_val, tune_val);

        /* if it need reduce audio latency, first do ringbuffer seek*/
        if (patch->audio_latency.ringbuffer_latency > AVSYNC_RINGBUFFER_MIN_LATENCY) {
            seek_space = patch->audio_latency.ringbuffer_latency - AVSYNC_RINGBUFFER_MIN_LATENCY;

            seek_space_ret = ringbuffer_seek(patch, seek_space);
            tune_val -= seek_space_ret;
        }
        delay_video = tune_val;

        ret = aml_dev_tune_video_path_latency(tune_val);
        ALOGD("  --start avsync, tuning video total latency: value [%dms]", tune_val);

        //ALOGI("Adjust AV latency, avdiff:%d(%s) tuned offset:(%dms), do delay audio:%d(%d) delay video:(%d)",
        //        avDiff, latency_details, user_tune_val, -1 * seek_space_ret, -1 * seek_space, delay_video);
        aml_dev_avsync_reset(patch);
    }
    return 0;
}

