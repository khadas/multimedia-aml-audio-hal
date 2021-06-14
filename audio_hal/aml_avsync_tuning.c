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

#include <string.h>
#include <time.h>
#include <cutils/log.h>
#include <linux/ioctl.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include <aml_android_utils.h>
#include <pthread.h>

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

    if (continous_mode(aml_dev)) {
        return;
    }

    if (pcm_handle && patch && patch->need_do_avsync == true && patch->is_avsync_start == false) {
        if (pcm_ioctl(pcm_handle, SNDRV_PCM_IOCTL_DELAY, &frames) >= 0) {
            int alsa_out_i2s_ltcy = frames / SAMPLE_RATE_MS;

            /* start to skip the output frames to reduce alsa out latency */
            if (alsa_out_i2s_ltcy >= AVSYNC_ALSA_OUT_MAX_LATENCY) {
                patch->skip_frames = 1;
            } else {
                patch->skip_frames = 0;
            }
        }
    }
}

static int calc_frame_to_latency(int frames, audio_format_t format)
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
#if 0
    if (format == AUDIO_FORMAT_E_AC3) {
        frames *= EAC3_MULTIPLIER;
    } else if (format == AUDIO_FORMAT_MAT ||
            (format == AUDIO_FORMAT_DOLBY_TRUEHD)) {
        frames *= MAT_MULTIPLIER;
    }
#endif
    frames = latency * SAMPLE_RATE_MS;
    ALOGV("%s(), %d", __func__, format);
    return frames;
}

static int ringbuffer_seek(struct aml_audio_patch *patch, int tune_val)
{
    int space = 0, seek_space = 0;

    space = calc_latency_to_frame(tune_val, patch->aformat);
    space *= CHANNEL_CNT * audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
    seek_space = ring_buffer_seek(&patch->aml_ringbuffer, space);

    if (seek_space == space)
        ALOGV("  --tunning audio ringbuffer %dms sucessfully!", tune_val);
    else
        ALOGV("  not enough space to seek!");

    return seek_space;
}

int aml_dev_sample_audio_path_latency(struct aml_audio_device *aml_dev)
{
    struct aml_stream_in *in = aml_dev->active_input;
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    int rbuf_ltcy = 0, spk_tuning_ltcy = 0, ms12_ltcy = 0, alsa_in_ltcy = 0;
    int alsa_out_i2s_ltcy = 0, alsa_out_spdif_ltcy = 0;
    int whole_path_ltcy = 0, in_path_ltcy = 0, out_path_ltcy = 0;
    snd_pcm_sframes_t frames = 0;
    int frame_size = 0;
    int ret = 0;

    frame_size = CHANNEL_CNT * audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
    if (patch) {
        size_t rbuf_avail = 0;

        rbuf_avail = get_buffer_read_space(&patch->aml_ringbuffer);
        frames = rbuf_avail / frame_size;
        rbuf_ltcy = calc_frame_to_latency(frames, patch->aformat);
        patch->rbuf_ltcy = rbuf_ltcy;

        ALOGI("  audio ringbuf latency = %d", rbuf_ltcy);
    }

    if (aml_dev->spk_tuning_lvl) {
        size_t rbuf_avail = 0;

        rbuf_avail = get_buffer_read_space(&aml_dev->spk_tuning_rbuf);
        frames = rbuf_avail / frame_size;
        spk_tuning_ltcy = frames / SAMPLE_RATE_MS;

        ALOGI("  audio spk tuning latency = %d", spk_tuning_ltcy);
    }

    if ((eDolbyMS12Lib == aml_dev->dolby_lib_type) &&
        aml_dev->ms12.dolby_ms12_enable &&
        aml_dev->ms12_out) {

        audio_format_t format = aml_dev->ms12_out->hal_internal_format;
        int dolby_main_avail = dolby_ms12_get_main_buffer_avail(NULL);
        int ms12_latency_decoder = MS12_DECODER_LATENCY;
        int ms12_latency_pipeline = MS12_PIPELINE_LATENCY;
        int ms12_latency_dap = MS12_DAP_LATENCY;
        int ms12_latency_encoder = MS12_ENCODER_LATENCY;

        if (dolby_main_avail > 0) {
            ms12_ltcy = calc_frame_to_latency((dolby_main_avail / frame_size), format);
            ALOGI("  audio ms12 buffer latency = %d, format = %x", ms12_ltcy, format);
        }

        ms12_ltcy += ms12_latency_pipeline;

        if ((format == AUDIO_FORMAT_AC3) ||
            (format == AUDIO_FORMAT_E_AC3) ||
            (format == AUDIO_FORMAT_AC4))
            ms12_ltcy += ms12_latency_decoder;
        if (aml_dev->sink_format == AUDIO_FORMAT_PCM_16_BIT) {
            ms12_ltcy += ms12_latency_dap;
        } else if ((aml_dev->optical_format == AUDIO_FORMAT_AC3) ||
                   (aml_dev->optical_format == AUDIO_FORMAT_E_AC3)) {
            ms12_ltcy += ms12_latency_encoder;
        }

        ALOGI("  audio ms12 latency = %d, format = %x", ms12_ltcy, format);
    }

    if (aml_dev->pcm_handle[I2S_DEVICE]) {
        ret = pcm_ioctl(aml_dev->pcm_handle[I2S_DEVICE], SNDRV_PCM_IOCTL_DELAY, &frames);
        if (ret >= 0) {
            alsa_out_i2s_ltcy = frames / SAMPLE_RATE_MS;
        }

        ALOGI("  audio_hw_primary audio i2s latency = %d", alsa_out_i2s_ltcy);
    }

    if (aml_dev->pcm_handle[DIGITAL_DEVICE]) {
        ret = pcm_ioctl(aml_dev->pcm_handle[DIGITAL_DEVICE], SNDRV_PCM_IOCTL_DELAY, &frames);
        if (ret >= 0) {
            alsa_out_spdif_ltcy = calc_frame_to_latency(frames, aml_dev->sink_format);
        }
        ALOGI("  audio spdif latency = %d", alsa_out_spdif_ltcy);
    }

    if (in) {
        if (in->pcm != NULL) {
            ret = pcm_ioctl(in->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
            if (ret >= 0) {
                frames = frames%(in->config.period_size*in->config.period_count);
                if (patch) {
                    alsa_in_ltcy = calc_frame_to_latency(frames, patch->aformat);
                }
            }
            ALOGI("  audio alsa in latency = %d", alsa_in_ltcy);
        }
    }

    /* calc whole path latency considering with format */
    if (patch) {
        int in_path_ltcy = alsa_in_ltcy + rbuf_ltcy + ms12_ltcy;
        int out_path_ltcy = 0;

        if (aml_dev->sink_format == AUDIO_FORMAT_PCM_16_BIT) {
            out_path_ltcy = alsa_out_i2s_ltcy + spk_tuning_ltcy;
        } else if (aml_dev->sink_format == AUDIO_FORMAT_AC3 ||
                   aml_dev->sink_format == AUDIO_FORMAT_E_AC3 ||
                   aml_dev->sink_format == AUDIO_FORMAT_MAT) {
            out_path_ltcy = alsa_out_spdif_ltcy;
        }

        whole_path_ltcy = in_path_ltcy + out_path_ltcy;
    }

    ALOGI("whole_path_ltcy = %d", whole_path_ltcy);

    return whole_path_ltcy;
}

int aml_dev_sample_video_path_latency(void)
{
    int vltcy = 0;

    vltcy = aml_sysfs_get_int("/sys/class/video/vframe_walk_delay");
    if (vltcy >= MAX_VEDIO_LATENCY || vltcy <= 0) {
        return -EINVAL;
    }

    return vltcy;
}

static inline int calc_diff(int altcy, int vltcy)
{
    return altcy - vltcy;
}

static int aml_dev_avsync_diff_in_path(struct aml_audio_patch *patch, int *av_diff)
{
    struct aml_audio_device *aml_dev;
    int altcy = 0, vltcy = 0;
    int src_diff_err = 0;
    int ret = 0;

    if (!patch || !av_diff) {
        ret = -EINVAL;
        goto err;
    }
    aml_dev = (struct aml_audio_device *)patch->dev;

    check_skip_frames(aml_dev);

    vltcy = aml_dev_sample_video_path_latency();
    if (vltcy < 0) {
        ret = -EINVAL;
        goto err;
    }

    altcy = aml_dev_sample_audio_path_latency(aml_dev);

    /*if (patch->input_src == AUDIO_DEVICE_IN_LINE) {
        src_diff_err = -30;
    }*/
    *av_diff = altcy - vltcy + src_diff_err;

    ALOGV("  altcy: %dms, vltcy: %dms, av_diff: %dms\n\n", altcy, vltcy, *av_diff);

    return 0;

err:
    return ret;
}

static inline void aml_dev_accumulate_avsync_diff(struct aml_audio_patch *patch, int av_diff)
{
    patch->av_diffs += av_diff;
    patch->avsync_sample_accumed++;
    patch->average_av_diffs = patch->av_diffs / patch->avsync_sample_accumed;

    ALOGV("  latency status[%d]: average av_diff = %dms\n\n",
            patch->avsync_sample_accumed, patch->average_av_diffs);
}

int aml_dev_tune_video_path_latency(int tune_val)
{
    char tmp[32];
    int ret = 0;

    if (tune_val > 200 || tune_val < -200) {
        ALOGE("%s():  unsupport tuning value: %dms", __func__, tune_val);
        ret = -EINVAL;
        goto err;
    }

    if ((tune_val < 16 && tune_val >= 0) || (tune_val <= 0 && tune_val > -16)) {
        return 0;
    }

    memset(tmp, 0, 32);
    snprintf(tmp, 32, "%d", tune_val);
    ret = aml_sysfs_set_str("/sys/class/video/hdmin_delay_duration", tmp);
    if (ret < 0) {
        ALOGE("%s() fail,  err: %s", __func__, strerror(errno));
        goto err;
    }
    ret = aml_sysfs_set_str("/sys/class/video/hdmin_delay_start", "1");
    if (ret < 0) {
        ALOGE("%s() fail,  err: %s", __func__, strerror(errno));
        goto err;
    }

    ALOGD("  --tuning video total latency: value %dms", tune_val);
    return 0;
err:
    return ret;
}

static inline void aml_dev_avsync_reset(struct aml_audio_patch *patch)
{
    patch->avsync_sample_accumed = 0;
    patch->av_diffs = 0;
    patch->average_av_diffs = 0;
    patch->need_do_avsync = false;
    patch->is_avsync_start = false;
    patch->skip_avsync_cnt = 0;
}

void aml_dev_patch_lower_output_latency(struct aml_audio_device *aml_dev)
{
    /* For streaming from mixer, the ALSA latency is set to big to give
     * more margin to application to send audio data. When switching from
     * streaming to patch (e.g. HDMI IN), need decrease pipeline latency for
     * non PCM/MAT input.
     * Under continuous output mode, drain output buffer level to a certain
     * level so thw whole pipeline is running with a consistant low level
     * when patch starts.
     */
    while (1) {
        snd_pcm_sframes_t frames = 0;
        struct pcm *pcm_handle;
        int r;

        pthread_mutex_lock(&aml_dev->patch_lock);
        /* for PCM/MAT input, assuming MS12 has less internal latency so allow
         * more latency on output buffer
         */
        if ((!aml_dev->audio_patch) ||
            !((aml_dev->patch_src == SRC_HDMIIN) || (aml_dev->patch_src == SRC_SPDIFIN) || (aml_dev->patch_src == SRC_LINEIN)) ||
            (aml_dev->audio_patch->aformat == AUDIO_FORMAT_MAT) ||
            (aml_dev->audio_patch->aformat == AUDIO_FORMAT_DOLBY_TRUEHD) ||
            audio_is_linear_pcm(aml_dev->audio_patch->aformat)) {
            pthread_mutex_unlock(&aml_dev->patch_lock);
            break;
        }
        pthread_mutex_unlock(&aml_dev->patch_lock);

        pthread_mutex_lock(&aml_dev->alsa_pcm_lock);
        pcm_handle = aml_dev->pcm_handle[I2S_DEVICE];
        if ((!pcm_handle) || (pcm_state(pcm_handle) != SNDRV_PCM_STATE_RUNNING)) {
            pthread_mutex_unlock(&aml_dev->alsa_pcm_lock);
            break;
        }

        r = pcm_ioctl(pcm_handle, SNDRV_PCM_IOCTL_DELAY, &frames);
        pthread_mutex_unlock(&aml_dev->alsa_pcm_lock);

        if (r >= 0) {
            int alsa_out_i2s_ltcy = frames / SAMPLE_RATE_MS;

            /* start to skip the output frames to reduce alsa out latency */
            if (alsa_out_i2s_ltcy < AVSYNC_ALSA_OUT_MAX_LATENCY) {
                break;
            }
        } else {
            break;
        }
    }
}

int aml_dev_try_avsync(struct aml_audio_patch *patch)
{
    int av_diff = 0, factor;
    struct aml_audio_device *aml_dev;
    struct audio_stream_in *in;
    int ret = 0;

    if (!patch) {
        return 0;
    }
    aml_dev = (struct aml_audio_device *)patch->dev;
    in = (struct audio_stream_in *)aml_dev->active_input;
    factor = (patch->aformat == AUDIO_FORMAT_E_AC3) ? 2 : 1;

    ret = aml_dev_avsync_diff_in_path(patch, &av_diff);
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

    aml_dev_accumulate_avsync_diff(patch, av_diff);

    if (patch->avsync_sample_accumed >= AVSYNC_SAMPLE_MAX_CNT) {
        int tune_val = patch->average_av_diffs;
        int user_tune_val = aml_audio_get_tvsrc_tune_latency(aml_dev->patch_src);

        tune_val += user_tune_val;

        ALOGD("  --start avsync, user tuning latency = %dms, total tuning latency = %dms",
                user_tune_val, tune_val);

        if (tune_val > 0) {
            /* if it need reduce audio latency, first do ringbuffer seek*/
            if (patch->rbuf_ltcy > AVSYNC_RINGBUFFER_MIN_LATENCY) {
                int seek_space = patch->rbuf_ltcy - AVSYNC_RINGBUFFER_MIN_LATENCY;

                if (seek_space > tune_val)
                    seek_space = tune_val;

                ringbuffer_seek(patch, seek_space);
                tune_val -= seek_space;
            }
            /* then delay video*/
            if (tune_val > 0) {
                ret = aml_dev_tune_video_path_latency(tune_val);
            } else {
                ret = aml_dev_tune_video_path_latency(0);
            }
        } else {
            /* if it need add more audio latency, direct do ringbuffer seek*/
            ringbuffer_seek(patch, tune_val);
        }

        aml_dev_avsync_reset(patch);
    }
    return 0;
}

