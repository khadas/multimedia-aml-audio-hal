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

#define LOG_TAG "audio_hw_ms12_render"
//#define LOG_NDEBUG 0

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/log.h>


#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "audio_dtv_utils.h"

#include "dolby_lib_api.h"
#include "aml_volume_utils.h"
#include "audio_hw_ms12_v2.h"
#include "aml_audio_timer.h"
#include "alsa_config_parameters.h"

#include "aml_audio_ms12_sync.h"
#include "aml_esmode_sync.h"
#include "aml_audio_hal_avsync.h"
#include "audio_media_sync_util.h"

#define MS12_MAIN_WRITE_LOOP_THRESHOLD                  (2000)
#define AUDIO_IEC61937_FRAME_SIZE 4

int aml_audio_get_cur_ms12_latency(struct audio_stream_out *stream) {

    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = NULL;
    if (NULL != patch)
    {
        aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    }

    int ms12_latencyms = 0;

    uint64_t inputnode_consumed = dolby_ms12_get_main_bytes_consumed(stream);
    uint64_t frames_generated = dolby_ms12_get_main_pcm_generated(stream);
    if (is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)) {
        if (demux_info && (demux_info->dual_decoder_support))
            ms12_latencyms = (frames_generated - ms12->master_pcm_frames) / 48;
        else
            ms12_latencyms = (ms12->ms12_main_input_size - inputnode_consumed) / aml_out->ddp_frame_size * 32 + (frames_generated - ms12->master_pcm_frames) / 48;
    } else {
        ms12_latencyms = ((ms12->ms12_main_input_size - inputnode_consumed ) / 4 + frames_generated - ms12->master_pcm_frames) / 48;
    }
    if (adev->debug_flag)
        ALOGI("ms12_latencyms %d  ms12_main_input_size %lld inputnode_consumed %lld frames_generated %lld master_pcm_frames %lld",
        ms12_latencyms, ms12->ms12_main_input_size, inputnode_consumed,frames_generated, ms12->master_pcm_frames);
    return ms12_latencyms;

}

int aml_audio_ms12_process_wrapper(struct audio_stream_out *stream, struct audio_buffer *abuffer)

{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    int write_bytes = abuffer->size;
    int return_bytes = abuffer->size;
    int ret = 0;
    int total_write = 0;
    void *buffer = (void *)abuffer->buffer;
    const void *write_buf = abuffer->buffer;
    struct aml_audio_patch *patch = adev->audio_patch;
    int write_retry = 0;
    size_t used_size = 0;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int ms12_write_failed = 0;
    int consume_size = 0,remain_size = 0,ms12_thredhold_size = 256;
    unsigned long long all_pcm_len1 = 0;
    unsigned long long all_pcm_len2 = 0;
    unsigned long long all_zero_len = 0;
    audio_format_t output_format = get_output_format (stream);
    if (adev->debug_flag) {
        ALOGD("%s:%d hal_format:%#x, output_format:0x%x, sink_format:0x%x",
            __func__, __LINE__, aml_out->hal_format, output_format, adev->sink_format);
    }

    remain_size = dolby_ms12_get_main_buffer_avail(NULL);
    dolby_ms12_get_pcm_output_size(&all_pcm_len1, &all_zero_len);

    if (aml_out->hw_sync_mode && aml_out->hwsync) {

        // missing code with aml_audio_hwsync_checkin_apts, need to add for netflix tunnel mode. zzz
        if (abuffer->pts != HWSYNC_PTS_NA) {
            aml_audio_hwsync_checkin_apts(aml_out->hwsync, aml_out->hwsync->payload_offset, abuffer->pts);
        }
#ifndef BUILD_LINUX
        // dolby_ms12_hwsync_checkin_pts checkin PTS info to MS12's global PTS sync table, which
        // is only used by UDC decoder to detect PTS gap for NTS. On Linux, audio gap for timestamp
        // is sent from upper layer directly and the gap detection is not needed.
        if (continuous_mode(adev) && !audio_is_linear_pcm(aml_out->hal_internal_format) && adev->is_netflix) {
            dolby_ms12_hwsync_checkin_pts(aml_out->hwsync->payload_offset, abuffer->pts);
        }
#endif
        aml_out->hwsync->payload_offset += abuffer->size;
        //change
        // when msync is used, the original first apts->audio start logic from legacy amaster tsync
        // implementation in hw_write() is moved to this place when main input samples are injected
        // because the sync policy return from msync side may need block when audio starts to play.
        if (aml_out->hwsync->first_apts_flag == false && AVSYNC_TYPE_MSYNC == aml_out->avsync_type) {
            // may be blocked by msync_cond for initial playback timing control
            // the first checkin pts should be consumed by aml_audio_hwsync_audio_process for av_sync_audio_start
            //aml_audio_hwsync_audio_process(aml_out->hwsync, 0, NULL);
            aml_audio_hwsync_audio_process(aml_out->hwsync, 0, abuffer->pts, NULL);
            // when dropping is needed at av_sync_audio_start
            // reset payload_offset since the data will not be
            // sent to MS12 pipeline. The consumed bytes number
            // from MS12 should match PTS checkin record from
            // payload_offset
            if (aml_out->msync_action == AV_SYNC_AA_DROP) {
                aml_out->hwsync->payload_offset = 0;
                ALOGI("MSYNC start dropping @0x%llx, %d bytes", aml_out->hwsync->first_apts, abuffer->size);
                goto exit;
            }

        }
    }

    if (is_bypass_dolbyms12(stream)) {
        if (adev->debug_flag) {
            ALOGI("%s passthrough dolbyms12, format %#x\n", __func__, aml_out->hal_format);
        }
        output_format = aml_out->hal_internal_format;
        if (audio_hal_data_processing (stream, write_buf, write_bytes, &output_buffer, &output_buffer_bytes, output_format) == 0)
            hw_write (stream, output_buffer, output_buffer_bytes, output_format);
    } else {
        /*not continuous mode, we use sink gain control the volume*/
        if (!continuous_mode(adev)) {
            /*when it is non continuous mode, we bypass data here*/
            dolby_ms12_bypass_process(stream, write_buf, write_bytes);
        }
        /*begin to write, clear the total write*/
        total_write = 0;
re_write:
        if (adev->debug_flag) {
            ALOGI("%s dolby_ms12_main_process before write_bytes %zu!\n", __func__, write_bytes);
        }

        used_size = 0;
        ret = dolby_ms12_main_process(stream, (char*)write_buf + total_write, write_bytes, &used_size);
        if (ret == 0) {
            if (adev->debug_flag) {
                ALOGI("%s dolby_ms12_main_process return %d, return used_size %zu!\n", __FUNCTION__, ret, used_size);
            }
            if (used_size < write_bytes && write_retry < MS12_MAIN_WRITE_LOOP_THRESHOLD) {
                if (adev->debug_flag) {
                    ALOGI("%s dolby_ms12_main_process used  %zu,write total %zu,left %zu\n", __FUNCTION__, used_size, write_bytes, write_bytes - used_size);
                }
                total_write += used_size;
                write_bytes -= used_size;
                /*if ms12 doesn't consume any data, we need sleep*/
                if (used_size == 0) {
                    aml_audio_sleep(1000);
                }
                if (adev->debug_flag >= 2) {
                    ALOGI("%s sleeep 1ms\n", __FUNCTION__);
                }
                write_retry++;
                if (adev->ms12.dolby_ms12_enable) {
                    goto re_write;
                }
            }
            if (write_retry >= MS12_MAIN_WRITE_LOOP_THRESHOLD) {
                ALOGE("%s main write retry time output,left %zu", __func__, write_bytes);
                //bytes -= write_bytes;
                ms12_write_failed = 1;
            }
        } else {
            ALOGE("%s dolby_ms12_main_process failed %d", __func__, ret);
        }
    }

    int size = dolby_ms12_get_main_buffer_avail(NULL);
    dolby_ms12_get_pcm_output_size(&all_pcm_len2, &all_zero_len);
    if (patch)
        patch->decoder_offset += remain_size + write_bytes - size;

exit:

    return return_bytes;

}
int aml_audio_ms12_render(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    int ret = -1;
    int dec_used_size = 0;
    int used_size = 0;
    int left_bytes = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    int return_bytes = abuffer->size;
    struct aml_audio_patch *patch = adev->audio_patch;
    int out_frames = 0;
    bool bypass_aml_dec = false;

    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    if (is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)
        || is_multi_channel_pcm(stream)) {
        bypass_aml_dec = true;
    }

    if (bypass_aml_dec) {
        ret = aml_audio_ms12_process_wrapper(stream, abuffer);
        if (MEDIA_SYNC_TSMODE(aml_out)) {
            if (aml_out->alsa_status_changed) {
                ALOGI("[%s:%d] aml_out->alsa_running_status %d", __FUNCTION__, __LINE__, aml_out->alsa_running_status);
                //aml_dtvsync_setParameter(patch->dtvsync, MEDIASYNC_KEY_ALSAREADY, &aml_out->alsa_running_status);
                mediasync_wrap_setParameter(aml_out->hwsync->es_mediasync.mediasync, MEDIASYNC_KEY_ALSAREADY, &aml_out->alsa_running_status);
                aml_out->alsa_status_changed = false;
            }
        }
    } else {
        if (aml_out->aml_dec == NULL) {
            config_output(stream, true);
        }
        aml_dec_t *aml_dec = aml_out->aml_dec;
        aml_dec_info_t dec_info;
        audio_mediasync_util_t * mediasync_util = aml_audio_get_mediasync_util_handle();
        struct audio_buffer ainput;
        ainput.buffer = abuffer->buffer;
        ainput.size = abuffer->size;
        ainput.pts = abuffer->pts;

        if ((MEDIA_SYNC_TSMODE(aml_out)) || (MEDIA_SYNC_ESMODE(aml_out)))
        {
            aml_dec->in_frame_pts = (NULL_INT64 == ainput.pts) ? aml_dec->out_frame_pts : ainput.pts;
            ainput.pts = aml_dec->in_frame_pts;
        }

        if (aml_dec) {
            dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
            dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
            dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;
            left_bytes = abuffer->size;
            struct audio_buffer pcm_abuffer = {0};

            do {

                ainput.buffer = abuffer->buffer + dec_used_size;
                ainput.size = left_bytes;

                AM_LOGI_IF(adev->debug_flag, "in pts:0x%llx (%lld ms) size: %d", ainput.pts, ainput.pts/90, ainput.size);
                ret = aml_decoder_process(aml_dec, &ainput, &used_size);

                if (ret < 0) {
                    ALOGI("aml_decoder_process error");
                    return return_bytes;
                }
                left_bytes -= used_size;
                dec_used_size += used_size;
                AM_LOGI_IF(adev->debug_flag, "out pts:0x%llx (%lld ms) pcm len =%d raw len=%d used_size %d total used size %d left_bytes =%d",
                    dec_pcm_data->pts, dec_pcm_data->pts/90, dec_pcm_data->data_len,dec_raw_data->data_len,
                    used_size, dec_used_size, left_bytes);

                if (dec_pcm_data->data_len > 0) {
                    if (adev->audio_hal_info.first_decoding_frame == false) {
                        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &adev->dec_stream_info);
                        adev->audio_hal_info.first_decoding_frame = true;
                        adev->audio_hal_info.is_decoding = true;
                        //ALOGI("[%s:%d] aml_decoder_stream_info %d %d", __func__, __LINE__, adev->dec_stream_info.dec_info.stream_ch, adev->dec_stream_info.dec_info.stream_sr);
                        if (48000 != aml_out->hal_rate || ms12->config_channel_mask != 0x3)
                        {
                            ms12->config_sample_rate  = aml_out->hal_rate = 48000;
                            ms12->config_channel_mask = 0x3;
                        }
                    }
                    out_frames = dec_pcm_data->data_len / (2 * dec_pcm_data->data_ch);
                    aml_dec->out_frame_pts = dec_pcm_data->pts;

                    void  *dec_data = (void *)dec_pcm_data->buf;
                    if ((adev->audio_patch != NULL) &&
                        (adev->patch_src == SRC_DTV || adev->patch_src == SRC_HDMIIN) &&
                        (adev->start_mute_flag == 1)) {
                        memset(dec_pcm_data->buf, 0, dec_pcm_data->data_len);
                    }
                    if (dec_pcm_data->data_sr > 0) {
                        aml_out->config.rate = dec_pcm_data->data_sr;
                    }
                    if (patch) {
                        patch->sample_rate = dec_pcm_data->data_sr;
                    }
                    if (dec_pcm_data->data_sr != OUTPUT_ALSA_SAMPLERATE) {
                         ret = aml_audio_resample_process_wrapper(&aml_out->resample_handle, dec_pcm_data->buf, dec_pcm_data->data_len, dec_pcm_data->data_sr, dec_pcm_data->data_ch);
                         if (ret != 0) {
                             ALOGI("aml_audio_resample_process_wrapper failed");
                         } else {
                             dec_data = aml_out->resample_handle->resample_buffer;
                             dec_pcm_data->data_len = aml_out->resample_handle->resample_size;
                         }
                    }

                    if ((MEDIA_SYNC_TSMODE(aml_out)) || (MEDIA_SYNC_ESMODE(aml_out)))
                    {
                        int64_t out_apts;
                        out_apts = aml_dec->out_frame_pts;
                        if (aml_audio_mediasync_util_checkin_apts(mediasync_util, mediasync_util->payload_offset, out_apts) < 0) {
                            ALOGE("[%s:%d] checkin apts(%llx) data_size(%zu) fail",
                                __FUNCTION__, __LINE__, out_apts, mediasync_util->payload_offset);
                        }
                        mediasync_util->payload_offset += dec_pcm_data->data_len;

                        if (mediasync_util->media_sync_debug)
                             ALOGI("out_frames %d, out_apts %llx, in_frame_pts %llx, out_frame_pts %llx, data_sr %d, data_ch %d, frame_offset %zu",
                                    out_frames, out_apts, aml_dec->in_frame_pts, aml_dec->out_frame_pts,
                                    dec_pcm_data->data_sr, dec_pcm_data->data_ch, mediasync_util->payload_offset);
                    }
                    //for next loop
                    ainput.pts = dec_pcm_data->pts + out_frames * 1000 * 90 /dec_pcm_data->data_sr;
                    aml_dec->out_frame_pts = ainput.pts;
                    pcm_abuffer.size = dec_pcm_data->data_len;
                    pcm_abuffer.buffer = dec_data;
                    pcm_abuffer.pts = aml_dec->out_frame_pts;
                    pcm_abuffer.format = AUDIO_FORMAT_PCM_16_BIT;
                    ret = aml_audio_ms12_process_wrapper(stream, &pcm_abuffer);
                }
            } while ((left_bytes > 0) || aml_dec->fragment_left_size);
        }
    }
    return return_bytes;
}

