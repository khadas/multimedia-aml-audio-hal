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
#include <aml_volume_utils.h>
#include <cutils/properties.h>

#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "audio_dtv_utils.h"
#include "aml_dec_api.h"
#include "aml_ddp_dec_api.h"
#include "aml_audio_spdifout.h"
#include "alsa_config_parameters.h"
#include "dolby_lib_api.h"
#include "aml_esmode_sync.h"
#include "aml_android_utils.h"
#include "amlAudioMixer.h"


extern int insert_output_bytes (struct aml_stream_out *out, size_t size);

static void aml_audio_stream_volume_process(struct audio_stream_out *stream, void *buf, int sample_size, int channels, int bytes) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    apply_volume_fade(aml_out->last_volume_l, aml_out->volume_l, buf, sample_size, channels, bytes);
    aml_out->last_volume_l = aml_out->volume_l;
    aml_out->last_volume_r = aml_out->volume_r;
    return;
}

static inline bool check_sink_pcm_sr_cap(struct aml_audio_device *adev, int sample_rate)
{
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;

    switch (sample_rate) {
        case 32000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<0));
        case 44100: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<1));
        case 48000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<2));
        case 88200: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<3));
        case 96000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<4));
        case 176400: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<5));
        case 192000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<6));
        default: return false;
    }

    return false;
}

ssize_t aml_audio_spdif_output(struct audio_stream_out *stream, void **spdifout_handle, dec_data_info_t * data_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int ret = 0;

    if (data_info->data_len <= 0) {
        return -1;
    }

    if (aml_dev->patch_src ==  SRC_DTV && aml_out->need_drop_size > 0) {
        if (aml_dev->debug_flag > 1)
            ALOGI("%s, av sync drop data,need_drop_size=%d\n",
                __FUNCTION__, aml_out->need_drop_size);
        return ret;
    }

    if (*spdifout_handle == NULL) {
        spdif_config_t spdif_config = { 0 };
        spdif_config.audio_format = data_info->data_format;
        spdif_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        spdif_config.mute = aml_out->offload_mute;
        spdif_config.data_ch = data_info->data_ch;
        if (spdif_config.data_ch == 0) {
            spdif_config.data_ch = 2;
        }
        if (spdif_config.audio_format == AUDIO_FORMAT_IEC61937) {
            spdif_config.sub_format = data_info->sub_format;
        } else if (audio_is_linear_pcm(spdif_config.audio_format)) {
            if (data_info->data_ch == 6) {
                spdif_config.channel_mask = AUDIO_CHANNEL_OUT_5POINT1;
            } else if (data_info->data_ch == 8) {
                spdif_config.channel_mask = AUDIO_CHANNEL_OUT_7POINT1;
            }
        }
        spdif_config.is_dtscd = data_info->is_dtscd;
        spdif_config.rate = data_info->data_sr;

        ret = aml_audio_spdifout_open(spdifout_handle, &spdif_config);
        if (ret != 0) {
            return -1;
        }

        if (aml_out->offload_mute)
            aml_audio_spdifout_mute(*spdifout_handle, aml_out->offload_mute);
    }

    ALOGV("[%s:%d] format =0x%x length =%d", __func__, __LINE__, data_info->data_format, data_info->data_len);
    aml_audio_spdifout_processs(*spdifout_handle, data_info->buf, data_info->data_len);

    return ret;
}

int aml_audio_nonms12_render(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    int decoder_ret = -1,ret = -1;
    int dec_used_size = 0;
    int left_bytes = 0;
    int used_size = 0;
    bool  try_again = false;
    int alsa_latency = 0;
    int decoder_latency = 0;
    int ringbuf_latency = 0;
    struct audio_buffer ainput;

    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    struct aml_native_postprocess *VX_postprocess = &adev->native_postprocess;

    int return_bytes = abuffer->size;
    int out_frames = 0;
    void *input_buffer = (void *)abuffer->buffer;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int duration = 0;
    bool speed_enabled = false;
    bool dts_pcm_direct_output = false;

    /* handle HWSYNC audio data*/
    /* to do: move this to before hw_write*/
    if (aml_out->need_sync && aml_out->hwsync && (aml_out->avsync_type == AVSYNC_TYPE_MSYNC)) {
        uint64_t  cur_pts = abuffer->pts;
        int outsize = abuffer->size;
        audio_hwsync_t *hw_sync = aml_out->hwsync;
        uint32_t apts32 = 0;

        if (cur_pts != 0xffffffff && outsize > 0) {

            // if we got the frame body,which means we get a complete frame.
            //we take this frame pts as the first apts.
            //this can fix the seek discontinue,we got a fake frame,which maybe cached before the seek
            if (hw_sync->first_apts_flag == false) {
                if (aml_out->avsync_type == AVSYNC_TYPE_MSYNC) {
                    struct audio_policy policy;
                    int latency = out_get_latency(stream) * 90;
                    if (adev->useSubMix) {
                        struct subMixing *sm = adev->sm;
                        struct amlAudioMixer *audio_mixer = sm->mixerData;
                        ringbuf_latency = mixer_get_inport_latency_frames(audio_mixer, aml_out->inputPortID) / 48 * 90;
                        latency += ringbuf_latency;
                    }
                    apts32 = (cur_pts - latency) & 0xffffffff;
                    if (aml_getprop_bool("media.audiohal.ptslog"))
                        ALOGI("apts:%d pts:%lld latency:%d ringbuf_%d_latency:%d", apts32, cur_pts, latency, aml_out->inputPortID, ringbuf_latency);
                    aml_audio_hwsync_set_first_pts(aml_out->hwsync, apts32);
                    if (aml_out->msync_session)
                        av_sync_audio_render(aml_out->msync_session, apts32, &policy);
                }
            } else {
                if (aml_out->msync_session && (cur_pts != HWSYNC_PTS_NA)) {
                    struct audio_policy policy;
                    uint64_t latency = out_get_latency(stream) * 90;
                    if (adev->useSubMix) {
                        struct subMixing *sm = adev->sm;
                        struct amlAudioMixer *audio_mixer = sm->mixerData;
                        ringbuf_latency = mixer_get_inport_latency_frames(audio_mixer, aml_out->inputPortID) / 48 * 90;
                        latency += ringbuf_latency;
                    }
                    uint32_t apts32 = (cur_pts - latency) & 0xffffffff;
                    if (aml_getprop_bool("media.audiohal.ptslog"))
                        ALOGI("apts:%d pts:%lld latency:%lld ringbuf_%d_latency:%d", apts32, cur_pts, latency, aml_out->inputPortID, ringbuf_latency);

                    av_sync_audio_render(aml_out->msync_session, apts32, &policy);
                    /* TODO: for non-amaster mode, handle sync policy on audio side */
                }
            }
        }
    }

    if (aml_out->aml_dec == NULL) {
        config_output(stream, true);
    }
    aml_dec_t *aml_dec = aml_out->aml_dec;

    ainput.buffer = abuffer->buffer;
    ainput.size = abuffer->size;
    ainput.pts = abuffer->pts;
    if (aml_dec) {
        dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
        dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
        dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;
        left_bytes = abuffer->size;

        aml_dec->in_frame_pts = (NULL_INT64 == ainput.pts) ? aml_dec->out_frame_pts : ainput.pts;
        ainput.pts = aml_dec->in_frame_pts;

        do {
            ALOGV("%s() in raw len=%d", __func__, left_bytes);
            used_size = 0;
            ainput.buffer = abuffer->buffer + dec_used_size;
            ainput.size = left_bytes;

            AM_LOGI_IF(adev->debug_flag, "in pts:0x%llx (%lld ms) size: %d", ainput.pts, ainput.pts/90, ainput.size);
            decoder_ret = aml_decoder_process(aml_dec, &ainput, &used_size);
            if (decoder_ret == AML_DEC_RETURN_TYPE_CACHE_DATA) {
                ALOGV("[%s:%d] cache the data to decode", __func__, __LINE__);
                return abuffer->size;
            } else if (decoder_ret < 0) {
                ALOGV("[%s:%d] decoder error, ret:%d", __func__, __LINE__, decoder_ret);
            }

            left_bytes -= used_size;
            dec_used_size += used_size;
            AM_LOGI_IF(adev->debug_flag, "out pts:0x%llx (%lld ms) pcm len =%d raw len=%d used_size %d total used size %d left_bytes =%d",
                dec_pcm_data->pts, dec_pcm_data->pts/90, dec_pcm_data->data_len,dec_raw_data->data_len,
                used_size, dec_used_size, left_bytes);

            // write pcm data
            if (dec_pcm_data->data_len > 0) {
                if (aml_dec->format == AUDIO_FORMAT_PCM_32_BIT)
                    aml_out->config.format = PCM_FORMAT_S16_LE;
                // aml_audio_dump_audio_bitstreams("/data/dec_data.raw", dec_pcm_data->buf, dec_pcm_data->data_len);
                out_frames = dec_pcm_data->data_len /( 2 * dec_pcm_data->data_ch);
                aml_dec->out_frame_pts = dec_pcm_data->pts;

                audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
                void  *dec_data = (void *)dec_pcm_data->buf;
                int pcm_len = dec_pcm_data->data_len;
                if (adev->patch_src  == SRC_DTV &&
                    (adev->start_mute_flag == 1 || adev->tv_mute)) {
                    memset(dec_pcm_data->buf, 0, dec_pcm_data->data_len);
                }

                if (patch) {
                    patch->sample_rate = dec_pcm_data->data_sr;
                }

                /* For dts certification:
                 * DTS 88.2K/96K pcm direct output case.
                 * If the PCM after decoding is 88.2k/96k, then direct output.
                 * Need to check whether HDMI sink supports 88.2k/96k or not.*/
                if (adev->hdmi_format == PCM
                    && is_dts_format(aml_out->hal_internal_format)
                    && dec_pcm_data->data_sr > 48000
                    && check_sink_pcm_sr_cap(adev, dec_pcm_data->data_sr)) {
                    dts_pcm_direct_output = true;
                }

                if (dec_pcm_data->data_sr != OUTPUT_ALSA_SAMPLERATE ) {
                    ret = aml_audio_resample_process_wrapper(&aml_out->resample_handle, dec_pcm_data->buf,
                    pcm_len, dec_pcm_data->data_sr, dec_pcm_data->data_ch);
                    if (ret != 0) {
                        ALOGE("aml_audio_resample_process_wrapper failed");
                    } else {
                        dec_data = aml_out->resample_handle->resample_buffer;
                        pcm_len = aml_out->resample_handle->resample_size;
                    }
                    aml_out->config.rate = OUTPUT_ALSA_SAMPLERATE;
                } else {
                    if (dec_pcm_data->data_sr > 0)
                        aml_out->config.rate = dec_pcm_data->data_sr;
                }
                if (!adev->is_TV) {
                    aml_out->config.channels = dec_pcm_data->data_ch;
                }

                /*process the stream volume before mix*/
                if (aml_audio_property_get_bool("vendor.media.decoder.tone",false)) {
                    if (2 == dec_pcm_data->data_ch) {
                        appply_tone_16bit2ch(dec_pcm_data->buf, dec_pcm_data->data_len);
                    }
                }
                if (!adev->useSubMix) {
                    aml_audio_stream_volume_process(stream, dec_data, sizeof(int16_t), dec_pcm_data->data_ch, pcm_len);
                }

                if (dec_pcm_data->data_ch == 6) {
                    ret = audio_VX_post_process(VX_postprocess, (int16_t *)dec_data, pcm_len);
                    if (ret > 0) {
                        pcm_len = ret; /* VX will downmix 6ch to 2ch, pcm size will be changed */
                        dec_pcm_data->data_ch /= 3;
                    }
                }
                if (adev->audio_hal_info.first_decoding_frame == false) {
                    aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &adev->dec_stream_info);
                    adev->audio_hal_info.first_decoding_frame = true;
                    adev->audio_hal_info.is_decoding = true;
                    ALOGI("[%s:%d] aml_decoder_stream_info %d %d", __func__, __LINE__, adev->dec_stream_info.dec_info.stream_ch, adev->dec_stream_info.dec_info.stream_sr);
                }

                if ((true == aml_out->need_sync) && (NULL != aml_out->hwsync) && (AVSYNC_TYPE_MEDIASYNC == aml_out->avsync_type))
                {
                    audio_hwsync_t *phwsync = aml_out->hwsync;

                    if (dec_pcm_data->data_ch != 0)
                    {
                        duration = (pcm_len * 1000) / (2 * dec_pcm_data->data_ch * aml_out->config.rate);
                    }

                    alsa_latency = 90 *(out_get_latency_frames(stream) * 1000) / aml_out->config.rate;

                    int tuning_delay = 0;
                    if (NULL != phwsync->get_tuning_latency) {
                        tuning_delay = phwsync->get_tuning_latency(stream);
                    }

                    phwsync->es_mediasync.cur_outapts = aml_dec->out_frame_pts - decoder_latency - alsa_latency - tuning_delay;

                    AM_LOGI_IF(adev->debug_flag, "sr:%d, ch:%d, format:0x%x, alsa_latency:%d(ms), duration:%d, in_apts:%lld(%llx), "
                        "out_pts:%lld, out_frames:%d, cur_outapts:%lld",
                        dec_pcm_data->data_sr, dec_pcm_data->data_ch, dec_pcm_data->data_format, alsa_latency/90, duration,
                        aml_dec->in_frame_pts, aml_dec->in_frame_pts, aml_dec->out_frame_pts, out_frames, phwsync->es_mediasync.cur_outapts);

                    if (adev->useSubMix) {
                        struct subMixing *sm = adev->sm;
                        struct amlAudioMixer *audio_mixer = sm->mixerData;
                        ringbuf_latency = mixer_get_inport_latency_frames(audio_mixer, aml_out->inputPortID) / 48 * 90;
                        phwsync->es_mediasync.cur_outapts -= ringbuf_latency;
                    }

                    //sync process here
                    if (aml_out->alsa_status_changed) {
                        ALOGI("[%s:%d]aml_out->alsa_running_status %d", __FUNCTION__, __LINE__, aml_out->alsa_running_status);
                        mediasync_wrap_setParameter(phwsync->es_mediasync.mediasync, MEDIASYNC_KEY_ALSAREADY, &aml_out->alsa_running_status);
                        aml_out->alsa_status_changed = false;
                    }
                    sync_process_res process_result = ESSYNC_AUDIO_OUTPUT;
                    process_result = aml_hwmediasync_nonms12_process(stream, duration, &speed_enabled);
                    if (ESSYNC_AUDIO_EXIT == process_result)
                    {
                        break;
                    }
                    else if (ESSYNC_AUDIO_DROP == process_result)
                    {
                        continue;
                    }

                    // pcm case, asink do speed before audio_hal.
                    if (fabs(aml_out->output_speed - 1.0f) > 1e-6) {
                        ret = aml_audio_speed_process_wrapper(&aml_out->speed_handle, dec_data,
                                                pcm_len, aml_out->output_speed,
                                                OUTPUT_ALSA_SAMPLERATE, dec_pcm_data->data_ch);
                        if (ret != 0) {
                            ALOGE("aml_audio_speed_process_wrapper failed");
                        } else {

                            ALOGV("data_len=%d, speed_size=%d\n", pcm_len, aml_out->speed_handle->speed_size);
                            dec_data = aml_out->speed_handle->speed_buffer;
                            pcm_len = aml_out->speed_handle->speed_size;
                        }
                    }
                }
                //for next loop
                //ainput.pts = dec_pcm_data->pts + out_frames * 1000 * 90 /dec_pcm_data->data_sr;
                aml_dec->out_frame_pts = ainput.pts;
                if (adev->patch_src == SRC_HDMIIN ||
                            adev->patch_src == SRC_SPDIFIN ||
                            adev->patch_src == SRC_LINEIN ||
                            adev->patch_src == SRC_ATV) {

                    if (patch && patch->need_do_avsync) {
                         memset(dec_data, 0, pcm_len);
                    } else {
                        if (adev->mute_start)  {
                            /* fade in start */
                            ALOGI("start fade in");
                            start_ease_in(adev);
                            adev->mute_start = false;
                        }
                    }
                    if (adev->audio_patching) {
                        /*ease in or ease out*/
                        aml_audio_ease_process(adev->audio_ease, dec_data, pcm_len);
                    }

                }

                if (eDolbyMS12Lib == adev->dolby_lib_type_last || !adev->useSubMix) {
                    /*ease in or ease out*/
                    aml_audio_ease_process(adev->audio_ease, dec_data, pcm_len);
                    aml_hw_mixer_mixing(&adev->hw_mixer, dec_data, pcm_len, output_format);
                    if (audio_hal_data_processing(stream, dec_data, pcm_len, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                        hw_write(stream, output_buffer, output_buffer_bytes, output_format);
                    }
                } else {
                    out_write_direct_pcm(stream, dec_data, pcm_len);
                }
            }

            if (aml_out->optical_format != adev->optical_format) {
                ALOGI("optical format change from 0x%x --> 0x%x", aml_out->optical_format, adev->optical_format);
                aml_out->optical_format = adev->optical_format;
                if (aml_out->spdifout_handle != NULL) {
                    aml_audio_spdifout_close(aml_out->spdifout_handle);
                    aml_out->spdifout_handle = NULL;
                }
                if (aml_out->spdifout2_handle != NULL) {
                    aml_audio_spdifout_close(aml_out->spdifout2_handle);
                    aml_out->spdifout2_handle = NULL;
                }

            }

            // write raw data
            /*for pcm case, we check whether it has muti channel pcm or 96k/88.2k pcm */
            if (!dts_pcm_direct_output && !speed_enabled && audio_is_linear_pcm(aml_dec->format) && raw_in_data->data_ch > 2) {
                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
            } else if (dts_pcm_direct_output && !speed_enabled) {
                aml_audio_stream_volume_process(stream, dec_pcm_data->buf, sizeof(int16_t), dec_pcm_data->data_ch, dec_pcm_data->data_len);
                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, dec_pcm_data);
            }

            if (!dts_pcm_direct_output && !speed_enabled && aml_out->optical_format != AUDIO_FORMAT_PCM_16_BIT) {
                if (dec_raw_data->data_sr > 0) {
                    aml_out->config.rate = dec_raw_data->data_sr;
                }

                if (aml_dec->format == AUDIO_FORMAT_E_AC3 || aml_dec->format == AUDIO_FORMAT_AC3) {
                    if (adev->dual_spdif_support) {
                        /*output raw ddp to hdmi*/
                        if (aml_dec->format == AUDIO_FORMAT_E_AC3 && aml_out->optical_format == AUDIO_FORMAT_E_AC3) {
                            if (raw_in_data->data_len)
                                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
                        }

                        /*output dd data to spdif*/
                        if (dec_raw_data->data_len > 0)
                            aml_audio_spdif_output(stream, &aml_out->spdifout2_handle, dec_raw_data);
                    } else {
                        if (aml_dec->format == AUDIO_FORMAT_E_AC3 && aml_out->optical_format == AUDIO_FORMAT_E_AC3) {
                            if (raw_in_data->data_len) {
                                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
                            }
                        } else if (aml_out->optical_format == AUDIO_FORMAT_AC3) {
                            if (dec_raw_data->data_len) {
                                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, dec_raw_data);
                            }
                        }
                    }
                } else {
                    aml_audio_spdif_output(stream, &aml_out->spdifout_handle, dec_raw_data);
                }

            }

            /*special case  for dts , dts decoder need to follow aml_dec_api.h */
            if ((aml_out->hal_internal_format == AUDIO_FORMAT_DTS ||
                aml_out->hal_internal_format == AUDIO_FORMAT_DTS_HD )&&
                decoder_ret == AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN ) {
                try_again = true;
            }
        } while ((left_bytes > 0) || aml_dec->fragment_left_size || try_again);
    }

exit:
    if (patch)
       patch->decoder_offset +=return_bytes;
    return return_bytes;
}

static audio_format_t get_dcvlib_output_format(audio_format_t src_format, struct aml_audio_device *aml_dev)
{
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    if (aml_dev->hdmi_format == AUTO) {
        if (src_format == AUDIO_FORMAT_E_AC3 && aml_dev->sink_capability == AUDIO_FORMAT_E_AC3) {
            output_format = AUDIO_FORMAT_E_AC3;
        } else if (src_format == AUDIO_FORMAT_AC3 &&
                (aml_dev->sink_capability == AUDIO_FORMAT_E_AC3 || aml_dev->sink_capability == AUDIO_FORMAT_AC3)) {
            output_format = AUDIO_FORMAT_AC3;
        }
    }
    return output_format;
}

bool aml_decoder_output_compatible(struct audio_stream_out *stream, audio_format_t sink_format __unused, audio_format_t optical_format) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    bool is_compatible = true;

    if (aml_out->hal_internal_format != aml_out->aml_dec->format) {
        ALOGI("[%s:%d] not compatible. dec format:%#x -> cur format:%#x", __func__, __LINE__,
            aml_out->aml_dec->format, aml_out->hal_internal_format);
        return false;
    }

    if ((aml_out->aml_dec->format == AUDIO_FORMAT_AC3)
        || (aml_out->aml_dec->format == AUDIO_FORMAT_E_AC3)) {
        aml_dcv_config_t* dcv_config = (aml_dcv_config_t *)(&aml_out->dec_config);
        if (((optical_format == AUDIO_FORMAT_PCM_16_BIT) && (dcv_config->digital_raw > AML_DEC_CONTROL_DECODING))
            || ((optical_format == AUDIO_FORMAT_E_AC3) && (dcv_config->digital_raw != AML_DEC_CONTROL_RAW))) {
                is_compatible = false;
        }
    } else if ((aml_out->aml_dec->format == AUDIO_FORMAT_DTS)
                || (aml_out->aml_dec->format == AUDIO_FORMAT_DTS_HD)) {
        aml_dca_config_t* dca_config = (aml_dca_config_t *)(&aml_out->dec_config);
        if ((optical_format == AUDIO_FORMAT_PCM_16_BIT) && (dca_config->digital_raw > AML_DEC_CONTROL_DECODING)) {
            is_compatible = false;
        }
    }

    return is_compatible;
}


static void ddp_decoder_config_prepare(struct audio_stream_out *stream, aml_dcv_config_t * ddp_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_arc_hdmi_desc *p_hdmi_descs = &adev->hdmi_descs;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = NULL;

    if (patch ) {
        demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    }

    adev->dcvlib_bypass_enable = 0;

    ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;

    if (demux_info && demux_info->dual_decoder_support) {
        ddp_config->decoding_mode = DDP_DECODE_MODE_AD_DUAL;
    } else if (aml_out->ad_substream_supported) {
        ddp_config->decoding_mode = DDP_DECODE_MODE_AD_SUBSTREAM;
    } else {
        ddp_config->decoding_mode = DDP_DECODE_MODE_SINGLE;
    }

    audio_format_t output_format = get_dcvlib_output_format(aml_out->hal_internal_format, adev);
    if (output_format != AUDIO_FORMAT_PCM_16_BIT && output_format != AUDIO_FORMAT_PCM_32_BIT) {
            ddp_config->decoding_mode = DDP_DECODE_MODE_SINGLE;
    }

    if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
        ddp_config->nIsEc3 = 1;
    } else if (aml_out->hal_internal_format == AUDIO_FORMAT_AC3) {
        ddp_config->nIsEc3 = 0;
    }
    /*check if the input format is contained with 61937 format*/
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        ddp_config->is_iec61937 = true;
    } else {
        ddp_config->is_iec61937 = false;
    }

    ALOGI("%s digital_raw:%d, dual_output_flag:%d, is_61937:%d, IsEc3:%d"
        , __func__, ddp_config->digital_raw, aml_out->dual_output_flag, ddp_config->is_iec61937, ddp_config->nIsEc3);
    return;
}

static void dts_decoder_config_prepare(struct audio_stream_out *stream, aml_dca_config_t * dts_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    adev->dtslib_bypass_enable = 0;

    dts_config->digital_raw = AML_DEC_CONTROL_CONVERT;
    dts_config->is_dtscd = aml_out->is_dtscd;
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937 && !dts_config->is_dtscd) {
        dts_config->is_iec61937 = true;
    } else {
        dts_config->is_iec61937 = false;
    }

    dts_config->dev = (void *)adev;
    ALOGI("%s digital_raw:%d, dual_output_flag:%d, is_iec61937:%d, is_dtscd:%d"
        , __func__, dts_config->digital_raw, aml_out->dual_output_flag, dts_config->is_iec61937, dts_config->is_dtscd);
    return;
}

static void mad_decoder_config_prepare(struct audio_stream_out *stream, aml_mad_config_t * mad_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    mad_config->channel    = aml_out->hal_ch;
    mad_config->samplerate = aml_out->hal_rate;
    mad_config->mpeg_format = aml_out->hal_format;
    return;
}

static void faad_decoder_config_prepare(struct audio_stream_out *stream, aml_faad_config_t * faad_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    faad_config->channel    = aml_out->hal_ch;
    faad_config->samplerate = aml_out->hal_rate;
    faad_config->aac_format = aml_out->hal_format;

    return;
}

static void pcm_decoder_config_prepare(struct audio_stream_out *stream, aml_pcm_config_t * pcm_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    pcm_config->channel    = aml_out->hal_ch;
    pcm_config->samplerate = aml_out->hal_rate;
    pcm_config->pcm_format = aml_out->hal_format;
    pcm_config->max_out_channels = adev->hdmi_descs.pcm_fmt.max_channels;
    if (ATTEND_TYPE_EARC  == aml_audio_earctx_get_type(adev)) {
        pcm_config->max_out_channels = 8;
    }

    return;
}

static void adpcm_decoder_config_prepare(struct audio_stream_out *stream, aml_adpcm_config_t * adpcm_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    adpcm_config->channel    = aml_out->hal_ch;
    adpcm_config->samplerate = aml_out->hal_rate;
    adpcm_config->pcm_format = aml_out->hal_format;
    adpcm_config->block_size = aml_out->hal_decode_block_size;

    return;
}

static void flac_decoder_config_prepare(struct audio_stream_out *stream, aml_flac_config_t * flac_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    flac_config->channel = aml_out->hal_ch;
    flac_config->samplerate = aml_out->hal_rate;
    flac_config->flac_format = aml_out->hal_format;

    return;
}

static void vorbis_decoder_config_prepare(struct audio_stream_out *stream, aml_vorbis_config_t * vorbis_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    vorbis_config->channel = aml_out->hal_ch;
    vorbis_config->samplerate = aml_out->hal_rate;
    vorbis_config->vorbis_format = aml_out->hal_format;

    return;
}

int aml_decoder_config_prepare(struct audio_stream_out *stream, audio_format_t format, aml_dec_config_t *dec_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = NULL;
    if (patch) {
        demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    }

    if (demux_info) {
        dec_config->ad_decoder_supported = demux_info->dual_decoder_support;
        dec_config->ad_mixing_enable = demux_info->associate_audio_mixing_enable;
        dec_config->mixer_level = adev->mixing_level;
        dec_config->advol_level = adev->advol_level;
        ALOGI("mixer_level %d adev->associate_audio_mixing_enable %d",adev->mixing_level, demux_info->associate_audio_mixing_enable);
    }
#ifdef NO_SERVER
    adev->decoder_drc_control = aml_audio_get_drc_control(&adev->alsa_mixer);
#endif
    dec_config->drc_control = adev->decoder_drc_control;
    dec_config->downmix_type = adev->downmix_type;

    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        ddp_decoder_config_prepare(stream, &dec_config->dcv_config);
        break;
    }
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD: {
        dts_decoder_config_prepare(stream, &dec_config->dca_config);
        break;
    }
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_8_BIT:
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_LPCM_DVD:
    case AUDIO_FORMAT_PCM_LPCM_1394:
    case AUDIO_FORMAT_PCM_LPCM_BLURAY: {
        pcm_decoder_config_prepare(stream, &dec_config->pcm_config);
        break;
    }
    case AUDIO_FORMAT_PCM_ADPCM_IMA_WAV: {
        adpcm_decoder_config_prepare(stream, &dec_config->adpcm_config);
        break;
    }
    case AUDIO_FORMAT_MP3:
    case AUDIO_FORMAT_MP2: {
        mad_decoder_config_prepare(stream, &dec_config->mad_config);
        break;
    }
    case AUDIO_FORMAT_AAC:
    case AUDIO_FORMAT_AAC_LATM: {
        faad_decoder_config_prepare(stream, &dec_config->faad_config);
        break;
    }
    case AUDIO_FORMAT_FLAC: {
        flac_decoder_config_prepare(stream, &dec_config->flac_config);
        break;
    }
    case AUDIO_FORMAT_VORBIS: {
        vorbis_decoder_config_prepare(stream, &dec_config->vorbis_config);
        break;
    }
    default:
        break;
    }

    return 0;
}


