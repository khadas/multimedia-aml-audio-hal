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

#define LOG_TAG "audio_hw_hal_render"
//#define LOG_NDEBUG 0

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/log.h>


#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "dtv_patch_utils.h"

#include "dolby_lib_api.h"
#include "aml_volume_utils.h"
#include "audio_hw_ms12_v2.h"
#include "aml_audio_timer.h"
#include "alsa_config_parameters.h"

#include "aml_audio_ms12_sync.h"
#include "aml_esmode_sync.h"
#include "dtv_patch_hal_avsync.h"

#define MS12_MAIN_WRITE_LOOP_THRESHOLD                  (2000)

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
        AM_LOGD("hal_format:%#x, output_format:0x%x, sink_format:0x%x, apts:0x%llx, size:%d",
            aml_out->hal_format, output_format, adev->sink_format, abuffer->pts, abuffer->size);
    }

    remain_size = dolby_ms12_get_main_buffer_avail(NULL);
    dolby_ms12_get_pcm_output_size(&all_pcm_len1, &all_zero_len);

    if ((AVSYNC_TYPE_NULL != aml_out->avsync_type) && (NULL != aml_out->avsync_ctx)) {
        // missing code with avsync_checkin_apts, need to add for netflix tunnel mode. zzz
        if (abuffer->pts != HWSYNC_PTS_NA) {
            avsync_checkin_apts(aml_out->avsync_ctx, aml_out->avsync_ctx->payload_offset, abuffer->pts);
            pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
            aml_out->avsync_ctx->payload_offset += abuffer->size;
            pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
        } else if (!abuffer->b_pts_valid) {
            pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
            aml_out->avsync_ctx->payload_offset += abuffer->size;
            pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
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
        /*begin to write, clear the total write*/
        total_write = 0;
re_write:
        if (adev->debug_flag) {
            ALOGI("%s dolby_ms12_main_process before write_bytes %zu, pts %llx!\n", __func__, write_bytes, abuffer->pts);
        }

        used_size = 0;
        ret = dolby_ms12_main_process(stream, abuffer, &used_size);
        if (ret == 0) {
            if (adev->debug_flag) {
                ALOGI("%s dolby_ms12_main_process return %d, return used_size %zu!\n", __FUNCTION__, ret, used_size);
            }
            if (used_size < abuffer->size && write_retry < MS12_MAIN_WRITE_LOOP_THRESHOLD) {
                if (adev->debug_flag) {
                    ALOGI("%s dolby_ms12_main_process used  %zu,write total %zu,left %zu\n", __FUNCTION__, used_size, abuffer->size, abuffer->size - used_size);
                }
                abuffer->buffer += used_size;
                abuffer->size -= used_size;
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
                ALOGE("%s main write retry time output,left %zu", __func__, abuffer->size);
                //bytes -= write_bytes;
                ms12_write_failed = 1;
            }
        } else {
            ALOGE("%s dolby_ms12_main_process failed %d", __func__, ret);
        }

        // MS12 bypass flow is here
        av_sync_policy_e av_sync_policy = AV_SYNC_AUDIO_NORMAL_OUTPUT;
        if ((AVSYNC_TYPE_NULL != aml_out->avsync_type) && (NULL != aml_out->avsync_ctx)) {
            av_sync_policy = aml_out->avsync_ctx->last_sync_policy;
        }

        // Fixme: we only process drop policy, other policy we do not process
        if (av_sync_policy != AV_SYNC_AUDIO_DROP_PCM && adev->hdmi_format == BYPASS) {
            if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
                dolby_ms12_bypass_process(stream, abuffer->iec_data_buf, abuffer->iec_data_size);
            } else {
                dolby_ms12_bypass_process(stream, write_buf, write_bytes);
            }
        }
    }

    int size = dolby_ms12_get_main_buffer_avail(NULL);
    dolby_ms12_get_pcm_output_size(&all_pcm_len2, &all_zero_len);
exit:

    return return_bytes;

}

int aml_audio_amldec_process(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev  = aml_out->dev;
    int return_bytes = abuffer->size;
    int ret = -1;

    //aml_dec
    if (NULL == aml_out->aml_dec) {
        ret = config_output(stream, true);
        if (0 > ret) {
            return return_bytes;
        }
    }

    aml_dec_t *aml_dec = aml_out->aml_dec;
    struct audio_buffer ainput;
    ainput.buffer = abuffer->buffer;
    ainput.size   = abuffer->size;
    ainput.pts    = abuffer->pts;
    ainput.b_pts_valid = true;

    /* check the input apts and set valid flag */
    if ((NULL_INT64 == ainput.pts) || (ainput.pts == aml_dec->last_in_frame_pts)) {
        AM_LOGI_IF(adev->debug_flag, "ainput.pts(0x%llx)->out_frame_pts(0x%llx)", ainput.pts, aml_dec->out_frame_pts);

        /* if input apts invalid use the out_frame_pts instead */
        ainput.pts            = aml_dec->out_frame_pts;
        ainput.b_pts_valid    = false;
    }
    aml_dec->last_in_frame_pts = ainput.pts;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;

    int used_size     = 0;
    int out_frames    = 0;
    int dec_used_size = 0;
    int left_bytes    = ainput.size;
    struct audio_buffer out_abuffer = {0};

    AM_LOGI_IF(adev->debug_flag, "new_in_pts:0x%llx (%lld ms) size: %d", ainput.pts, ainput.pts/90, ainput.size);
    do {
        ainput.buffer += used_size;
        ainput.size   = left_bytes;

        AM_LOGI_IF(adev->debug_flag, "in pts:0x%llx (%lld ms) size: %d", ainput.pts, ainput.pts/90, ainput.size);
        ret = aml_decoder_process(aml_dec, &ainput, &used_size);
        if (0 > ret) {
            AM_LOGW("aml_decoder_process error, ret:%d", ret);
            break;
        }
        left_bytes    -= used_size;
        dec_used_size += used_size;

        /* cleanup the pts valid flag to false for next round, the aml dec will accumulate the output apts by decode frame,
           and when pts valid is true, the aml dec will aligned output apts with input pts add cached frames for the input pts may not continue */
        ainput.b_pts_valid     = false;
        aml_dec->out_frame_pts = ainput.pts;

        AM_LOGI_IF(adev->debug_flag, "out pts:0x%llx (%lld ms) pcm len =%d raw len=%d used_size %d total used size %d left_bytes =%d",
            dec_pcm_data->pts, dec_pcm_data->pts/90, dec_pcm_data->data_len,dec_raw_data->data_len,
            used_size, dec_used_size, left_bytes);

        if (0 >= dec_pcm_data->data_len) {
            continue;
        }

        if (false == adev->audio_hal_info.first_decoding_frame) {
            aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &adev->dec_stream_info);
            adev->audio_hal_info.first_decoding_frame = true;
            adev->audio_hal_info.is_decoding = true;
            struct dolby_ms12_desc *ms12 = &(adev->ms12);
            //ALOGI("[%s:%d] aml_decoder_stream_info %d %d", __func__, __LINE__, adev->dec_stream_info.dec_info.stream_ch, adev->dec_stream_info.dec_info.stream_sr);
            if (48000 != aml_out->hal_rate || ms12->config_channel_mask != 0x3)
            {
                ms12->config_sample_rate  = aml_out->hal_rate = 48000;
                ms12->config_channel_mask = 0x3;
            }
        }

        void  *dec_data = (void *)dec_pcm_data->buf;
        if (1 == adev->start_mute_flag) {
            memset(dec_pcm_data->buf, 0, dec_pcm_data->data_len);
        }
        if (dec_pcm_data->data_sr > 0) {
            aml_out->config.rate = dec_pcm_data->data_sr;
        }

        if (dec_pcm_data->data_sr != OUTPUT_ALSA_SAMPLERATE && dec_pcm_data->data_sr > 0) {
             ret = aml_audio_resample_process_wrapper(&aml_out->resample_handle, dec_pcm_data->buf, dec_pcm_data->data_len, dec_pcm_data->data_sr, dec_pcm_data->data_ch);
             if (ret != 0) {
                 ALOGI("aml_audio_resample_process_wrapper failed");
             } else {
                 dec_data = aml_out->resample_handle->resample_buffer;
                 dec_pcm_data->data_len = aml_out->resample_handle->resample_size;
             }
        }

        aml_dec->out_frame_pts = dec_pcm_data->pts;
        out_abuffer.size   = dec_pcm_data->data_len;
        out_abuffer.buffer = dec_data;
        out_abuffer.pts    = dec_pcm_data->pts;
        out_abuffer.format = AUDIO_FORMAT_PCM_16_BIT;
        ret = aml_audio_ms12_process_wrapper(stream, &out_abuffer);
    } while ((left_bytes > 0) || aml_dec->fragment_left_size);

    return return_bytes;
}

int aml_audio_ms12_render(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    int return_bytes = abuffer->size;

    if (is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)
        || is_multi_channel_pcm(stream)) { //dolby_dec
        return_bytes = aml_audio_ms12_process_wrapper(stream, abuffer);
        if ((aml_out->alsa_status_changed) && (AVSYNC_TYPE_MEDIASYNC == aml_out->avsync_type)) {
            ALOGI("[%s:%d] aml_out->alsa_running_status %d", __FUNCTION__, __LINE__, aml_out->alsa_running_status);
            mediasync_wrap_setParameter(aml_out->avsync_ctx->mediasync_ctx->handle, MEDIASYNC_KEY_ALSAREADY, &aml_out->alsa_running_status);
            aml_out->alsa_status_changed = false;
        }
    } else { //aml_dec
        return_bytes = aml_audio_amldec_process(stream, abuffer);
    }

    return return_bytes;
}

int aml_audio_ad_render(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev  = aml_out->dev;
    int return_bytes = abuffer->size;
    int ret = -1;
    //dolby codec: write ad data to ms12
    if (eDolbyMS12Lib == adev->dolby_lib_type && is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)) {
        ms12_ad_process(stream, abuffer);
        goto exit;
    } else if (eDolbyDcvLib == adev->dolby_lib_type && (aml_out->hal_internal_format == AUDIO_FORMAT_AC3 || aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3)) {
        aml_dec_t *aml_dec = aml_out->aml_dec;
        int used = 0;
        ret = aml_decoder_ad_process(stream, abuffer, &used);
        AM_LOGI_IF(adev->debug_flag, "ad_frame_size %d used %d, ret %d", abuffer->size, used, ret);
        goto exit;
    }

    //nondolby codec: init aml_dec first
    if (NULL == aml_out->aml_dec) {
        ret = config_output(stream, true);
        if (0 > ret) {
            AM_LOGE("aml_dec for ad init failed");
            return -1;
        }
    }

    aml_dec_t *aml_dec = aml_out->aml_dec;
    struct audio_buffer ainput;
    ainput.buffer = abuffer->buffer;
    ainput.size = abuffer->size;
    ainput.pts = abuffer->pts;

    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;

    int used_size     = 0;
    int dec_used_size = 0;
    int left_bytes    = ainput.size;
    struct audio_buffer out_abuffer = {0};

    //AM_LOGI_IF(adev->debug_flag, "out %p size: %d", aml_out, ainput.size);
    do {
        ainput.buffer += used_size;
        ainput.size   = left_bytes;

        AM_LOGI_IF(adev->debug_flag, "out %p,in pts:0x%llx (%lld ms) size: %d, used %d, total used %d", aml_out,
            ainput.pts, ainput.pts/90, ainput.size, used_size, dec_used_size);
        ret = aml_decoder_process(aml_dec, &ainput, &used_size);
        if (0 > ret) {
            AM_LOGW("aml_decoder_process error, ret:%d", ret);
            break;
        }
        left_bytes    -= used_size;
        dec_used_size += used_size;

        AM_LOGI_IF(adev->debug_flag, "dec pcm len =%d raw len=%d used %d total used %d left_bytes =%d",
            dec_pcm_data->data_len,dec_raw_data->data_len, used_size, dec_used_size, left_bytes);

        if (0 >= dec_pcm_data->data_len) {
            continue;
        }

        void *dec_data = (void *)dec_pcm_data->buf;
        if (dec_pcm_data->data_sr > 0) {
            aml_out->config.rate = dec_pcm_data->data_sr;
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

        aml_dec->out_frame_pts = dec_pcm_data->pts;
        out_abuffer.size   = dec_pcm_data->data_len;
        out_abuffer.buffer = dec_data;
        out_abuffer.pts    = dec_pcm_data->pts;
        out_abuffer.format = AUDIO_FORMAT_PCM_16_BIT;

        //nondolby codec: then use mixer to mix ad
        if (eDolbyMS12Lib == adev->dolby_lib_type) {//use ms12 to mix ad pcm
            ret = ms12_ad_process(stream, &out_abuffer);
        } else {
            if (adev->useSubMix) {//use submix to mix ad pcm
                out_write_direct_pcm(stream, out_abuffer.buffer, out_abuffer.size);
            }
        }

        if (0 != ret) {
            AM_LOGE("dolby_ms12_ad_process failed");
        }
    } while ((left_bytes > 0) || aml_dec->fragment_left_size);
exit:
    return return_bytes;
}

