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
#define LOG_TAG "audio_hw_hal_sync"
//#define LOG_NDEBUG 0
#include <cutils/log.h>
#include <cutils/properties.h>

#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "a2dp_hal.h"
#if defined(MS12_V24_ENABLE) || defined(MS12_V26_ENABLE)
#include "audio_hw_ms12_v2.h"
#endif
#include "aml_audio_avsync_table.h"
#include "aml_audio_spdifout.h"
#include "dolby_lib_api.h"

#define MS12_OUTPUT_5_1_DDP "vendor.media.audio.ms12.output.5_1_ddp"

typedef enum DEVICE_TYPE {
    STB = 0,
    TV = 1,
    SBR = 2
}device_type_t;


static int get_ms12_dv_tunnel_input_latency(audio_format_t input_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_DV_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_DV_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_DV_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_DV_TUNNEL_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = AVSYNC_MS12_DV_TUNNEL_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_DV_TUNNEL_AC4_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    return latency_ms;
}

static int get_ms12_nontunnel_input_latency(audio_format_t input_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        prop_name = AVSYNC_MS12_NONTUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        prop_name = AVSYNC_MS12_NONTUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = AVSYNC_MS12_NONTUNNEL_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_AC4_LATENCY;
        break;
    }
    default:
        break;

    }
    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    return latency_ms;
}


static int get_ms12_tunnel_input_latency(audio_format_t input_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = AVSYNC_MS12_TUNNEL_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_AC4_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    return latency_ms;
}


static int get_ms12_netflix_nontunnel_input_latency(audio_format_t input_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        prop_name = AVSYNC_MS12_NETFLIX_NONTUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_NONTUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        prop_name = AVSYNC_MS12_NETFLIX_NONTUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_NONTUNNEL_DDP_LATENCY;
        break;
    }
    default:
        break;

    }
    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    return latency_ms;
}


static int get_ms12_netflix_tunnel_input_latency(audio_format_t input_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_NETFLIX_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for non tunnel dolby ddp5.1 case:netlfix AV1/HDR10/HEVC case*/
        prop_name = AVSYNC_MS12_NETFLIX_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_TUNNEL_DDP_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    return latency_ms;
}


static int get_ms12_output_latency(audio_format_t output_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (output_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        latency_ms = AVSYNC_MS12_PCM_OUT_LATENCY;
        prop_name = AVSYNC_MS12_PCM_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_AC3: {
        latency_ms = AVSYNC_MS12_DD_OUT_LATENCY;
        prop_name = AVSYNC_MS12_DD_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_E_AC3: {
        latency_ms = AVSYNC_MS12_DDP_OUT_LATENCY;
        prop_name = AVSYNC_MS12_DDP_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_MAT: {
        latency_ms = AVSYNC_MS12_MAT_OUT_LATENCY;
        prop_name = AVSYNC_MS12_MAT_OUT_LATENCY_PROPERTY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }
    ALOGV("%s output format =0x%x latency ms =%d", __func__, output_format, latency_ms);
    return latency_ms;

}

static int get_ms12_netflix_output_latency(audio_format_t output_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (output_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        latency_ms = AVSYNC_MS12_NETFLIX_PCM_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_PCM_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_AC3: {
        latency_ms = AVSYNC_MS12_NETFLIX_DD_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_DD_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_E_AC3: {
        latency_ms = AVSYNC_MS12_NETFLIX_DDP_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_DDP_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_MAT: {
        latency_ms = AVSYNC_MS12_NETFLIX_MAT_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_MAT_OUT_LATENCY_PROPERTY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }
    ALOGV("%s output format =0x%x latency ms =%d", __func__, output_format, latency_ms);
    return latency_ms;

}


int get_ms12_port_latency( enum OUT_PORT port, audio_format_t output_format)
{
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (port)  {
        case OUTPORT_HDMI_ARC:
            if (output_format == AUDIO_FORMAT_AC3) {
                latency_ms = AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY_PROPERTY;
            } else if (output_format == AUDIO_FORMAT_E_AC3) {
                latency_ms = AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY_PROPERTY;
            } else {
                latency_ms = AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY_PROPERTY;
            }
            break;
        case OUTPORT_HDMI:
            if (output_format == AUDIO_FORMAT_AC3) {
                latency_ms = AVSYNC_MS12_HDMI_OUT_DD_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_OUT_DD_LATENCY_PROPERTY;
            } else if (output_format == AUDIO_FORMAT_E_AC3) {
                latency_ms = AVSYNC_MS12_HDMI_OUT_DDP_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_OUT_DDP_LATENCY_PROPERTY;
            } else if (output_format == AUDIO_FORMAT_MAT) {
                latency_ms = AVSYNC_MS12_HDMI_OUT_MAT_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_OUT_MAT_LATENCY_PROPERTY;
            } else {
                latency_ms = AVSYNC_MS12_HDMI_OUT_PCM_LATENCY;
                prop_name = AVSYNC_MS12_HDMI_OUT_PCM_LATENCY_PROPERTY;
            }
            break;
        case OUTPORT_SPEAKER:
        case OUTPORT_AUX_LINE:
            latency_ms = AVSYNC_MS12_HDMI_SPEAKER_LATENCY;
            prop_name = AVSYNC_MS12_HDMI_SPEAKER_LATENCY_PROPERTY;
            break;
        default :
            break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }
    ALOGV("%s outport =%d output format =0x%x latency ms =%d", __func__, port, output_format, latency_ms);
    return latency_ms;
}


static int get_ms12_nontunnel_latency_offset(enum OUT_PORT port
    , audio_format_t input_format
    , audio_format_t output_format
    , bool is_netflix
    , device_type_t platform_type
    , bool is_eARC)
{
    int latency_ms = 0;
    int input_latency_ms = 0;
    int output_latency_ms = 0;
    int port_latency_ms = 0;
    bool is_tunnel = false;
    if (is_netflix) {
        input_latency_ms  = get_ms12_netflix_nontunnel_input_latency(input_format);
        output_latency_ms = get_ms12_netflix_output_latency(output_format);
    } else {
        input_latency_ms  = get_ms12_nontunnel_input_latency(input_format);
        output_latency_ms = get_ms12_output_latency(output_format);
        port_latency_ms   = get_ms12_port_latency(port, output_format);
    }
    latency_ms = input_latency_ms + output_latency_ms + port_latency_ms;
    ALOGV("%s total latency =%d ms in=%d ms out=%d ms port=%d ms", __func__,
        latency_ms, input_latency_ms, output_latency_ms, port_latency_ms);
    return latency_ms;
}

int get_ms12_atmos_latency_offset(bool tunnel, bool is_netflix)
{
    int latency_ms = 0;
    char *prop_name = NULL;
    if (is_netflix) {
        if (tunnel) {
            /*tunnel atmos case*/
            prop_name = AVSYNC_MS12_NETFLIX_TUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_NETFLIX_TUNNEL_ATMOS_LATENCY;
        } else {
            /*non tunnel atmos case*/
            prop_name = AVSYNC_MS12_NETFLIX_NONTUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_NETFLIX_NONTUNNEL_ATMOS_LATENCY;
        }
    } else {
        if (tunnel) {
            /*tunnel atmos case*/
            prop_name = AVSYNC_MS12_TUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_TUNNEL_ATMOS_LATENCY;
        } else {
            /*non tunnel atmos case*/
            prop_name = AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY;
        }
    }
    latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    return latency_ms;
}


int get_ms12_bypass_latency_offset(bool tunnel)
{
    int latency_ms = 0;
    char *prop_name = NULL;
    if (tunnel) {
        /*tunnel atmos case*/
        prop_name = AVSYNC_MS12_TUNNEL_BYPASS_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_BYPASS_LATENCY;
    } else {
        /*non tunnel atmos case*/
        prop_name = AVSYNC_MS12_NONTUNNEL_BYPASS_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_BYPASS_LATENCY;
    }
    latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    return latency_ms;
}


uint32_t out_get_ms12_latency_frames(struct audio_stream_out *stream, bool ignore_default)
{
    struct aml_stream_out *hal_out = (struct aml_stream_out *)stream;
    snd_pcm_sframes_t frames = 0;
    struct snd_pcm_status status;
    uint32_t whole_latency_frames;
    int ret = 0;
    struct aml_audio_device *adev = hal_out->dev;
    struct aml_stream_out *ms12_out = NULL;
    struct pcm_config *config = &adev->ms12_config;
    int mul = 1;
    unsigned int device;

    if (continuous_mode(adev)) {
        ms12_out = adev->ms12_out;
    } else {
        ms12_out = hal_out;
    }

    if (ms12_out == NULL) {
        return 0;
    }

    device = ms12_out->device;
    ms12_out->pcm = adev->pcm_handle[device];
//#ifdef ENABLE_BT_A2DP
#ifndef BUILD_LINUX
    if (ms12_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        return a2dp_out_get_latency(adev) * ms12_out->hal_rate / 1000;
    }
#endif
    if (adev->optical_format == AUDIO_FORMAT_PCM_16_BIT) {
        whole_latency_frames = config->start_threshold;
    } else {
        whole_latency_frames = hal_out->config.period_size * hal_out->config.period_count;
    }

    if (ignore_default)
        whole_latency_frames = 0;

    if (!ms12_out->pcm || !pcm_is_ready(ms12_out->pcm)) {
        return whole_latency_frames / mul;
    }

    ret = pcm_ioctl(ms12_out->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    if (status.state != PCM_STATE_RUNNING && status.state != PCM_STATE_DRAINING) {
        return whole_latency_frames / mul;
    }

    ret = pcm_ioctl(ms12_out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    ALOGV("%s frames =%ld mul=%d", __func__, frames, mul);
    return frames / mul;
}

static int get_ms12_tunnel_video_delay(void) {
    int latency_ms = 0;
    char *prop_name = NULL;

    prop_name= AVSYNC_MS12_TUNNEL_VIDEO_DELAY_PROPERTY;
    latency_ms = AVSYNC_MS12_TUNNEL_VIDEO_DELAY;
    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }
    ALOGV("%s latency ms =%d", __func__, latency_ms);
    return latency_ms;

}

int get_ms12_tuning_latency_pts(struct audio_stream_out *stream)
{
    struct aml_stream_out *out    = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int32_t total_latency_pts = 0;
    int32_t total_latency_ms  = 0;
    int alsa_delay_ms         = 0;
    int tuning_delay_ms       = 0;
    int atmos_tuning_delay_ms = 0;
    int input_latency_ms      = 0;
    int output_latency_ms     = 0;
    int port_latency_ms       = 0;
    int bypass_delay_ms       = 0;
    int video_delay_ms        = 0;

    alsa_delay_ms = (int32_t)out_get_ms12_latency_frames(stream, false) / 48;

    if (adev->is_netflix) {
        input_latency_ms = get_ms12_netflix_tunnel_input_latency(out->hal_internal_format);
    } else {
        input_latency_ms = get_ms12_tunnel_input_latency(out->hal_internal_format);
    }

    if (adev->is_netflix) {
        output_latency_ms = get_ms12_netflix_output_latency(adev->ms12.optical_format);
    } else {
        output_latency_ms = get_ms12_output_latency(adev->ms12.optical_format);
        if (adev->ms12.is_bypass_ms12) {
            bypass_delay_ms = get_ms12_bypass_latency_offset(true);
        }
    }

    if (0 == adev->is_netflix) {
        port_latency_ms = get_ms12_port_latency(adev->active_outport, adev->ms12.optical_format);
    }
    tuning_delay_ms = input_latency_ms + output_latency_ms + port_latency_ms;

    if (adev->ms12.is_dolby_atmos || adev->atoms_lock_flag) {
        /*
         * In DV AV sync, the ATMOS(DDP_JOC) item, it will add atmos_tunning_delay into the latency_frames.
         * If other case choose an diff value, here seperate by is_netflix.
         */
        atmos_tuning_delay_ms = get_ms12_atmos_latency_offset(true, adev->is_netflix);
    }

    if (adev->is_TV) {
        video_delay_ms = get_ms12_tunnel_video_delay();
    }

    total_latency_ms  = alsa_delay_ms + tuning_delay_ms + atmos_tuning_delay_ms + bypass_delay_ms + video_delay_ms;
    total_latency_pts = total_latency_ms * 90;    //90k

    AM_LOGI_IF((adev->debug_flag > 1), "latency_pts:0x%x(%d ms), alsa delay(%d ms), tuning delay(%d ms), atmos(%d ms), bypass_delay(%d ms), video_delay(%d ms)",
            total_latency_pts, total_latency_ms, alsa_delay_ms, tuning_delay_ms, atmos_tuning_delay_ms, bypass_delay_ms, video_delay_ms);
    return total_latency_pts;
}

static int get_nonms12_netflix_tunnel_input_latency(audio_format_t input_format) {
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_NONMS12_NETFLIX_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_NONMS12_NETFLIX_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for tunnel dolby ddp5.1 case:netlfix AV1/HDR10/HEVC case*/
        prop_name = AVSYNC_NONMS12_NETFLIX_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_NONMS12_NETFLIX_TUNNEL_DDP_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    return latency_ms;
}

int get_nonms12_tuning_latency_pts(struct audio_stream_out *stream)
{
    struct aml_stream_out *out    = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int32_t total_latency_pts = 0;
    int32_t total_latency_ms  = 0;
    int alsa_delay_ms         = 0;
    int tuning_delay_ms       = 0;
    int atmos_tuning_delay_ms = 0;
    int input_latency_ms      = 0;
    int output_latency_ms     = 0;
    int port_latency_ms       = 0;
    int bypass_delay_ms       = 0;
    int video_delay_ms        = 0;

    alsa_delay_ms = (int32_t)out_get_outport_latency(stream);

    if (adev->is_netflix) {
        input_latency_ms  = get_nonms12_netflix_tunnel_input_latency(out->hal_internal_format);
    } else {
        // to do.
    }
    //output_latency_ms / port_latency_ms /atmos_tuning_delay_ms to do.

    tuning_delay_ms = input_latency_ms + output_latency_ms + port_latency_ms;

    total_latency_ms  = alsa_delay_ms + tuning_delay_ms + atmos_tuning_delay_ms + bypass_delay_ms + video_delay_ms;
    total_latency_pts = total_latency_ms * 90;    //90k

    AM_LOGI_IF((adev->debug_flag > 1), "latency_pts:0x%x(%d ms), alsa delay(%d ms), tuning delay(%d ms), atmos(%d ms), bypass_delay(%d ms), video_delay(%d ms)",
            total_latency_pts, total_latency_ms, alsa_delay_ms, tuning_delay_ms, atmos_tuning_delay_ms, bypass_delay_ms, video_delay_ms);
    return total_latency_pts;
}

int get_latency_pts(struct audio_stream_out *stream)
{
    struct aml_stream_out *out    = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        return get_ms12_tuning_latency_pts(stream);
    }
    else {
        return get_nonms12_tuning_latency_pts(stream);
    }
}

int aml_audio_get_ms12_presentation_position(const struct audio_stream_out *stream, uint64_t *frames, struct timespec *timestamp)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int frame_latency = 0, timems_latency = 0;
    bool b_raw_in = false;
    bool b_raw_out = false;
    uint64_t frames_written_hw = out->last_frames_position;
    device_type_t platform_type = STB;
    bool is_earc = (ATTEND_TYPE_EARC == aml_audio_earctx_get_type(adev));

    if (adev->is_STB) {
        platform_type = STB;
    }
    else if (adev->is_TV) {
        platform_type = TV;
    }
    else if (adev->is_SBR) {
        platform_type = SBR;
    }

    if (frames_written_hw == 0) {
        ALOGV("%s(), not ready yet", __func__);
        return -EINVAL;
    }
    *frames = frames_written_hw;
    *timestamp = out->lasttimestamp;

    if (adev->continuous_audio_mode) {
        if (direct_continous((struct audio_stream_out *)stream)) {
            frames_written_hw = adev->ms12.last_frames_position;
            *timestamp = adev->ms12.timestamp;
        }

        if (out->is_normal_pcm && adev->ms12.dolby_ms12_enable) {
            frames_written_hw = adev->ms12.sys_audio_frame_pos;
            *timestamp = adev->ms12.sys_audio_timestamp;
        }

        *frames = frames_written_hw;

        if (adev->ms12.is_bypass_ms12) {
            frame_latency = get_ms12_bypass_latency_offset(false) * 48;
        } else {
            frame_latency = get_ms12_nontunnel_latency_offset(adev->active_outport,
                                                               out->hal_internal_format,
                                                               adev->ms12.sink_format,
                                                               adev->is_netflix,
                                                               platform_type,
                                                               is_earc) * 48;
            if (adev->ms12.is_dolby_atmos || adev->atoms_lock_flag) {
                frame_latency += get_ms12_atmos_latency_offset(false, adev->is_netflix) * 48;
            }
        }
    }

    ALOGV("[%s]adev->active_outport %d out->hal_internal_format %x adev->ms12.sink_format %x adev->continuous_audio_mode %d \n",
            __func__,adev->active_outport, out->hal_internal_format, adev->ms12.sink_format, adev->continuous_audio_mode);
    ALOGV("[%s]adev->ms12.is_bypass_ms12 %d adev->ms12.is_dolby_atmos %d adev->atmos_lock_flag %d\n",
            __func__,adev->ms12.is_bypass_ms12, adev->ms12.is_dolby_atmos, adev->atoms_lock_flag);
    ALOGV("[%s] frame_latency %d\n",__func__,frame_latency);

    if (frame_latency < 0) {
        *frames -= frame_latency;
    } else if (*frames >= (uint64_t)abs(frame_latency)) {
        *frames -= frame_latency;
    } else {
        *frames = 0;
    }
    if ((out->hal_rate != MM_FULL_POWER_SAMPLING_RATE) &&
        (!is_bypass_dolbyms12((struct audio_stream_out *)stream))) {
        *frames = (*frames * out->hal_rate) / MM_FULL_POWER_SAMPLING_RATE;
    }

    return 0;
}


uint32_t aml_audio_out_get_ms12_latency_frames(struct audio_stream_out *stream) {
    return out_get_ms12_latency_frames(stream, false);
}


