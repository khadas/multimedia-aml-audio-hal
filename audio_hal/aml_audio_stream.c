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
#include <string.h>
#include <tinyalsa/asoundlib.h>

#include "aml_alsa_mixer.h"
#include "aml_audio_stream.h"
#include "audio_hw_utils.h"
#include "dolby_lib_api.h"

#define min(a,b) (((a) < (b)) ? (a) : (b))


/*
 *@brief get sink capability
 */
static audio_format_t get_sink_capability (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    audio_format_t sink_capability = AUDIO_FORMAT_PCM_16_BIT;
    char *cap = (char *) get_hdmi_sink_cap(AUDIO_PARAMETER_STREAM_SUP_FORMATS, 0, &(adev->hdmi_descs));

    if (cap) {
#ifdef ENABLE_MAT_OUTPUT
        if (hdmi_desc->mat_fmt.is_support) {
            sink_capability = AUDIO_FORMAT_MAT;
        } else
#endif
        if (hdmi_desc->ddp_fmt.is_support) {
            sink_capability = AUDIO_FORMAT_E_AC3;
        } else if (hdmi_desc->dd_fmt.is_support) {
            sink_capability = AUDIO_FORMAT_AC3;
        }

        free(cap);
    }

    ALOGI ("%s dd support %d ddp support %d mat support \n", __FUNCTION__,
        hdmi_desc->dd_fmt.is_support,
        hdmi_desc->ddp_fmt.is_support,
        hdmi_desc->mat_fmt.is_support);

    return sink_capability;
}

bool is_sink_support_dolby_passthrough(audio_format_t sink_capability)
{
    return sink_capability == AUDIO_FORMAT_MAT ||
        sink_capability == AUDIO_FORMAT_E_AC3 ||
        sink_capability == AUDIO_FORMAT_AC3;
}

/*
 *@brief get sink format by logic min(source format / digital format / sink capability)
 * For Speaker/Headphone output, sink format keep PCM-16bits
 * For optical output, min(dd, source format, digital format)
 * For HDMI_ARC output
 *      1.digital format is PCM, sink format is PCM-16bits
 *      2.digital format is dd, sink format is min (source format,  AUDIO_FORMAT_AC3)
 *      3.digital format is auto, sink format is min (source format, digital format)
 */
void get_sink_format (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev;
    /*set default value for sink_audio_format/optical_audio_format*/
    audio_format_t sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
    audio_format_t optical_audio_format = AUDIO_FORMAT_PCM_16_BIT;
    audio_format_t sink_capability, source_format;
    int hdmi_format;

    if (!stream) {
        return;
    }

    adev = aml_out->dev;
    hdmi_format = adev->hdmi_format;
    sink_capability = get_sink_capability(stream);
    source_format = aml_out->hal_internal_format;

    if (aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        ALOGD("get_sink_format: a2dp set to pcm");
        adev->sink_format = AUDIO_FORMAT_PCM_16_BIT;
        adev->optical_format = AUDIO_FORMAT_PCM_16_BIT;
        aml_out->dual_output_flag = false;
        return;
    }

    /*when device is HDMI_ARC*/
    ALOGI("!!!%s() Sink devices %#x Source format %#x digital_format(hdmi_format) %#x Sink Capability %#x\n",
          __FUNCTION__, adev->active_outport, aml_out->hal_internal_format, adev->hdmi_format, sink_capability);

    if ((source_format != AUDIO_FORMAT_PCM_16_BIT) && \
        (source_format != AUDIO_FORMAT_AC3) && \
        (source_format != AUDIO_FORMAT_E_AC3) && \
        (source_format != AUDIO_FORMAT_MAT) && \
        (source_format != AUDIO_FORMAT_AC4) && \
        (source_format != AUDIO_FORMAT_DTS) &&
        (source_format != AUDIO_FORMAT_DTS_HD)) {
        /*unsupport format [dts-hd/true-hd]*/
        ALOGI("%s() source format %#x change to %#x", __FUNCTION__, source_format, AUDIO_FORMAT_PCM_16_BIT);
        source_format = AUDIO_FORMAT_PCM_16_BIT;
    }

#if 0
    // "adev->hdmi_format" is the UI selection item.
    // "adev->active_outport" was set when HDMI ARC cable plug in/off
    // condition 1: ARC port, single output.
    // condition 2: for BOX with continous mode, there are no speaker, only on HDMI outport, same use case
    // condition 3: for STB case
    if (adev->active_outport == OUTPORT_HDMI_ARC
         || ((eDolbyMS12Lib == adev->dolby_lib_type) && adev->continuous_audio_mode && !adev->is_TV)
         || adev->is_STB) {
        ALOGI("%s() HDMI ARC case", __FUNCTION__);
        switch (adev->hdmi_format) {
        case PCM:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        case DD:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = (source_format != AUDIO_FORMAT_DTS && source_format != AUDIO_FORMAT_DTS_HD) ? AUDIO_FORMAT_AC3 : AUDIO_FORMAT_DTS;
            break;
        case AUTO:
            sink_audio_format = (source_format != AUDIO_FORMAT_DTS && source_format != AUDIO_FORMAT_DTS_HD) ? min(source_format, sink_capability) : AUDIO_FORMAT_DTS;
            if ((source_format == AUDIO_FORMAT_PCM_16_BIT) && (adev->continuous_audio_mode == 1) && (sink_capability >= AUDIO_FORMAT_AC3)) {
                // For continous output, we need to continous output data
                // when input is PCM, we still need to output AC3/EAC3 according to sink capability
                sink_audio_format = sink_capability;
                ALOGI("%s continuous_audio_mode %d source_format %#x sink_capability %#x\n", __FUNCTION__, adev->continuous_audio_mode, source_format, sink_capability);
            }
            optical_audio_format = sink_audio_format;
            break;
        default:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        }
    }
    /*when device is SPEAKER/HEADPHONE*/
    else {
        ALOGI("%s() SPEAKER/HEADPHONE case", __FUNCTION__);
        if (!adev->is_STB && is_sink_support_dolby_passthrough(sink_capability))
            ALOGE("!!!%s() SPEAKER/HEADPHONE case, should no arc caps!!!", __FUNCTION__);

        switch (adev->hdmi_format) {
        case PCM:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        case DD:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = (source_format != AUDIO_FORMAT_DTS && source_format != AUDIO_FORMAT_DTS_HD) ? AUDIO_FORMAT_AC3 : AUDIO_FORMAT_DTS;
            break;
        case AUTO:
            if (adev->continuous_audio_mode == 0) {
                sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
                optical_audio_format = (source_format != AUDIO_FORMAT_DTS && source_format != AUDIO_FORMAT_DTS_HD)
                                       ? min(source_format, AUDIO_FORMAT_AC3)
                                       : AUDIO_FORMAT_DTS;
            } else {
                sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
                optical_audio_format = (source_format != AUDIO_FORMAT_DTS && source_format != AUDIO_FORMAT_DTS_HD) ? AUDIO_FORMAT_AC3 : AUDIO_FORMAT_DTS;
            }
            ALOGI("%s() source_format %#x sink_audio_format %#x "
                  "optical_audio_format %#x",
                  __FUNCTION__, source_format, sink_audio_format,
                  optical_audio_format);
            break;
        default:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        }
    }
#else

    if (eDolbyMS12Lib != adev->dolby_lib_type) {
        /* if MS12 library does not exist, then output format is same as BYPASS case */
        hdmi_format = BYPASS;
    }

    switch (hdmi_format) {
    case PCM:
        break;
    case DD:
        optical_audio_format = (source_format != AUDIO_FORMAT_DTS && source_format != AUDIO_FORMAT_DTS_HD) ? AUDIO_FORMAT_AC3 : AUDIO_FORMAT_DTS;
        break;
    case BYPASS:
        sink_audio_format = min(sink_capability,source_format);
        optical_audio_format = sink_audio_format;
        break;
    case AUTO:
    default:
        if ((source_format == AUDIO_FORMAT_DTS) || (source_format == AUDIO_FORMAT_DTS_HD)) {
            optical_audio_format = AUDIO_FORMAT_DTS;
        } else {
            optical_audio_format = sink_capability;
        }
        sink_audio_format = optical_audio_format;
        break;
    }
#endif

    adev->sink_format = sink_audio_format;
    adev->optical_format = optical_audio_format;

#if 1
    /* use single output for HDMI_ARC */
    if ((adev->active_outport == OUTPORT_HDMI_ARC) &&
        adev->bHDMIConnected)
        adev->sink_format = adev->optical_format;
#endif

    ALOGI("%s sink_format %#x optical_format %#x,soure fmt %#x stream device %d\n",
           __FUNCTION__, adev->sink_format, adev->optical_format, source_format, aml_out->device);
    return ;
}

int set_stream_dual_output(struct audio_stream_out *stream, bool en)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    aml_out->dual_output_flag = en;
    return 0;
}

int update_stream_dual_output(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    /* set the dual output format flag */
    if (adev->sink_format != adev->optical_format) {
        set_stream_dual_output(stream, true);
    } else {
        set_stream_dual_output(stream, false);
    }
    return 0;
}

bool is_dual_output_stream(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    return aml_out->dual_output_flag;
}

bool is_hdmi_in_stable_hw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    struct aml_audio_device *aml_dev = in->dev;
    int type = 0;
    int stable = 0;
    int txl_chip = is_txl_chip();
    hdmiin_audio_packet_t audio_packet = AUDIO_PACKET_NONE;


    stable = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_HDMI_IN_AUDIO_STABLE);
    if (!stable)
        return false;

    if (txl_chip) {
        audio_packet = get_hdmiin_audio_packet(&aml_dev->alsa_mixer);
        /*txl chip, we don't use hardware format detect for HBR*/
        if (audio_packet == AUDIO_PACKET_HBR) {
            return true;
        }
    }

    type = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIFIN_AUDIO_TYPE);
    if (type != in->spdif_fmt_hw) {
        ALOGV ("%s(), in type changed from %d to %d", __func__, in->spdif_fmt_hw, type);
        in->spdif_fmt_hw = type;
        return false;
    }

    return true;
}

bool is_hdmi_in_stable_sw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    struct aml_audio_device *aml_dev = in->dev;
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    audio_format_t fmt;

    /* now, only hdmiin->(spk, hp, arc) cases init the soft parser thread
     * TODO: init hdmiin->mix soft parser too
     */
    if (!patch)
        return true;

    fmt = audio_parse_get_audio_type (patch->audio_parse_para);
    if (fmt != in->spdif_fmt_sw) {
        ALOGV ("%s(), in type changed from %#x to %#x", __func__, in->spdif_fmt_sw, fmt);
        in->spdif_fmt_sw = fmt;
        return false;
    }

    return true;
}


void  release_audio_stream(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    if (aml_out->is_tv_platform == 1) {
        free(aml_out->tmp_buffer_8ch);
        aml_out->tmp_buffer_8ch = NULL;
        free(aml_out->audioeffect_tmp_buffer);
        aml_out->audioeffect_tmp_buffer = NULL;
    }
    free(stream);
}
bool is_atv_in_stable_hw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    struct aml_audio_device *aml_dev = in->dev;
    int type = 0;
    int stable = 0;

    stable = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_ATV_IN_AUDIO_STABLE);
    if (!stable)
        return false;

    return true;
}
bool is_av_in_stable_hw(struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *aml_dev = in->dev;
    int type = 0;
    int stable = 0;

    stable = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_AV_IN_AUDIO_STABLE);
    if (!stable)
        return false;

    return true;
}

bool is_spdif_in_stable_hw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    struct aml_audio_device *aml_dev = in->dev;
    int type = 0;

    type = aml_mixer_ctrl_get_int (&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIFIN_AUDIO_TYPE);
    if (type != in->spdif_fmt_hw) {
        ALOGV ("%s(), in type changed from %d to %d", __func__, in->spdif_fmt_hw, type);
        in->spdif_fmt_hw = type;
        return false;
    }

    return true;
}

int set_audio_source(struct aml_mixer_handle *mixer_handle,
        enum input_source audio_source, bool is_auge)
{
    int src;

    if (is_auge) {
        switch (audio_source) {
        case LINEIN:
            src = TDMIN_A;
            break;
        case ATV:
            src = FRATV;
            break;
        case HDMIIN:
            src = FRHDMIRX;
            break;
        case ARCIN:
            src = EARCRX_DMAC;
            break;
        case SPDIFIN:
            src = SPDIFIN_AUGE;
            break;
        default:
            src = FRHDMIRX;
            ALOGW("%s(),audio_source %d src: %d not support", __func__, audio_source, src);
            break;
        }
    } else {
        src = (int)audio_source;
    }

    return aml_mixer_ctrl_set_int(mixer_handle, AML_MIXER_ID_AUDIO_IN_SRC, src);
}

int set_resample_source(struct aml_mixer_handle *mixer_handle, enum ResampleSource source)
{
    return aml_mixer_ctrl_set_int(mixer_handle, AML_MIXER_ID_HW_RESAMPLE_SOURCE, source);
}

int set_spdifin_pao(struct aml_mixer_handle *mixer_handle,int enable)
{
    return aml_mixer_ctrl_set_int(mixer_handle, AML_MIXER_ID_SPDIFIN_PAO, enable);
}

eMixerHwResample get_spdifin_samplerate(struct aml_mixer_handle *mixer_handle)
{
    int index = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_SPDIF_IN_SAMPLERATE);

    if ((index > HW_RESAMPLE_DISABLE) && (index < HW_RESAMPLE_MAX)) {
        return index;
    }

    return HW_RESAMPLE_DISABLE;
}

static inline eMixerHwResample _sr_enum(int sr)
{
    switch (sr) {
        case 32000:
            return HW_RESAMPLE_32K;
        case 44100:
            return HW_RESAMPLE_44K;
        case 48000:
            return HW_RESAMPLE_48K;
        case 88200:
            return HW_RESAMPLE_88K;
        case 96000:
            return HW_RESAMPLE_96K;
        case 176400:
            return HW_RESAMPLE_176K;
        case 192000:
            return HW_RESAMPLE_192K;
        default:
            return HW_RESAMPLE_DISABLE;
    }
}

eMixerHwResample get_eArcIn_samplerate(struct aml_mixer_handle *mixer_handle)
{
    int sr = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_EARCRX_AUDIO_SAMPLERATE);

    /* eARC mix control does not return sample rate as enum type as HDMIIN and SPDIF */
    return _sr_enum(sr);
}

eMixerHwResample get_hdmiin_samplerate(struct aml_mixer_handle *mixer_handle)
{
    int stable = 0;
    int r;

    stable = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_HDMI_IN_AUDIO_STABLE);
    if (!stable) {
        return HW_RESAMPLE_DISABLE;
    }

    r = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_HDMI_IN_SAMPLERATE);
    if ((r > HW_RESAMPLE_DISABLE) && (r < HW_RESAMPLE_MAX)) {
        return r;
    }

    return HW_RESAMPLE_DISABLE;
}

int get_hdmiin_channel(struct aml_mixer_handle *mixer_handle)
{
    int stable = 0;
    int channel_index = 0;

    stable = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_HDMI_IN_AUDIO_STABLE);
    if (!stable) {
        return -1;
    }

    /* hmdirx audio support: N/A, 2, 3, 4, 5, 6, 7, 8 as enum */
    channel_index = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_HDMI_IN_CHANNELS);
    if ((channel_index > 0) && (channel_index <= 7))
        return channel_index + 1;

    return 2;
}

int get_arcin_channel(struct aml_mixer_handle *mixer_handle)
{
    if (aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_EARCRX_ATTENDED_TYPE) == ATTEND_TYPE_EARC) {
        if (aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_EARCRX_AUDIO_CODING_TYPE) == AUDIO_CODING_TYPE_MULTICH_8CH_LPCM) {
            return 8;
        }
    }

    return 2;
}

/* return audio channel assignment, see CEA-861-D Table 20 */
int get_arcin_ca(struct aml_mixer_handle *mixer_handle)
{
    if (aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_EARCRX_ATTENDED_TYPE) == ATTEND_TYPE_EARC) {
        return aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_EARCRX_CA);
    }

    return 0;
}

audio_channel_mask_t aml_map_ca_to_mask(int ca)
{
    audio_channel_mask_t mask = 0;

#if 0
    /* FLC/FRC and RLC/RRC are represented by AUDIO_CHANNEL_IN_LEFT_PROCESSED/AUDIO_CHANNEL_IN_RIGHT_PROCESSED */
    const audio_channel_mask_t mask_tab[] = {
        AUDIO_CHANNEL_IN_STEREO,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT | AUDIO_CHANNEL_IN_BACK,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT | AUDIO_CHANNEL_IN_LEFT_PROCESSED | AUDIO_CHANNEL_IN_RIGHT_PROCESSED,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_LEFT_PROCESSED | AUDIO_CHANNEL_IN_RIGHT_PROCESSED,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK | AUDIO_CHANNEL_IN_LEFT_PROCESSED | AUDIO_CHANNEL_IN_RIGHT_PROCESSED,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT | AUDIO_CHANNEL_IN_LEFT_PROCESSED | AUDIO_CHANNEL_IN_RIGHT_PROCESSED,
    };
#else
    const audio_channel_mask_t mask_tab[] = {
        AUDIO_CHANNEL_IN_STEREO,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT | AUDIO_CHANNEL_IN_BACK,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT | AUDIO_CHANNEL_IN_BACK_RIGHT | AUDIO_CHANNEL_IN_LEFT_PROCESSED | AUDIO_CHANNEL_IN_RIGHT_PROCESSED,
        AUDIO_CHANNEL_IN_STEREO,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK,
        AUDIO_CHANNEL_IN_STEREO | AUDIO_CHANNEL_IN_BACK_LEFT,
    };
#endif

    if ((ca >= 0) || (ca <= 0x1f)) {
        mask = AUDIO_CHANNEL_IN_STEREO;
        if (ca & 1)
            mask |= AUDIO_CHANNEL_IN_LOW_FREQUENCY;
        if ((ca >> 1) & 1)
            mask |= AUDIO_CHANNEL_IN_CENTER;
        mask |=  mask_tab[ca/4];
        return mask;
    }

    return 0;
}

int get_HW_resample(struct aml_mixer_handle *mixer_handle)
{
    return aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_HW_RESAMPLE_ENABLE);
}

int enable_HW_resample(struct aml_mixer_handle *mixer_handle, int enable_sr)
{
    if (enable_sr == 0)
        aml_mixer_ctrl_set_int(mixer_handle, AML_MIXER_ID_HW_RESAMPLE_ENABLE, HW_RESAMPLE_DISABLE);
    else
        aml_mixer_ctrl_set_int(mixer_handle, AML_MIXER_ID_HW_RESAMPLE_ENABLE, enable_sr);
    return 0;
}

bool Stop_watch(struct timespec start_ts, int64_t time) {
    struct timespec end_ts;
    int64_t start_ms, end_ms;
    int64_t interval_ms;

    clock_gettime (CLOCK_MONOTONIC, &end_ts);
    start_ms = start_ts.tv_sec * 1000LL +
               start_ts.tv_nsec / 1000000LL;
    end_ms = end_ts.tv_sec * 1000LL +
             end_ts.tv_nsec / 1000000LL;
    interval_ms = end_ms - start_ms;
    if (interval_ms < time) {
        return true;
    }
    return false;
}

bool signal_status_check(audio_devices_t in_device, int *mute_time,
                        struct audio_stream_in *stream) {
    if ((in_device & AUDIO_DEVICE_IN_HDMI) &&
            (!is_hdmi_in_stable_hw(stream) ||
            !is_hdmi_in_stable_sw(stream))) {
        *mute_time = 100;
        return false;
    }
    if ((in_device & AUDIO_DEVICE_IN_TV_TUNER) &&
            !is_atv_in_stable_hw (stream)) {
        *mute_time = 100;
        return false;
    }
    if (((in_device & AUDIO_DEVICE_IN_SPDIF) ||
            ((in_device & AUDIO_DEVICE_IN_HDMI_ARC) &&
                    (access(SYS_NODE_EARC_RX, F_OK) == -1))) &&
            !is_spdif_in_stable_hw(stream)) {
        *mute_time = 100;
        return false;
    }
    if ((in_device & AUDIO_DEVICE_IN_LINE) &&
            !is_av_in_stable_hw(stream)) {
       *mute_time = 100;
       return false;
    }
    return true;
}


hdmiin_audio_packet_t get_hdmiin_audio_packet(struct aml_mixer_handle *mixer_handle)
{
    int audio_packet = 0;
    audio_packet = aml_mixer_ctrl_get_int(mixer_handle,AML_MIXER_ID_HDMIIN_AUDIO_PACKET);
    if (audio_packet < 0) {
        return AUDIO_PACKET_NONE;
    }
    return (hdmiin_audio_packet_t)audio_packet;
}

unsigned int inport_to_device(enum IN_PORT inport)
{
    unsigned int device = 0;
    switch (inport) {
    case INPORT_TUNER:
        device = AUDIO_DEVICE_IN_TV_TUNER;
        break;
    case INPORT_HDMIIN:
        device = AUDIO_DEVICE_IN_AUX_DIGITAL;
        break;
    case INPORT_ARCIN:
        device = AUDIO_DEVICE_IN_HDMI_ARC;
        break;
    case INPORT_SPDIF:
        device = AUDIO_DEVICE_IN_SPDIF;
        break;
    case INPORT_LINEIN:
        device = AUDIO_DEVICE_IN_LINE;
        break;
    case INPORT_REMOTE_SUBMIXIN:
        device = AUDIO_DEVICE_IN_REMOTE_SUBMIX;
        break;
    case INPORT_WIRED_HEADSETIN:
        device = AUDIO_DEVICE_IN_WIRED_HEADSET;
        break;
    case INPORT_BUILTIN_MIC:
        device = AUDIO_DEVICE_IN_BUILTIN_MIC;
        break;
    case INPORT_BT_SCO_HEADSET_MIC:
        device = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
        break;
    default:
        ALOGE("%s(), unsupport %s", __func__, inport2String(inport));
        device = AUDIO_DEVICE_IN_BUILTIN_MIC;
        break;
    }

    return device;
}

void get_audio_indicator(struct aml_audio_device *dev, char *temp_buf) {
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;

    if (adev->update_type == TYPE_PCM)
        sprintf (temp_buf, "audioindicator=");
    else if (adev->update_type == TYPE_AC3)
        sprintf (temp_buf, "audioindicator=Dolby AC3");
    else if (adev->update_type == TYPE_EAC3)
        sprintf (temp_buf, "audioindicator=Dolby EAC3");
    else if (adev->update_type == TYPE_AC4)
        sprintf (temp_buf, "audioindicator=Dolby AC4");
    else if (adev->update_type == TYPE_MAT)
        sprintf (temp_buf, "audioindicator=Dolby MAT");
    else if (adev->update_type == TYPE_TRUE_HD)
        sprintf (temp_buf, "audioindicator=Dolby TrueHD");
    else if (adev->update_type == TYPE_DDP_ATMOS)
        sprintf (temp_buf, "audioindicator=Dolby EAC3,Dolby Atmos");
    else if (adev->update_type == TYPE_TRUE_HD_ATMOS)
        sprintf (temp_buf, "audioindicator=Dolby TrueHD,Dolby Atmos");
    else if (adev->update_type == TYPE_MAT_ATMOS)
        sprintf (temp_buf, "audioindicator=Dolby MAT,Dolby Atmos");
    else if (adev->update_type == TYPE_AC4_ATMOS)
        sprintf (temp_buf, "audioindicator=Dolby AC4,Dolby Atmos");
    else if (adev->update_type == TYPE_DTS)
        sprintf (temp_buf, "audioindicator=DTS");
    else if (adev->update_type == TYPE_DTS_HD_MA)
        sprintf (temp_buf, "audioindicator=DTS HD");
    ALOGI("%s(), [%s]", __func__, temp_buf);
}


bool is_dolby_ms12_support_compression_format(audio_format_t format)
{
    return (format == AUDIO_FORMAT_AC3 ||
            format == AUDIO_FORMAT_E_AC3 ||
            format == AUDIO_FORMAT_DOLBY_TRUEHD ||
            format == AUDIO_FORMAT_AC4 ||
            format == AUDIO_FORMAT_MAT);
}


void update_audio_format(struct aml_audio_device *adev, audio_format_t format)
{
    int atmos_flag = 0;
    int update_type = TYPE_PCM;
    bool is_dolby_active = dolby_stream_active(adev);
    bool is_dolby_format = is_dolby_ms12_support_compression_format(format);
    /*
     *for dolby & pcm case or dolby case
     *to update the dolby stream's format
     */
    if (is_dolby_active && is_dolby_format) {
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            atmos_flag = adev->ms12.is_dolby_atmos;
        } else {
            atmos_flag = adev->ddp.is_dolby_atmos;
        }

        if (adev->hal_internal_format != format ||
                atmos_flag != adev->is_dolby_atmos) {

            update_type = get_codec_type(format);

            if (atmos_flag == 1) {
                if (format == AUDIO_FORMAT_E_AC3)
                    update_type = TYPE_DDP_ATMOS;
                else if (format == AUDIO_FORMAT_DOLBY_TRUEHD)
                    update_type = TYPE_TRUE_HD_ATMOS;
                else if (format == AUDIO_FORMAT_MAT)
                    update_type = TYPE_MAT_ATMOS;
                else if (format == AUDIO_FORMAT_AC4)
                    update_type = TYPE_AC4_ATMOS;
            }

            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_AUDIO_HAL_FORMAT, update_type);

            ALOGD("%s()audio hal format change from %x to %x, atmos flag = %d, update_type = %d\n",
                __FUNCTION__, adev->hal_internal_format, format, atmos_flag, update_type);

            adev->hal_internal_format = format;
            adev->is_dolby_atmos = atmos_flag;
            adev->update_type = update_type;
        }
    }
    /*
     *to update the audio format for other cases
     *DTS-format / DTS format & Mixer-PCM
     *only Mixer-PCM
     */
    else if (!is_dolby_active && !is_dolby_format) {
        if (adev->hal_internal_format != format) {
            adev->hal_internal_format = format;
            adev->is_dolby_atmos = false;
            adev->update_type = get_codec_type(format);
            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_AUDIO_HAL_FORMAT, adev->update_type);
            ALOGD("%s()audio hal format change from %x to %x, atmos flag = %d, update_type = %d\n",
                __FUNCTION__, adev->hal_internal_format, format, atmos_flag, update_type);
        }
    }
    /*
     * **Dolby stream is active, and get the Mixer-PCM case steam format,
     * **we should ignore this Mixer-PCM update request.
     * else if (is_dolby_active && !is_dolby_format) {
     * }
     * **If Dolby steam is not active, the available format is LPCM or DTS
     * **The following case do not exit at all **
     * else //(!is_dolby_acrive && is_dolby_format) {
     * }
     */
}

bool is_direct_stream_and_pcm_format(struct aml_stream_out *out)
{
    return audio_is_linear_pcm(out->hal_internal_format) && (out->flags & AUDIO_OUTPUT_FLAG_DIRECT);
}
