/*
* Copyright 2023 Amlogic Inc. All rights reserved.
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

#define LOG_TAG "audio_hw_input_tv"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <utils/Timers.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <hardware/audio.h>
#include <aml_data_utils.h>
#include <audio_utils/channels.h>
#if ANDROID_PLATFORM_SDK_VERSION >= 25 // 8.0
#include <system/audio-base.h>
#endif

#include "audio_hw.h"
#include "aml_audio_stream.h"
#include "audio_hw_utils.h"
#include "aml_audio_timer.h"
#include "alsa_config_parameters.h"
#include "tv_patch_avsync.h"
#include "aml_ng.h"
#include "alsa_device_parser.h"
#include "tv_patch_ctrl.h"
#include "tv_patch.h"
#include "dolby_lib_api.h"
#include "spdif_encoder_api.h"
#include "audio_port.h"
#include "amlAudioMixer.h"
#include "audio_hw_ms12_common.h"
#include "aml_audio_ms12_bypass.h"
#include "aml_audio_dev2mix_process.h"


static int get_audio_patch_by_src_dev(struct audio_hw_device *dev, audio_devices_t dev_type, struct audio_patch **p_audio_patch)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    struct listnode *node = NULL;
    struct audio_patch_set *patch_set_tmp = NULL;
    struct audio_patch *patch_tmp = NULL;

    list_for_each(node, &aml_dev->patch_list) {
        patch_set_tmp = node_to_item(node, struct audio_patch_set, list);
        patch_tmp = &patch_set_tmp->audio_patch;
        if (patch_tmp->sources[0].ext.device.type == dev_type) {
            ALOGI("%s, patch_tmp->id = %d, dev_type = %ud", __func__, patch_tmp->id, dev_type);
            *p_audio_patch = patch_tmp;
            break;
        }
    }
    return 0;
}

void audio_digital_input_format_check(struct aml_audio_patch *patch)
{
    audio_format_t cur_aformat;
    if (IS_DIGITAL_IN_HW(patch->input_src)) {
        cur_aformat = audio_parse_get_audio_type (patch->audio_parse_para);
        if (cur_aformat != patch->aformat) {
            ALOGI ("HDMI/SPDIF input format changed from %#x to %#x\n", patch->aformat, cur_aformat);
            patch->aformat = cur_aformat;
            patch->digital_input_fmt_change = true;
            //patch->mode_reconfig_flag = true;
        }
    }
}

void *audio_patch_signal_detect_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    bool cur_hw_stable = false;
    bool last_hw_stable = false;
    int cur_type = 0;
    int lat_type = 0;
    int cur_sr = -1;
    int last_sr = -1;
    int cur_channel = -1;
    int last_channel = -1;
    audio_format_t cur_aformat = AUDIO_FORMAT_INVALID;
    audio_format_t last_aformat = AUDIO_FORMAT_INVALID;
    bool cur_raw_data_change = false;
    bool last_raw_data_change = false;
    struct timespec mute_start_ts;
    int mute_mdelay = 120;
    bool mute_flag = false;
    bool in_mute = false;


    ALOGD("%s: in", __func__);
    prctl(PR_SET_NAME, (unsigned long)"audio_signal_detect");
    aml_set_thread_priority("audio_signal_detect", patch->audio_signal_detect_threadID);

    while (!patch->signal_detect_thread_exit) {
        cur_hw_stable = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_HDMI_IN_AUDIO_STABLE);
        cur_type = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_HDMIIN_AUDIO_TYPE);
        cur_sr = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_HDMI_IN_SAMPLERATE);
        cur_channel = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_HDMI_IN_CHANNELS);
        cur_aformat = audio_parse_get_audio_type (patch->audio_parse_para);
        cur_raw_data_change = patch->start_mute;

        if ((!cur_hw_stable && cur_hw_stable != last_hw_stable) || (cur_type != lat_type)
             || (cur_sr != last_sr) || (cur_channel != last_channel)
             || (cur_raw_data_change && cur_raw_data_change  != last_raw_data_change)
             || ((cur_aformat != last_aformat) && (cur_aformat == AUDIO_FORMAT_DTS || cur_aformat == AUDIO_FORMAT_DTS_HD) )) {// resolve dts resume pop noise
            ALOGI("[%s:%d] hw_stable(%d)(%d) sr(%d)(%d) channel(%d)(%d) aformat(%d)(%d) raw_data_change(%d)(%d) type(%d)(%d)", __FUNCTION__, __LINE__,
                cur_hw_stable, last_hw_stable, cur_sr, last_sr, cur_channel, last_channel, cur_aformat, last_aformat, cur_raw_data_change,
                last_raw_data_change, cur_type, lat_type);
            audiohal_send_msg_2_ms12(&aml_dev->ms12, MS12_MESG_TYPE_FLUSH);

            set_aed_master_volume_mute(&aml_dev->alsa_mixer, true);
            //set_dac_digital_volume_mute(&aml_dev->alsa_mixer, true);//TBD
            clock_gettime(CLOCK_MONOTONIC, &mute_start_ts);
            mute_flag = true;
            ALOGD("%s: mute", __func__); /*Complete mute*/
        }

        if (mute_flag) {
            in_mute = Stop_watch(mute_start_ts, mute_mdelay);
            if (!in_mute) {
                set_aed_master_volume_mute(&aml_dev->alsa_mixer, false);
                //set_dac_digital_volume_mute(&aml_dev->alsa_mixer, false);//TBD
                ALOGD("%s: unmute", __func__);
                mute_flag = false;
            }
        }

        lat_type = cur_type;
        last_hw_stable = cur_hw_stable;
        last_sr = cur_sr;
        last_channel = cur_channel;
        last_aformat = cur_aformat;
        last_raw_data_change = cur_raw_data_change;
        aml_audio_sleep(5*1000);
    }

    set_aed_master_volume_mute(&aml_dev->alsa_mixer, false);
    //set_dac_digital_volume_mute(&aml_dev->alsa_mixer, false);//TBD
    ALOGD("%s: exit and unmute", __func__);
}


// buffer/period ratio, bigger will add more latency
void *audio_patch_input_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    ring_buffer_t *ringbuffer = & (patch->aml_ringbuffer);
    struct audio_stream_in *stream_in = NULL;
    struct aml_stream_in *in;
    struct audio_config stream_config;
    struct timespec ts;
    int aux_read_bytes, read_bytes;
    // FIXME: add calc for read_bytes;
    read_bytes = DEFAULT_CAPTURE_PERIOD_SIZE * CAPTURE_PERIOD_COUNT;
    int ret = 0, retry = 0;
    audio_format_t cur_aformat;
    int ring_buffer_size = 0;
    bool stable_flag = false;

    ALOGI("++%s", __FUNCTION__);

    ALOGD("%s: enter", __func__);
    patch->chanmask = stream_config.channel_mask = patch->in_chanmask;
    stream_config.sample_rate = patch->in_sample_rate;
    patch->aformat = stream_config.format = patch->in_format;

    ret = aml_dev->hw_device.open_input_stream(patch->dev, 0, patch->input_src, &stream_config, &stream_in, 0, NULL, 0);
    if (ret < 0) {
        ALOGE("%s: open input steam failed ret = %d", __func__, ret);
        return (void *)0;
    }

    in = (struct aml_stream_in *)stream_in;
    /* CVBS IN signal is stable very quickly, sometimes there is no unstable state,
     * we need to do ease in before playing CVBS IN at in_read func.
     */
    if (in->device & AUDIO_DEVICE_IN_LINE) {
        in->mute_flag = true;
    }

    patch->in_buf_size = read_bytes = in->config.period_size * audio_stream_in_frame_size(&in->stream);
    patch->in_buf = aml_audio_calloc(1, patch->in_buf_size);
    if (!patch->in_buf) {
        aml_dev->hw_device.close_input_stream(patch->dev, &in->stream);
        return (void *)0;
    }

    int first_start = 1;
    prctl(PR_SET_NAME, (unsigned long)"audio_input_patch");
    aml_set_thread_priority("audio_input_patch", patch->audio_input_threadID);
    /*affinity the thread to cpu 2/3 which has few IRQ*/
    aml_audio_set_cpu23_affinity();

    if (ringbuffer) {
        ring_buffer_size = ringbuffer->size;
    }

    while (!patch->input_thread_exit) {
        int bytes_avail = 0;
        /* Todo: read bytes should reconfig with period size */
        int period_mul = 1;//convert_audio_format_2_period_mul(patch->aformat);
        int read_threshold = 0;
        if (!patch->game_mode)
            read_bytes = DEFAULT_CAPTURE_PERIOD_SIZE * audio_stream_in_frame_size(&in->stream) * period_mul;
        else
            read_bytes = LOW_LATENCY_CAPTURE_PERIOD_SIZE * audio_stream_in_frame_size(&in->stream) * period_mul;
        bool hdmi_raw_in_flag = patch && (patch->input_src == AUDIO_DEVICE_IN_HDMI) && (!audio_is_linear_pcm(patch->aformat));
        if (hdmi_raw_in_flag) {
            read_bytes = read_bytes / 2;
            if (patch->aformat == AUDIO_FORMAT_MAT) {
                read_bytes = read_bytes * 4;
            }
        }

        if (patch->input_src == AUDIO_DEVICE_IN_LINE) {
            read_threshold = 4 * read_bytes;
        }

        // buffer size diff from allocation size, need to resize.
        if (patch->in_buf_size < (size_t)read_bytes) {
            ALOGI("%s: !!realloc in buf size from %zu to %d", __func__, patch->in_buf_size, read_bytes);
            patch->in_buf = aml_audio_realloc(patch->in_buf, read_bytes);
            if (!patch->in_buf) {
               break;
            }
            patch->in_buf_size = read_bytes;
            memset(patch->in_buf, 0, patch->in_buf_size);
        }

        if (in->standby) {
            ret = start_input_stream(in);
            if (ret < 0) {
                ALOGE("start_input_stream failed !");
            }
            in->standby = 0;
        }

        bytes_avail = read_bytes;
        /* if audio is unstable, don't read data from hardware */
        stable_flag = check_tv_stream_signal(&in->stream);
        if (aml_dev->tv_mute || !stable_flag) {
            memset(patch->in_buf, 0, bytes_avail);
            ring_buffer_clear(ringbuffer);
            usleep(20*1000);
        } else {
            if (aml_dev->patch_src == SRC_HDMIIN && in->audio_packet_type == AUDIO_PACKET_AUDS && in->config.channels != 2) {
                input_stream_channels_adjust(&in->stream, patch->in_buf, read_bytes);
            } else {
                //aml_audio_trace_int("input_read_thread", read_bytes);
                aml_alsa_input_read(&in->stream, patch->in_buf, read_bytes);
                //aml_audio_trace_int("input_read_thread", 0);
                if (IS_DIGITAL_IN_HW(patch->input_src) && !check_digital_in_stream_signal(&in->stream)) {
                    memset(patch->in_buf, 0, bytes_avail);
                }
            }

            if (IS_DIGITAL_IN_HW(patch->input_src)) {
                cur_aformat = audio_parse_get_audio_type (patch->audio_parse_para);
                if (cur_aformat != AUDIO_FORMAT_PCM_16_BIT) {
                    audio_raw_data_continuous_check(aml_dev, patch->audio_parse_para, patch->in_buf, read_bytes);
                }
            }
            if (patch->start_mute) {
                int flag = Stop_watch(patch->start_ts, patch->mdelay);
                if (!flag) {
                    patch->start_mute = false;
                } else {
                    memset(patch->in_buf, 0, read_bytes);
                }
            }

            if (aml_audio_property_get_bool("vendor.media.audiohal.indump", false)) {
                aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/alsa_in_read.raw",
                    patch->in_buf, read_bytes);
            }
        }
#ifdef USE_EQ_DRC
        /*noise gate is only used in Linein for 16bit audio data*/
        if (aml_dev->active_inport == INPORT_LINEIN && aml_dev->aml_ng_enable == 1) {
            int ng_status = noise_evaluation(aml_dev->aml_ng_handle, patch->in_buf, bytes_avail >> 1);
            /*if (ng_status == NG_MUTE)
                ALOGI("noise gate is working!");*/
        }
#endif
        ALOGV("++%s in read over read_bytes = %d, in_read returns = %d, threshold %d avail data=%d",
              __FUNCTION__, read_bytes, bytes_avail, read_threshold, get_buffer_read_space(ringbuffer));

        if (bytes_avail > 0) {
            //DoDumpData(patch->in_buf, bytes_avail, CC_DUMP_SRC_TYPE_INPUT);
            do {
                if (patch->input_src == AUDIO_DEVICE_IN_HDMI)
                {
                    pthread_mutex_lock(&in->lock);
                    ret = reconfig_read_param_through_hdmiin(aml_dev, in, ringbuffer, ring_buffer_size);
                    pthread_mutex_unlock(&in->lock);
                    if (ret == 0) {
                        break;
                    }
                }

                if (get_buffer_write_space(ringbuffer) >= bytes_avail) {
                    retry = 0;
                    aml_audio_trace_int("input_thread_write2buf", bytes_avail);
                    ret = ring_buffer_write(ringbuffer,
                                            (unsigned char*)patch->in_buf,
                                            bytes_avail, UNCOVER_WRITE);
                    if (ret != bytes_avail) {
                        ALOGE("%s(), write buffer fails!", __func__);
                    }
                    aml_audio_trace_int("input_thread_write2buf", 0);

                    if (!first_start || get_buffer_read_space(ringbuffer) >= read_threshold) {
                        pthread_cond_signal(&patch->cond);
                        if (first_start) {
                            first_start = 0;
                        }
                    }
                    //usleep(1000);
                } else {
                    retry = 1;
                    pthread_cond_signal(&patch->cond);
                    //Fixme: if ringbuffer is full enough but no output, reset ringbuffer
                    ALOGD("%s(), ring buffer no space to write, buffer free size:%d, need write size:%d", __func__,
                        get_buffer_write_space(ringbuffer), bytes_avail);
                    ring_buffer_reset(ringbuffer);
                    usleep(3000);
                }
            } while (retry && !patch->input_thread_exit);
        } else {
            ALOGV("%s(), read alsa pcm fails, to _read(%d), bytes_avail(%d)!",
                  __func__, read_bytes, bytes_avail);
            if (get_buffer_read_space(ringbuffer) >= bytes_avail) {
                pthread_cond_signal(&patch->cond);
            }
            usleep(3000);
        }
    }
    aml_dev->hw_device.close_input_stream(patch->dev, &in->stream);
    if (patch->in_buf) {
        aml_audio_free(patch->in_buf);
        patch->in_buf = NULL;
    }
    ALOGD("%s: exit", __func__);

    return (void *)0;
}

void *audio_patch_output_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    struct dolby_ms12_desc *ms12 = &(aml_dev->ms12);
    ring_buffer_t *ringbuffer = & (patch->aml_ringbuffer);
    struct audio_stream_out *stream_out = NULL;
    struct aml_stream_out *aml_out = NULL,*out;
    struct audio_config stream_config;
    struct timespec ts;
    int write_bytes = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    int original_write_bytes = write_bytes;
    int ret;
    bool find_iec_sync_word = false;
    AM_LOGD("enter");
    stream_config.channel_mask = patch->out_chanmask;
    stream_config.sample_rate = patch->out_sample_rate;
    stream_config.format = patch->out_format;

#ifdef DOLBY_MS12_INPUT_FORMAT_TEST
    int format = 0;
    format = aml_audio_property_get_int("vendor.dolby.ms12.input.format", format);
    if (format == 1) {
        stream_config.format = AUDIO_FORMAT_AC3;
    } else if (format == 2) {
        stream_config.format = AUDIO_FORMAT_E_AC3;
    }
#endif
    /*
    may we just exit from a direct active stream playback
    still here.we need remove to standby to new playback
    */
    pthread_mutex_lock(&aml_dev->lock);
    aml_out = direct_active(aml_dev);
    if (aml_out) {
        AM_LOGI("stream %p active,need standby aml_out->usecase:%s ", aml_out, usecase2Str(aml_out->usecase));
        pthread_mutex_lock(&aml_out->lock);
        do_output_standby_l((struct audio_stream *)aml_out);
        pthread_mutex_unlock(&aml_out->lock);
    }
    aml_dev->mix_init_flag = false;
    aml_dev->mute_start = true;
    pthread_mutex_unlock(&aml_dev->lock);
    ret = aml_dev->hw_device.open_output_stream(patch->dev,
                                      0,
                                      patch->output_src,
                                      AUDIO_OUTPUT_FLAG_DIRECT,
                                      &stream_config,
                                      &stream_out,
                                      "AML_TV_SOURCE");
    if (ret < 0) {
        AM_LOGE("open output stream failed");
        return (void *)0;
    }

    out = (struct aml_stream_out *)stream_out;
    patch->out_buf_size = write_bytes = original_write_bytes = out->config.period_size * audio_stream_out_frame_size(&out->stream);
    patch->out_buf = aml_audio_calloc(1, patch->out_buf_size);
    if (!patch->out_buf) {
        aml_dev->hw_device.close_output_stream(patch->dev, &out->stream);
        AM_LOGE("patch->out_buf calloc fail");
        return (void *)0;
    }
    prctl(PR_SET_NAME, (unsigned long)"audio_output_patch");
    aml_set_thread_priority("audio_output_patch", patch->audio_output_threadID);
    /*affinity the thread to cpu 2/3 which has few IRQ*/
    aml_audio_set_cpu23_affinity();

    while (!patch->output_thread_exit) {
        int period_mul;
        //! check hdmi input format
        audio_digital_input_format_check(patch);
        if (IS_DIGITAL_IN_HW(patch->input_src) && patch->digital_input_fmt_change) {
            AM_LOGI("HDMI format change from %x to %x", out->hal_internal_format, patch->aformat);

            //! close old stream
            if (out != NULL) {
                aml_dev->hw_device.close_output_stream(patch->dev, &out->stream);
                stream_out = NULL;
                out = NULL;
            }
            //! open new stream
            stream_config.channel_mask = audio_parse_get_audio_channel_mask (patch->audio_parse_para);
            stream_config.format = patch->aformat;
            if (patch->aformat == AUDIO_FORMAT_PCM_16_BIT) {//Fix Me:
                stream_config.sample_rate = 48000;
            } else {
                stream_config.sample_rate = audio_parse_get_audio_samplerate(patch->audio_parse_para);
            }

            ret = aml_dev->hw_device.open_output_stream(patch->dev,
                                              0,
                                              patch->output_src,
                                              AUDIO_OUTPUT_FLAG_DIRECT,
                                              &stream_config,
                                              &stream_out,
                                              "AML_TV_SOURCE");
            if (ret < 0) {
                AM_LOGE("open output stream failed");
                stream_out = NULL;
                out = NULL;
                continue;
            }
            //! set stream format and hal rate
            out = (struct aml_stream_out *)stream_out;
            if (out->hal_internal_format != AUDIO_FORMAT_PCM_16_BIT &&
                out->hal_internal_format != AUDIO_FORMAT_PCM_32_BIT) {
                out->hal_format = AUDIO_FORMAT_IEC61937;
                find_iec_sync_word = false;
                //Check DTS-CD
                if (out->hal_internal_format == AUDIO_FORMAT_DTS ||
                    out->hal_internal_format == AUDIO_FORMAT_DTS_HD) {
                    if (audio_parse_get_audio_type_direct(patch->audio_parse_para) == DTSCD ) {
                        out->is_dtscd = true;
                    } else {
                        out->is_dtscd = false;
                    }
                }
                //Reset dd and ddp sample rate
                if (out->hal_internal_format == AUDIO_FORMAT_AC3 ||
                    out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
                    if (out->hal_rate == 96000 || out->hal_rate == 192000) {
                        out->hal_rate = 48000;
                    } else if (out->hal_rate == 88200 || out->hal_rate == 176400) {
                        out->hal_rate = 44100;
                    }
                }
            }

            /* reset audio patch ringbuffer */
            ring_buffer_reset(&patch->aml_ringbuffer);
            if ((aml_dev->hdmi_format == BYPASS) && (ms12->ms12_bypass_handle)) {
                aml_ms12_bypass_reset(ms12->ms12_bypass_handle);
            }
#ifdef ADD_AUDIO_DELAY_INTERFACE
            // fixed switch between RAW and PCM noise, drop delay residual data
            aml_audio_delay_clear(AML_DELAY_OUTPORT_SPDIF);
            aml_audio_delay_clear(AML_DELAY_OUTPORT_SPDIF_RAW);
            aml_audio_delay_clear(AML_DELAY_OUTPORT_SPDIF_B_RAW);
#endif
            /* we need standby a2dp when switch the format, in order to prevent UNDERFLOW in a2dp stack. */
            if (aml_dev->active_outport == OUTPORT_A2DP) {
                aml_dev->need_reset_a2dp = true;
            }

            patch->digital_input_fmt_change = false;
        }

        period_mul = 1;
        write_bytes = original_write_bytes;
        if (out->hal_internal_format == AUDIO_FORMAT_AC3) {
            write_bytes = AC3_PERIOD_SIZE;
        } else if (out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
            write_bytes = EAC3_PERIOD_SIZE;
        } else if (out->hal_internal_format == AUDIO_FORMAT_MAT) {
            write_bytes = MAT_PERIOD_SIZE;
        } else if (patch->aformat == AUDIO_FORMAT_DTS_HD){
           period_mul = HBR_MULTIPLIER;
        }

        //ALOGI("%s  period_mul = %d ", __func__, period_mul);

        if (aml_dev->game_mode)
            write_bytes = LOW_LATENCY_PLAYBACK_PERIOD_SIZE * audio_stream_out_frame_size(&out->stream);

        // buffer size diff from allocation size, need to resize.
        if (patch->out_buf_size < (size_t)write_bytes * period_mul) {
            AM_LOGI("!!realloc out buf size from %zu to %d", patch->out_buf_size, write_bytes * period_mul);
            patch->out_buf = aml_audio_realloc(patch->out_buf, write_bytes * period_mul);
            patch->out_buf_size = write_bytes * period_mul;
        }

        pthread_mutex_lock(&patch->mutex);
        ALOGV("%s(), ringbuffer level read before wait--%d",
              __func__, get_buffer_read_space(ringbuffer));
        if (get_buffer_read_space(ringbuffer) < (write_bytes * period_mul)) {
            // wait 300ms
            ts_wait_time(&ts, 300000);
            pthread_cond_timedwait(&patch->cond, &patch->mutex, &ts);
        }
        pthread_mutex_unlock(&patch->mutex);

        ALOGV("%s(), ringbuffer level read after wait-- %d, %d * %d",
              __func__, get_buffer_read_space(ringbuffer), write_bytes, period_mul);

        if (is_dolby_ms12_support_compression_format(out->hal_internal_format)) {
            int pos_sync_word = -1;
            int need_drop_size = 0;
            int ring_buffer_read_space = get_buffer_read_space(ringbuffer);
            pos_sync_word = find_61937_sync_word_position_in_ringbuffer(ringbuffer);

            if (pos_sync_word >= 0) {
                need_drop_size = pos_sync_word;
                find_iec_sync_word = true;
            } else if (pos_sync_word == -1 && (find_iec_sync_word == false || ring_buffer_read_space > write_bytes)) {
                //write_bytes is frame size.
                //We should be able to find sync word within a frame size.
                //If we cannot find it, it is considered that there is a problem with the data
                need_drop_size = ring_buffer_read_space;
            } else if (pos_sync_word == -1) {
                continue;
            }

            if (need_drop_size >0) {
                AM_LOGE("data error, we drop data(%d) pos_sync_word(%d) find_sync_word(%d) read_space(%d)(%d) out_buf_size(%zu), write_bytes(%d)",
                    need_drop_size, pos_sync_word, find_iec_sync_word, ring_buffer_read_space, ringbuffer->size, patch->out_buf_size, write_bytes);
                int drop_size = ring_buffer_seek(ringbuffer, need_drop_size);
                if (drop_size != need_drop_size) {
                    AM_LOGE("drop fail, need_drop_size(%d) actual drop_size(%d)", need_drop_size, drop_size);
                }
                continue;
            }
        }
        if (get_buffer_read_space(ringbuffer) >= (write_bytes * period_mul)) {
            ret = ring_buffer_read(ringbuffer,
                                   (unsigned char*)patch->out_buf, write_bytes * period_mul);
            if (ret == 0) {
                AM_LOGE("ring_buffer read 0 data!  write_bytes(%d), period_mul(%d)", write_bytes, period_mul);
            }

            /* avsync for dev->dev patch*/
            if (patch && (patch->need_do_avsync == true) && (patch->input_signal_stable == true) &&
                    (aml_dev->patch_src == SRC_ATV || aml_dev->patch_src == SRC_HDMIIN ||
                    aml_dev->patch_src == SRC_LINEIN || aml_dev->patch_src == SRC_SPDIFIN)) {

                if (aml_dev_try_avsync(patch) < 0) {
                    AM_LOGE("aml_dev_try_avsync return error!");
                    continue;
                }
                if (patch->skip_frames) {
                    //ALOGD("%s(), skip this period data for avsync!", __func__);
                    usleep(5);
                    continue;
                }
            }
            if (aml_dev->audio_patching) {
                float source_gain = 1.0;
                if (aml_dev->patch_src == SRC_LINEIN)
                    source_gain = aml_dev->src_gain[INPORT_LINEIN];
                else if (aml_dev->patch_src == SRC_ATV)
                    source_gain = aml_dev->src_gain[INPORT_ATV];

                if (source_gain != 1.0)
                    apply_volume(source_gain, patch->out_buf, sizeof(uint16_t), ret);//inorder to adjust analog audio data
            }

            if (patch && patch->input_src == AUDIO_DEVICE_IN_HDMI) {
                stream_check_reconfig_param(stream_out);
            }
            stream_out->write(stream_out, patch->out_buf, ret);
        } else {
            ALOGV("%s(), no enough data in ring buffer, available data size:%d, need data size:%d", __func__,
                get_buffer_read_space(ringbuffer), (write_bytes * period_mul));
            usleep( (DEFAULT_PLAYBACK_PERIOD_SIZE) * 1000000 / 4 /
                stream_config.sample_rate);
        }
    }

    aml_dev->hw_device.close_output_stream(patch->dev, &out->stream);
    if (patch->out_buf) {
        aml_audio_free(patch->out_buf);
        patch->out_buf = NULL;
    }
    AM_LOGD("exit");

    return (void *)0;
}


static int create_patch_l(struct audio_hw_device *dev,
                        audio_devices_t input,
                        audio_devices_t output __unused)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct aml_audio_patch *patch;
    int play_buffer_size = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    pthread_attr_t attr;
    struct sched_param param;
    int ret = 0;

    AM_LOGD("enter");

    patch = aml_audio_calloc(1, sizeof(*patch));
    if (!patch) {
        return -ENOMEM;
    }

    patch->dev = dev;
    patch->input_src = input;
    patch->is_dtv_src = false;
    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
    patch->digital_input_fmt_change = false;
    aml_dev->audio_patch = patch;
    pthread_mutex_init(&patch->mutex, NULL);
    pthread_cond_init(&patch->cond, NULL);

    patch->in_sample_rate = 48000;
    patch->in_chanmask = AUDIO_CHANNEL_IN_STEREO;
    patch->output_src = AUDIO_DEVICE_OUT_SPEAKER;
    patch->out_sample_rate = 48000;
    patch->out_chanmask = AUDIO_CHANNEL_OUT_STEREO;
    patch->in_format = AUDIO_FORMAT_PCM_16_BIT;
    patch->out_format = AUDIO_FORMAT_PCM_16_BIT;

    /* when audio patch start, singal is unstale or
     * patch signal is unstable, it need do avsync
     */
    patch->need_do_avsync = true;
    patch->game_mode = aml_dev->game_mode;

    if (patch->out_format == AUDIO_FORMAT_PCM_16_BIT) {
        AM_LOGD("init audio ringbuffer game %d",  patch->game_mode);
        if (!patch->game_mode)
            ret = ring_buffer_init(&patch->aml_ringbuffer, 4 * 2 * play_buffer_size * PATCH_PERIOD_COUNT);
        else
            ret = ring_buffer_init(&patch->aml_ringbuffer, 2 * 4 * LOW_LATENCY_PLAYBACK_PERIOD_SIZE);
    } else {
        ret = ring_buffer_init(&patch->aml_ringbuffer, 4 * 4 * play_buffer_size * PATCH_PERIOD_COUNT);
    }

    if (ret < 0) {
        AM_LOGE("init audio ringbuffer failed");
        goto err_ring_buf;
    }

    if (IS_DIGITAL_IN_HW(patch->input_src)) {
        //TODO add sample rate and channel information
        ret = creat_pthread_for_audio_type_parse(&patch->audio_parse_threadID,
                &patch->audio_parse_para, &aml_dev->alsa_mixer, patch->input_src);
        if (ret !=  0) {
            AM_LOGE("create format parse thread failed");
            goto err_parse_thread;
        }
    }

    ret = pthread_create(&patch->audio_input_threadID, NULL,
                          &audio_patch_input_threadloop, patch);

    if (ret != 0) {
        AM_LOGE("Create input thread failed");
        goto err_in_thread;
    }
    ret = pthread_create(&patch->audio_output_threadID, NULL,
                          &audio_patch_output_threadloop, patch);
    if (ret != 0) {
        AM_LOGE("Create output thread failed");
        goto err_out_thread;
    }

    if (IS_DIGITAL_IN_HW(patch->input_src)) {
        ret = pthread_create(&patch->audio_signal_detect_threadID, NULL,
                            &audio_patch_signal_detect_threadloop, patch);
        if (ret != 0) {
            AM_LOGE("Create signal detect thread failed");
            goto err_signal_detect_thread;
        }
    }

    aml_dev->audio_patch = patch;
    AM_LOGD("exit");

    return 0;
err_signal_detect_thread:
    patch->signal_detect_thread_exit = 1;
    pthread_join(patch->audio_parse_threadID, NULL);
err_parse_thread:
    patch->output_thread_exit = 1;
    pthread_join(patch->audio_output_threadID, NULL);
err_out_thread:
    patch->input_thread_exit = 1;
    pthread_join(patch->audio_input_threadID, NULL);
err_in_thread:
    ring_buffer_release(&patch->aml_ringbuffer);
err_ring_buf:
    aml_audio_free(patch);
    return ret;
}

static int create_patch_ext(struct audio_hw_device *dev,
                            audio_devices_t input,
                            audio_devices_t output,
                            audio_patch_handle_t handle)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    int ret = 0;

    pthread_mutex_lock(&aml_dev->patch_lock);
    ret = create_patch_l(dev, input, output);
    pthread_mutex_unlock(&aml_dev->patch_lock);

    // successful
    if (!ret) {
        aml_dev->audio_patch->patch_hdl = handle;
    }

    return ret;

}

int release_patch_l(struct aml_audio_device *aml_dev)
{
    struct aml_audio_patch *patch = aml_dev->audio_patch;

    ALOGD("%s: enter", __func__);
    if (aml_dev->audio_patch == NULL) {
        ALOGD("%s(), no patch to release", __func__);
        goto exit;
    }

    patch->output_thread_exit = 1;
    pthread_join(patch->audio_output_threadID, NULL);

    patch->input_thread_exit = 1;
    pthread_join(patch->audio_input_threadID, NULL);

    if (IS_DIGITAL_IN_HW(patch->input_src)) {
        patch->signal_detect_thread_exit = 1;
        pthread_join(patch->audio_signal_detect_threadID, NULL);
    }

    if (IS_DIGITAL_IN_HW(patch->input_src))
        exit_pthread_for_audio_type_parse(patch->audio_parse_threadID,&patch->audio_parse_para);

    ring_buffer_release(&patch->aml_ringbuffer);
    aml_audio_free(patch);
    aml_dev->audio_patch = NULL;
    aml_dev->patch_start = false;
    ALOGD("%s: exit", __func__);

exit:
    return 0;
}


int release_patch(struct aml_audio_device *aml_dev)
{
    pthread_mutex_lock(&aml_dev->patch_lock);
    release_patch_l(aml_dev);
    pthread_mutex_unlock(&aml_dev->patch_lock);
    return 0;
}

int create_patch(struct audio_hw_device *dev,
                        audio_devices_t input,
                        audio_devices_t output)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    int ret = 0;

    pthread_mutex_lock(&aml_dev->patch_lock);
    ret = create_patch_l(dev, input, output);
    pthread_mutex_unlock(&aml_dev->patch_lock);

    return ret;
}

int set_tv_source_switch_parameters(struct audio_hw_device *dev, struct str_parms *parms)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    int ret = -1;
    char value[64] = {'\0'};

    ret = str_parms_get_str (parms, "hal_param_tuner_in", value, sizeof (value) );
    // tuner_in=atv: tuner_in=dtv
    if (ret >= 0 && adev->is_TV) {
        if (adev->tuner2mix_patch) {
            if (strncmp(value, "dtv", 3) == 0) {
                adev->patch_src = SRC_DTV;
                if (adev->audio_patching == 0) {
                    ALOGI("[audiohal_kpi][%s] create dtv patch start.\n", __func__);
#ifdef USE_DTV
                    ret = create_dtv_patch(dev, AUDIO_DEVICE_IN_TV_TUNER,AUDIO_DEVICE_OUT_SPEAKER);
#endif
                    if (ret == 0) {
                        adev->audio_patching = 1;
                    }
                    ALOGI("[audiohal_kpi][%s] create dtv patch end.\n", __func__);
                }
            } else if (strncmp(value, "atv", 3) == 0) {
                adev->patch_src = SRC_ATV;
                set_audio_source(&adev->alsa_mixer,
                        ATV, alsa_device_is_auge());
            }
            ALOGI("%s, tuner to mixer case, no need to create patch", __func__);
            goto exit;
        }
        if (strncmp(value, "dtv", 3) == 0) {
            // no audio patching in dtv
            if (adev->audio_patching && (adev->patch_src == SRC_ATV)) {
                // this is to handle atv->dtv case
                ALOGI("%s, atv->dtv", __func__);
                ret = release_patch(adev);
                if (!ret) {
                    adev->audio_patching = 0;
                }
            }
            ALOGI("%s, now the audio patch src is %s, the audio_patching is %d ", __func__,
                patchSrc2Str(adev->patch_src), adev->audio_patching);
#ifdef USE_DTV
           if ((adev->patch_src == SRC_DTV) && adev->audio_patching) {
                ALOGI("[audiohal_kpi] %s, now release the dtv patch now\n ", __func__);
                ret = release_dtv_patch(adev);
                if (!ret) {
                    adev->audio_patching = 0;
                }
            }
            ALOGI("[audiohal_kpi] %s, now end release dtv patch the audio_patching is %d ", __func__, adev->audio_patching);
            ALOGI("[audiohal_kpi] %s, now create the dtv patch now\n ", __func__);
            adev->patch_src = SRC_DTV;
            ret = create_dtv_patch(dev, AUDIO_DEVICE_IN_TV_TUNER,
                                   AUDIO_DEVICE_OUT_SPEAKER);
            if (ret == 0) {
                adev->audio_patching = 1;
            }
#endif
            ALOGI("[audiohal_kpi] %s, now end create dtv patch the audio_patching is %d ", __func__, adev->audio_patching);

        } else if (strncmp(value, "atv", 3) == 0) {
#ifdef USE_DTV
            // need create patching
            if ((adev->patch_src == SRC_DTV) && adev->audio_patching) {
                ALOGI ("[audiohal_kpi] %s, release dtv patching", __func__);
                ret = release_dtv_patch(adev);
                if (!ret) {
                    adev->audio_patching = 0;
                }
            }
#endif

            if (!adev->audio_patching) {
                ALOGI ("[audiohal_kpi] %s, create atv patching", __func__);
                set_audio_source(&adev->alsa_mixer,
                        ATV, alsa_device_is_auge());
                ret = create_patch (dev, AUDIO_DEVICE_IN_TV_TUNER, AUDIO_DEVICE_OUT_SPEAKER);
                // audio_patching ok, mark the patching status
                if (ret == 0) {
                    adev->audio_patching = 1;
                }
            }
            adev->patch_src = SRC_ATV;
        }
        goto exit;
    }

    /*
     * This is a work around when plug in HDMI-DVI connector
     * first time application only recognize it as HDMI input device
     * then it can know it's DVI in ,and send "audio=linein" message to audio hal
    */
    ret = str_parms_get_str(parms, "audio", value, sizeof(value));
    if (ret >= 0) {
        if (strncmp(value, "linein", 6) == 0) {
            struct audio_patch *pAudPatchTmp = NULL;

            get_audio_patch_by_src_dev(dev, AUDIO_DEVICE_IN_HDMI, &pAudPatchTmp);
            if (pAudPatchTmp == NULL) {
                ALOGE("%s,There is no autio patch using HDMI as input", __func__);
                goto exit;
            }
            if (pAudPatchTmp->sources[0].ext.device.type != AUDIO_DEVICE_IN_HDMI) {
                ALOGE("%s, pAudPatchTmp->sources[0].ext.device.type != AUDIO_DEVICE_IN_HDMI", __func__);
                goto exit;
            }

            // dev->dev (example: HDMI in-> speaker out)
            if (pAudPatchTmp->sources[0].type == AUDIO_PORT_TYPE_DEVICE
                && pAudPatchTmp->sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
                // This "adev->audio_patch" will be created in create_patch() function
                // which in current design is only in "dev->dev" case
                // make sure everything is matching, no error
                if (adev->audio_patch && (adev->patch_src == SRC_HDMIIN) /*&& pAudPatchTmp->id == adev->audio_patch->patch_hdl*/) {
                    // TODO: notices
                    // These codes must corresponding to the same case in adev_create_audio_patch() and adev_release_audio_patch()
                    // Anything change in the adev_create_audio_patch() . Must also change code here..
                    ALOGI("%s, create hdmi-dvi patching dev->dev", __func__);
                    release_patch(adev);
                    adev->active_inport = INPORT_LINEIN;
                    set_audio_source(&adev->alsa_mixer, LINEIN, alsa_device_is_auge());
                    ret = create_patch_ext(dev, AUDIO_DEVICE_IN_LINE, pAudPatchTmp->sinks[0].ext.device.type, pAudPatchTmp->id);
                    pAudPatchTmp->sources[0].ext.device.type = AUDIO_DEVICE_IN_LINE;
                }
            }

            // dev->mix (example: HDMI in-> USB out)
            if (pAudPatchTmp->sources[0].type == AUDIO_PORT_TYPE_DEVICE
                && pAudPatchTmp->sinks[0].type == AUDIO_PORT_TYPE_MIX) {
                ALOGI("%s, !!create hdmi-dvi patching dev->mix", __func__);
                if (adev->patch_src == SRC_HDMIIN) {
                    aml_dev2mix_parser_release(adev);
                }
                adev->active_inport = INPORT_LINEIN;
                adev->patch_src = SRC_LINEIN;
                pAudPatchTmp->sources[0].ext.device.type = AUDIO_DEVICE_IN_LINE;
                set_audio_source(&adev->alsa_mixer,
                        LINEIN, alsa_device_is_auge());
            }
        } else if (strncmp(value, "hdmi", 4) == 0 && adev->audio_patch) {
            //timing switch, audio is stable, do avsync once more
            adev->audio_patch->need_do_avsync = true;
        }
    }


exit:
    return ret;
}


