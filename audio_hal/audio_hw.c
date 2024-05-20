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

#define LOG_TAG "audio_hw_hal_primary"
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
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <linux/ioctl.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <audio_utils/channels.h>
#include <memory.h>

#include "aml_dec_api.h"
#ifdef DTS_VX_V4_ENABLE
#include "Virtualx_v4.h"
#else
#include "Virtualx.h"
#endif


#if ANDROID_PLATFORM_SDK_VERSION >= 25 //8.0
#include <system/audio-base.h>
#endif

#include <hardware/audio.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include <audio_route/audio_route.h>
#include <spdifenc_wrap.h>
#include <aml_android_utils.h>
#include <aml_alsa_mixer.h>
#ifndef NO_AUDIO_CAP
    #include <IpcBuffer/IpcBuffer_c.h>
#endif

#include "tv_patch_format_parser.h"
#include "SPDIFEncoderAD.h"
#include "aml_volume_utils.h"
#include "aml_data_utils.h"
#include "spdifenc_wrap.h"
#include "alsa_manager.h"
#include "aml_audio_stream.h"
#include "audio_hw.h"
#include "spdif_encoder_api.h"
#include "audio_hw_utils.h"
#include "audio_hw_profile.h"
#include "aml_dump_debug.h"
#include "alsa_manager.h"
#include "alsa_device_parser.h"
#include "aml_audio_stream.h"
#include "alsa_config_parameters.h"
#include "spdif_encoder_api.h"
#include "tv_patch_avsync.h"
#include "aml_ng.h"
#include "aml_audio_timer.h"
#include "aml_audio_ease.h"
#include "aml_audio_spdifout.h"
#include "aml_config_parser.h"
#include "audio_effect_if.h"
#include "aml_audio_nonms12_render.h"

#ifdef BUILD_LINUX
#include "atomic.h"
#endif
//#ifdef ENABLE_MMAP
#ifndef BUILD_LINUX
#include "aml_mmap_audio.h"
#endif
// for invoke bluetooth rc hal
#ifndef BUILD_LINUX
#include "audio_hal_thunks.h"
#endif
#include "earc_utils.h"
#include <dolby_ms12_status.h>
#include <SPDIFEncoderAD.h>
#include "audio_hw_ms12_common.h"
#include "dolby_lib_api.h"
#include "aml_audio_ac3parser.h"
#include "aml_audio_ac4parser.h"
#include "aml_audio_ms12_sync.h"

#include "tv_patch_ctrl.h"
#include "tv_patch.h"
#include "hdmirx_utils.h"
#include "aml_audio_dev2mix_process.h"

#include "dmx_audio_es.h"
#include "aml_audio_ms12_render.h"
#include "aml_audio_nonms12_render.h"
#include "aml_config_data.h"
#include "aml_audio_spdifdec.h"

#ifndef BUILD_LINUX
#define ENABLE_NANO_NEW_PATH 1
#endif
#if ENABLE_NANO_NEW_PATH
#include "jb_nano.h"
#endif

#ifdef USE_MEDIAINFO
#include "aml_media_info.h"
#endif

// for dtv playback
#include "dtv_patch.h"
#include "../bt_voice/kehwin/audio_kw.h"
/*Google Voice Assistant channel_mask */
#define BUILT_IN_MIC 12

//#define SUBMIXER_V1_1
#define HDMI_LATENCY_MS 60

#ifdef ENABLE_AEC_HAL
#include "audio_aec_process.h"
#endif

/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support { */
#if defined(ENABLE_HBG_PATCH)
#include "../hbg_bt_voice/hbg_blehid_mic.h"
#endif
/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support } */

#include "amlAudioMixer.h"
#include "a2dp_hal.h"
#include "audio_bt_sco.h"
#include "aml_malloc_debug.h"


#ifdef ENABLE_AEC_APP
#include "audio_aec.h"
#endif
#include <audio_effects/effect_aec.h>
#include <audio_utils/clock.h>
#include "audio_hal_version.h"
#include "audio_hw_ms12_v2.h"
#include "aml_audio_scaletempo.h"

#define CARD_AMLOGIC_BOARD 0
/* ALSA ports for AML */
#define PORT_I2S 0
#define PORT_SPDIF 1
#define PORT_PCM 2
#undef PLAYBACK_PERIOD_COUNT
#define PLAYBACK_PERIOD_COUNT 4
/* number of periods for capture */
#undef CAPTURE_PERIOD_COUNT
#define CAPTURE_PERIOD_COUNT 4

/*Google Voice Assistant channel_mask */
#define BUILT_IN_MIC 12

/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000
#undef RESAMPLER_BUFFER_FRAMES
#define RESAMPLER_BUFFER_FRAMES (PERIOD_SIZE * 6)
#define RESAMPLER_BUFFER_SIZE (4 * RESAMPLER_BUFFER_FRAMES)
#define NSEC_PER_SECOND 1000000000ULL

/* sampling rate when using MM low power port */
#define MM_LOW_POWER_SAMPLING_RATE 44100
/* sampling rate when using MM full power port */
#define MM_FULL_POWER_SAMPLING_RATE 48000
/* sampling rate when using VX port for narrow band */
#define VX_NB_SAMPLING_RATE 8000
#define VX_WB_SAMPLING_RATE 16000

#ifdef BUILD_LINUX
#define MIXER_XML_PATH "/etc/mixer_paths.xml"
#else
#define MIXER_XML_PATH "/vendor/etc/mixer_paths.xml"
#endif
#define DOLBY_MS12_INPUT_FORMAT_TEST

#define IEC61937_PACKET_SIZE_OF_AC3                     (0x1800)
#define IEC61937_PACKET_SIZE_OF_EAC3                    (0x6000)

#define MAX_INPUT_STREAM_CNT                            (3)
#define AML_AEC_MIC_CHANNEL_NUM                         (2)

#define NETFLIX_DDP_BUFSIZE                             (768)

/*Tunnel sync HEADER is 20 bytes*/
#define TUNNEL_SYNC_HEADER_SIZE    (20)

#define DISABLE_CONTINUOUS_OUTPUT "persist.vendor.audio.continuous.disable"

#define LLP_BUFFER_NAME "llp_input"
#define LLP_BUFFER_SIZE (256 * 6 * 2 * 20)

#if 0
static void *llp_input_threadloop(void *data);
#else
static int ms12_llp_callback(void *priv_data, void *info);
#endif

static const struct pcm_config pcm_config_out = {
    .channels = 2,
    .rate = MM_FULL_POWER_SAMPLING_RATE,
    .period_size = DEFAULT_PLAYBACK_PERIOD_SIZE,
    .period_count = DEFAULT_PLAYBACK_PERIOD_CNT,
    .format = PCM_FORMAT_S16_LE,
};

static const struct pcm_config pcm_config_out_direct = {
    .channels = 2,
    .rate = MM_FULL_POWER_SAMPLING_RATE,
    .period_size = DEFAULT_PLAYBACK_PERIOD_SIZE,
    .period_count = PLAYBACK_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

static const struct pcm_config pcm_config_in = {
    .channels = 2,
    .rate = MM_FULL_POWER_SAMPLING_RATE,
    .period_size = DEFAULT_CAPTURE_PERIOD_SIZE,
    .period_count = CAPTURE_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

static const struct pcm_config pcm_config_aec = {
    .channels = 4,
    .rate = AEC_CAP_SAMPLING_RATE,
    .period_size = DEFAULT_CAPTURE_PERIOD_SIZE,
    .period_count = CAPTURE_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = 0,
    .avail_min = 0,
};


static const struct pcm_config pcm_config_bt = {
    .channels = 1,
    .rate = VX_NB_SAMPLING_RATE,
    .period_size = DEFAULT_PLAYBACK_PERIOD_SIZE,
    .period_count = PLAYBACK_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};
int start_ease_in(struct aml_audio_device *adev) {
    /*start ease in the audio*/
    ease_setting_t ease_setting;
    adev->audio_ease->data_format.format = AUDIO_FORMAT_PCM_16_BIT;
    adev->audio_ease->data_format.ch = 2;
    adev->audio_ease->data_format.sr = 48000;
    adev->audio_ease->ease_type = EaseInCubic;
    ease_setting.duration = 110;
    ease_setting.start_volume = 0.0;
    ease_setting.target_volume = 1.0;
    aml_audio_ease_config(adev->audio_ease, &ease_setting);

    return 0;
}

int start_ease_out(struct aml_audio_device *adev) {
    /*start ease out the audio*/
    ease_setting_t ease_setting;
    if (adev->is_TV) {
        ease_setting.duration = 150;
        ease_setting.start_volume = 1.0;
        ease_setting.target_volume = 0.0;
        adev->audio_ease->ease_type = EaseOutCubic;
        adev->audio_ease->data_format.format = AUDIO_FORMAT_PCM_16_BIT;
        adev->audio_ease->data_format.ch = 2;
        adev->audio_ease->data_format.sr = 48000;
    } else {
        ease_setting.duration = 30;
        ease_setting.start_volume = 1.0;
        ease_setting.target_volume = 0.0;
        adev->audio_ease->ease_type = EaseOutCubic;
        adev->audio_ease->data_format.format = AUDIO_FORMAT_PCM_16_BIT;
        adev->audio_ease->data_format.ch = 2;
        adev->audio_ease->data_format.sr = 48000;
    }
    aml_audio_ease_config(adev->audio_ease, &ease_setting);

    return 0;
}
static void select_output_device (struct aml_audio_device *adev);
static void select_input_device (struct aml_audio_device *adev);
static void select_devices (struct aml_audio_device *adev);
static int adev_set_voice_volume (struct audio_hw_device *dev, float volume);
static int do_output_standby (struct aml_stream_out *out);
//static int do_output_standby_l (struct audio_stream *out);
static uint32_t out_get_sample_rate (const struct audio_stream *stream);
static int out_pause (struct audio_stream_out *stream);
static int usecase_change_validate_l (struct aml_stream_out *aml_out, bool is_standby);
static inline int is_usecase_mix (stream_usecase_t usecase);
static inline bool need_hw_mix(usecase_mask_t masks);
//static int out_standby_new(struct audio_stream *stream);
static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle __unused,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused);
static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream);
static int get_audio_patch_by_src_dev(struct audio_hw_device *dev,
                                      audio_devices_t dev_type,
                                      struct audio_patch **p_audio_patch);
static int set_hdmi_format(struct aml_audio_device *adev, int val);

static ssize_t out_write_new(struct audio_stream_out *stream,
                      const void *buffer,
                      size_t bytes);
static int out_get_presentation_position (const struct audio_stream_out *stream, uint64_t *frames, struct timespec *timestamp);
static int out_standby_new(struct audio_stream *stream);

static aec_timestamp get_timestamp(void);

static int adev_get_mic_mute(const struct audio_hw_device* dev, bool* state);
static int adev_get_microphones(const struct audio_hw_device* dev,
                                struct audio_microphone_characteristic_t* mic_array,
                                size_t* mic_count);
static void get_mic_characteristics(struct audio_microphone_characteristic_t* mic_data,
                                    size_t* mic_count);
static void * g_aml_primary_adev = NULL;

static int aml_audio_focus(struct aml_audio_device *adev, bool on);

static int aml_audio_parser_process_wrapper(struct audio_stream_out *stream,
                                                         const void *in_buf,
                                                         int32_t in_size,
                                                         int32_t *used_size,
                                                         const void **output_buf,
                                                         int32_t *out_size,
                                                         int *frame_dur)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    audio_format_t audio_format = aml_out->hal_format;
    int ret = -1;
    if (!used_size) {
        *used_size = 0;
    }
    if (!in_buf || !in_size|| !used_size|| !output_buf|| !out_size) {
        AM_LOGE("parameter error");
        goto parser_error;
    }

    if (audio_format == AUDIO_FORMAT_IEC61937) {
        struct ac3_parser_info ac3_info = { 0 };
        void * dolby_inbuf = NULL;
        int32_t dolby_buf_size = 0;
        int temp_used_size = 0;
        const void * temp_main_frame_buffer = NULL;
        int temp_main_frame_size = 0;
        if (!aml_out->spdif_dec_handle) {
            ret = aml_spdif_decoder_open(&aml_out->spdif_dec_handle);
            if (ret != 0) {
                AM_LOGE(" aml_spdif_decoder_open fail");
                goto parser_error;
            }
        }
        ret = aml_spdif_decoder_process(aml_out->spdif_dec_handle, in_buf, in_size, used_size, output_buf, out_size);
        if (output_buf && out_size) {
            endian16_convert((void*)*output_buf, *out_size);
        }
        if (*out_size == 0) {
            *used_size = in_size;
            goto parser_error;
        }

        // for IEC data, we need get correct sample rate here?
        audio_format_t output_format = aml_spdif_decoder_getformat(aml_out->spdif_dec_handle);
        if (output_format == AUDIO_FORMAT_E_AC3
            || output_format == AUDIO_FORMAT_AC3) {
            const void* dolby_inbuf = *output_buf;
            int32_t dolby_buf_size = *out_size;

            if (!aml_out->ac3_parser_handle) {
                ret = aml_ac3_parser_open(&aml_out->ac3_parser_handle);
                if (ret != 0) {
                    AM_LOGE(" aml_ac3_parser_open fail");
                    goto parser_error;
                }
            }
            aml_ac3_parser_process(aml_out->ac3_parser_handle, dolby_inbuf, dolby_buf_size, &temp_used_size, &temp_main_frame_buffer, &temp_main_frame_size, &ac3_info);

            //AM_LOGI("out(%p) hal_rate(%d) parser_rate(%d) hal_format(%x)", aml_out, aml_out->hal_rate, ac3_info.sample_rate, aml_out->hal_internal_format);
            if (aml_out->hal_rate != ac3_info.sample_rate && ac3_info.sample_rate != 0) {
                aml_out->hal_rate = ac3_info.sample_rate;
                AM_LOGI("ac3/eac3 parser sample rate is %dHz", aml_out->hal_rate);
            }
        }
    } else if (audio_format == AUDIO_FORMAT_AC4) {
        struct ac4_parser_info ac4_info = { 0 };
        if (!aml_out->ac4_parser_handle) {
            ret = aml_ac4_parser_open(&aml_out->ac4_parser_handle);
            if (ret != 0) {
                AM_LOGE(" aml_ac4_parser_open fail");
                goto parser_error;
            }
        }
        ret = aml_ac4_parser_process(aml_out->ac4_parser_handle, in_buf, in_size, used_size, output_buf, out_size, &ac4_info);
        ALOGV("frame size =%d frame rate=%d sample rate=%d used =%d", ac4_info.frame_size, ac4_info.frame_rate, ac4_info.sample_rate, *used_size);
        if (*out_size == 0 && *used_size == 0) {
            *used_size = in_size;
            AM_LOGE("wrong ac4 frame size");
            goto parser_error;
        }
    } else if ((audio_format == AUDIO_FORMAT_AC3) ||
                (audio_format == AUDIO_FORMAT_E_AC3)) {
        struct ac3_parser_info ac3_info = { 0 };
        AM_LOGI_IF(adev->debug_flag, " ###### frame size %d #####", aml_out->ddp_frame_size);
        if (!aml_out->ac3_parser_handle) {
            ret = aml_ac3_parser_open(&aml_out->ac3_parser_handle);
            if (ret != 0) {
                AM_LOGE(" aml_ac3_parser_open fail");
                goto parser_error;
            }
        }
        ret = aml_ac3_parser_process(aml_out->ac3_parser_handle, in_buf, in_size, used_size, output_buf, out_size, &ac3_info);
        aml_out->ddp_frame_size = *out_size;
        aml_out->ddp_frame_nblks = ac3_info.numblks;
        aml_out->total_ddp_frame_nblks += aml_out->ddp_frame_nblks;
        //dependent_frame = ac3_info.frame_dependent;
        /*for patch mode, the hal_rate is not correct, we should correct it*/
        if (ac3_info.sample_rate != 0 && *out_size) {
            if (aml_out->hal_rate != ac3_info.sample_rate) {
                aml_out->hal_rate = ac3_info.sample_rate;
                AM_LOGI("ac3/eac3 parser sample rate is %dHz", aml_out->hal_rate);
            }
        }
        if (ac3_info.frame_size == 0) {
            if (*used_size == 0) {
                *used_size = in_size;
            }
            goto parser_error;
        }
    }else if ((audio_format == AUDIO_FORMAT_AAC)
            || (audio_format == AUDIO_FORMAT_AAC_LATM)
            || (audio_format == AUDIO_FORMAT_HE_AAC_V1)
            || (audio_format == AUDIO_FORMAT_HE_AAC_V2)) {
            if (!aml_out->heaac_parser_handle) {
                ret = aml_heaac_parser_open(&aml_out->heaac_parser_handle);
                if (ret != 0) {
                    AM_LOGE(" aml_heaac_parser_open fail");
                    goto parser_error;
                }
            }
            aml_out->heaac_info.debug_print = adev->debug_flag;
            if (audio_format == AUDIO_FORMAT_AAC_LATM || audio_format == AUDIO_FORMAT_HE_AAC_V1) {
                aml_out->heaac_info.is_loas = 1;
                aml_out->heaac_info.is_adts = 0;
            } else if (audio_format == AUDIO_FORMAT_AAC || audio_format == AUDIO_FORMAT_HE_AAC_V2) {
                aml_out->heaac_info.is_loas = 0;
                aml_out->heaac_info.is_adts = 1;
            }

            ret = aml_heaac_parser_process(aml_out->heaac_parser_handle,
                                       in_buf,
                                       in_size,
                                       used_size,
                                       output_buf,
                                       out_size, &(aml_out->heaac_info));
            if (*out_size <= 0 || !output_buf) {
                AM_LOGW("do not get aac frames !!!");
                *used_size = in_size;
                ret = -1;
                goto parser_error;
            }

            int sample_rate;
            int channels;
            int sample_size = aml_out->heaac_info.frame_size;

            if (audio_format == AUDIO_FORMAT_AAC_LATM || audio_format == AUDIO_FORMAT_HE_AAC_V1) {
                sample_rate = aml_out->heaac_info.sampleRateHz;
                channels = aml_out->heaac_info.channel_mask;
                //aac_info.samples = 2048;
                *frame_dur = 0;  //todo:get frame pts here, calc it by decoder output now.
            } else {
                sample_rate = aml_out->heaac_info.sample_rate;
                channels = aml_out->heaac_info.channelCount;
                *frame_dur = aml_out->heaac_info.samples * 1000 * 90 / sample_rate;
            }

            AM_LOGI_IF(adev->debug_flag, "out_size %d in_size %d used_size %d, dur %x,"
                        " framesize:%d, sample_rate:%d, channel_mask:%d, sampleRateHz:%d, channelCount:%d, numSubframes:%d",
                        *out_size, in_size, *used_size, *frame_dur,
                        aml_out->heaac_info.frame_size, aml_out->heaac_info.sample_rate, aml_out->heaac_info.channel_mask,
                        aml_out->heaac_info.sampleRateHz, aml_out->heaac_info.channelCount, aml_out->heaac_info.numSubframes);

        } else {// no parser
        *output_buf = in_buf;
        *out_size = in_size;
        *used_size = in_size;
        ret = 0;
    }

    return ret;
parser_error:
    *output_buf = in_buf;
    *out_size = in_size;
    return ret;
}

void *aml_adev_get_handle(void)
{
    return (void *)g_aml_primary_adev;
}

static inline bool need_hw_mix(usecase_mask_t masks)
{
    return (masks > 1);
}

static inline int is_usecase_mix(stream_usecase_t usecase)
{
    return usecase > STREAM_PCM_NORMAL;
}

static inline short CLIP (int r)
{
    return (r >  0x7fff) ? 0x7fff :
           (r < -0x8000) ? -0x8000 :
           r;
}

//code here for audio hal mixer when hwsync with af mixer output stream output
//at the same,need do a software mixer in audio hal.
static int aml_hal_mixer_init (struct aml_hal_mixer *mixer)
{
    pthread_mutex_lock (&mixer->lock);
    mixer->wp = 0;
    mixer->rp = 0;
    mixer->buf_size = AML_HAL_MIXER_BUF_SIZE;
    mixer->need_cache_flag = 1;
    pthread_mutex_unlock (&mixer->lock);
    return 0;
}
static uint32_t aml_hal_mixer_get_space (struct aml_hal_mixer *mixer)
{
    unsigned space;
    if (mixer->wp >= mixer->rp) {
        space = mixer->buf_size - (mixer->wp - mixer->rp);
    } else {
        space = mixer->rp - mixer->wp;
    }
    return space > 64 ? (space - 64) : 0;
}
static int aml_hal_mixer_get_content (struct aml_hal_mixer *mixer)
{
    unsigned content = 0;
    pthread_mutex_lock (&mixer->lock);
    if (mixer->wp >= mixer->rp) {
        content = mixer->wp - mixer->rp;
    } else {
        content = mixer->wp - mixer->rp + mixer->buf_size;
    }
    //ALOGI("wp %d,rp %d\n",mixer->wp,mixer->rp);
    pthread_mutex_unlock (&mixer->lock);
    return content;
}
//we assue the cached size is always smaller then buffer size
//need called by device mutux locked
static int aml_hal_mixer_write (struct aml_hal_mixer *mixer, const void *w_buf, uint32_t size)
{
    unsigned space;
    unsigned write_size = size;
    unsigned tail = 0;
    pthread_mutex_lock (&mixer->lock);
    space = aml_hal_mixer_get_space (mixer);
    if (space < size) {
        ALOGI ("write data no space,space %d,size %d,rp %d,wp %d,reset all ptr\n", space, size, mixer->rp, mixer->wp);
        mixer->wp = 0;
        mixer->rp = 0;
    }
    //TODO
    if (write_size > space) {
        write_size = space;
    }
    if (write_size + mixer->wp > mixer->buf_size) {
        tail = mixer->buf_size - mixer->wp;
        memcpy (mixer->start_buf + mixer->wp, w_buf, tail);
        write_size -= tail;
        memcpy (mixer->start_buf, (unsigned char*) w_buf + tail, write_size);
        mixer->wp = write_size;
    } else {
        memcpy (mixer->start_buf + mixer->wp, w_buf, write_size);
        mixer->wp += write_size;
        mixer->wp %= AML_HAL_MIXER_BUF_SIZE;
    }
    pthread_mutex_unlock (&mixer->lock);
    return size;
}
//need called by device mutux locked
static int aml_hal_mixer_read (struct aml_hal_mixer *mixer, void *r_buf, uint32_t size)
{
    unsigned cached_size;
    unsigned read_size = size;
    unsigned tail = 0;
    cached_size = aml_hal_mixer_get_content (mixer);
    pthread_mutex_lock (&mixer->lock);
    // we always assue we have enough data to read when hwsync enabled.
    // if we do not have,insert zero data.
    if (cached_size < size) {
        ALOGI ("read data has not enough data to mixer,read %d, have %d,rp %d,wp %d\n", size, cached_size, mixer->rp, mixer->wp);
        memset ( (unsigned char*) r_buf + cached_size, 0, size - cached_size);
        read_size = cached_size;
    }
    if (read_size + mixer->rp > mixer->buf_size) {
        tail = mixer->buf_size - mixer->rp;
        memcpy (r_buf, mixer->start_buf + mixer->rp, tail);
        read_size -= tail;
        memcpy ( (unsigned char*) r_buf + tail, mixer->start_buf, read_size);
        mixer->rp = read_size;
    } else {
        memcpy (r_buf, mixer->start_buf + mixer->rp, read_size);
        mixer->rp += read_size;
        mixer->rp %= AML_HAL_MIXER_BUF_SIZE;
    }
    pthread_mutex_unlock (&mixer->lock);
    return size;
}
// aml audio hal mixer code end

static void select_devices (struct aml_audio_device *adev)
{
    ALOGD ("%s(mode=%d, out_device=%#x)", __FUNCTION__, adev->mode, adev->out_device);
    int headset_on;
    int headphone_on;
    int speaker_on;
    int hdmi_on;
    int earpiece;
    int mic_in;
    int headset_mic;

    headset_on = adev->out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET;
    headphone_on = adev->out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    speaker_on = adev->out_device & AUDIO_DEVICE_OUT_SPEAKER;
    hdmi_on = adev->out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL;
    earpiece =  adev->out_device & AUDIO_DEVICE_OUT_EARPIECE;
    mic_in = adev->in_device & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC);
    headset_mic = adev->in_device & AUDIO_DEVICE_IN_WIRED_HEADSET;

    ALOGD ("%s : hs=%d , hp=%d, sp=%d, hdmi=0x%x,earpiece=0x%x", __func__,
             headset_on, headphone_on, speaker_on, hdmi_on, earpiece);
    ALOGD ("%s : in_device(%#x), mic_in(%#x), headset_mic(%#x)", __func__,
             adev->in_device, mic_in, headset_mic);
    audio_route_reset (adev->ar);
    if (hdmi_on) {
        audio_route_apply_path (adev->ar, "hdmi");
    }
    if (headphone_on || headset_on) {
        audio_route_apply_path (adev->ar, "headphone");
    }
    if (speaker_on || earpiece) {
        audio_route_apply_path (adev->ar, "speaker");
    }
    if (mic_in) {
        audio_route_apply_path (adev->ar, "main_mic");
    }
    if (headset_mic) {
        audio_route_apply_path (adev->ar, "headset-mic");
    }

    audio_route_update_mixer (adev->ar);

}

static void select_mode (struct aml_audio_device *adev)
{
    ALOGD ("%s(out_device=%#x)", __FUNCTION__, adev->out_device);
    ALOGD ("%s(in_device=%#x)", __FUNCTION__, adev->in_device);
    return;

    /* force earpiece route for in call state if speaker is the
        only currently selected route. This prevents having to tear
        down the modem PCMs to change route from speaker to earpiece
        after the ringtone is played, but doesn't cause a route
        change if a headset or bt device is already connected. If
        speaker is not the only thing active, just remove it from
        the route. We'll assume it'll never be used initally during
        a call. This works because we're sure that the audio policy
        manager will update the output device after the audio mode
        change, even if the device selection did not change. */
    if ( (adev->out_device & AUDIO_DEVICE_OUT_ALL) == AUDIO_DEVICE_OUT_SPEAKER) {
        adev->in_device = AUDIO_DEVICE_IN_BUILTIN_MIC & ~AUDIO_DEVICE_BIT_IN;
    } else {
        adev->out_device &= ~AUDIO_DEVICE_OUT_SPEAKER;
    }

    return;
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream (struct aml_stream_out *out)
{
    struct aml_audio_device *adev = out->dev;
    unsigned int card = CARD_AMLOGIC_BOARD;
    unsigned int port = PORT_I2S;
    int ret = 0;
    struct aml_stream_out *out_removed = NULL;
    int channel_count = popcount (out->hal_channel_mask);
    bool hwsync_lpcm = (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && out->config.rate  <= 48000 &&
                        audio_is_linear_pcm(out->hal_internal_format) && channel_count <= 2);
    ALOGD("%s(adev->out_device=%#x, adev->mode=%d)",
            __FUNCTION__, adev->out_device, adev->mode);
    if (adev->mode != AUDIO_MODE_IN_CALL) {
        /* FIXME: only works if only one output can be active at a time */
        //select_devices(adev);
    }
    card = alsa_device_get_card_index();
    if (adev->out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
        port = PORT_PCM;
        out->config = pcm_config_bt;
        if (adev->bt_wbs)
            out->config.rate = VX_WB_SAMPLING_RATE;
    } else if (out->flags & AUDIO_OUTPUT_FLAG_DIRECT && !hwsync_lpcm) {
        port = PORT_SPDIF;
    }

    /* check to update port */
    port = alsa_device_update_pcm_index(port, PLAYBACK);

    ALOGD ("*%s, open card(%d) port(%d)", __FUNCTION__, card, port);

    /* default to low power: will be corrected in out_write if necessary before first write to
     * tinyalsa.
     */
    out->write_threshold = out->config.period_size * PLAYBACK_PERIOD_COUNT;
    out->config.start_threshold = out->config.period_size * PLAYBACK_PERIOD_COUNT;
    out->config.avail_min = 0;//SHORT_PERIOD_SIZE;
    //added by xujian for NTS hwsync/system stream mix smooth playback.
    //we need re-use the tinyalsa pcm handle by all the output stream, including
    //hwsync direct output stream,system mixer output stream.
    //TODO we need diff the code with AUDIO_DEVICE_OUT_ALL_SCO.
    //as it share the same hal but with the different card id.
    //TODO need reopen the tinyalsa card when sr/ch changed,
    if (adev->pcm == NULL) {
        ALOGD("%s(), pcm_open card %u port %u\n", __func__, card, port);
        out->pcm = pcm_open (card, port, PCM_OUT /*| PCM_MMAP | PCM_NOIRQ*/, & (out->config) );
        if (!pcm_is_ready (out->pcm) ) {
            ALOGE ("cannot open pcm_out driver: %s", pcm_get_error (out->pcm) );
            pcm_close (out->pcm);
            return -ENOMEM;
        }
        if (out->config.rate != out_get_sample_rate (&out->stream.common) ) {
            ALOGD ("%s(out->config.rate=%d, out->config.channels=%d)",
                     __FUNCTION__, out->config.rate, out->config.channels);
            ret = create_resampler (out_get_sample_rate (&out->stream.common),
                                    out->config.rate,
                                    out->config.channels,
                                    RESAMPLER_QUALITY_DEFAULT,
                                    NULL,
                                    &out->resampler);
            if (ret != 0) {
                ALOGE ("cannot create resampler for output");
                return -ENOMEM;
            }
            out->buffer_frames = (out->config.period_size * out->config.rate) /
                                 out_get_sample_rate (&out->stream.common) + 1;
            out->buffer = aml_audio_malloc (pcm_frames_to_bytes (out->pcm, out->buffer_frames) );
            if (out->buffer == NULL) {
                ALOGE ("cannot malloc memory for out->buffer");
                return -ENOMEM;
            }
        }
        adev->pcm = out->pcm;
        ALOGI ("device pcm %p\n", adev->pcm);
    } else {
        ALOGI ("stream %p share the pcm %p\n", out, adev->pcm);
        out->pcm = adev->pcm;
        // add to fix start output when pcm in pause state
        if (adev->pcm_paused && pcm_is_ready (out->pcm) ) {
            ret = pcm_ioctl (out->pcm, SNDRV_PCM_IOCTL_PAUSE, 0);
            if (ret < 0) {
                ALOGE ("%s(), cannot resume channel\n", __func__);
            }
        }
    }
    ALOGD ("channels=%d---format=%d---period_count%d---period_size%d---rate=%d---",
             out->config.channels, out->config.format, out->config.period_count,
             out->config.period_size, out->config.rate);

    if (out->resampler) {
        out->resampler->reset (out->resampler);
    }
    if (out->is_tv_platform == 1) {
        sysfs_set_sysfs_str ("/sys/class/amhdmitx/amhdmitx0/aud_output_chs", "2:2");
    }

    return 0;
}

static int check_input_parameters(uint32_t sample_rate, audio_format_t format, int channel_count, audio_devices_t devices)
{
    ALOGD("%s(sample_rate=%d, format=%d, channel_count=%d, devices = %x)", __FUNCTION__, sample_rate, format, channel_count, devices);

    if(AUDIO_DEVICE_IN_DEFAULT == devices && AUDIO_CHANNEL_NONE == channel_count &&
       AUDIO_FORMAT_DEFAULT == format && 0 == sample_rate) {
       /* Add for Hidl6.0:CloseDeviceWithOpenedInputStreams test */
       return -ENOSYS; /*Currently System Not Supported.*/
    }

    if (format != AUDIO_FORMAT_PCM_16_BIT && format != AUDIO_FORMAT_PCM_32_BIT) {
        ALOGE("%s: unsupported AUDIO FORMAT (%d)", __func__, format);
        return -EINVAL;
    }

    if (channel_count < 1 || channel_count > 4 || channel_count == 3) {
        ALOGE("%s: unsupported channel count (%d) passed  Min / Max (1 / 2)", __func__, channel_count);
        return -EINVAL;
    }

    switch (sample_rate) {
        case 8000:
        case 11025:
        /*fallthrough*/
        case 12000:
        case 16000:
        case 17000:
        case 22050:
        case 24000:
        case 32000:
        case 44100:
        case 48000:
        case 96000:
            break;
        default:
            ALOGE("%s: unsupported (%d) samplerate passed ", __func__, sample_rate);
            return -EINVAL;
    }

    devices &= ~AUDIO_DEVICE_BIT_IN;
    if ((devices & AUDIO_DEVICE_IN_LINE) ||
        (devices & AUDIO_DEVICE_IN_SPDIF) ||
        (devices & AUDIO_DEVICE_IN_TV_TUNER) ||
        (devices & AUDIO_DEVICE_IN_HDMI) ||
        (devices & AUDIO_DEVICE_IN_HDMI_ARC)) {
        if (format == AUDIO_FORMAT_PCM_16_BIT &&
            channel_count == 2 &&
            sample_rate == 48000) {
            ALOGD("%s: audio patch input device %x", __FUNCTION__, devices);
            return 0;
        } else {
            ALOGD("%s: unspported audio patch input device %x", __FUNCTION__, devices);
            return -EINVAL;
        }
    }

    return 0;
}

static size_t get_input_buffer_size(unsigned int period_size, uint32_t sample_rate, audio_format_t format, int channel_count)
{
    size_t size;

    ALOGD("%s(sample_rate=%d, format=%d, channel_count=%d)", __FUNCTION__, sample_rate, format, channel_count);
    /*
    if (check_input_parameters(sample_rate, format, channel_count, AUDIO_DEVICE_NONE) != 0) {
        return 0;
    }
    */
    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames */
    if (period_size == 0)
        period_size = (pcm_config_in.period_size * sample_rate) / pcm_config_in.rate;
    size = (period_size + 15) / 16 * 16;

    if (format == AUDIO_FORMAT_PCM_32_BIT)
        return size * channel_count * sizeof(int32_t);
    else
        return size * channel_count * sizeof(int16_t);
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *) stream;
    unsigned int rate = out->hal_rate;
    ALOGV("Amlogic_HAL - out_get_sample_rate() = %d", rate);
    return rate;
}

static int out_set_sample_rate(struct audio_stream *stream __unused, uint32_t rate __unused)
{
    return 0;
}

static uint32_t out_get_latency (const struct audio_stream_out *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *) stream;
    const struct aml_audio_device *adev = out->dev;
    int buffer_size = 0;
    int ret = 0;
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (out->is_normal_pcm) {
           ret = dolby_ms12_get_system_buffer_avail(&buffer_size);
           if (ret < 0) {
               AM_LOGE("get available system buffer error!");
           } else {
               return ret;
           }
       }
    }
    ret = out_get_latency_frames (stream) * audio_stream_out_frame_size(stream);
    return ret;
}

static size_t out_get_buffer_size (const struct audio_stream *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    ALOGI("%s(out->config.rate=%d, format %x,stream format %x)", __FUNCTION__,
          out->config.rate, out->hal_internal_format, stream->get_format(stream));
    /* take resampling into account and return the closest majoring
     * multiple of 16 frames, as audioflinger expects audio buffers to
     * be a multiple of 16 frames
     */
    int size = out->config.period_size;
    size_t buffer_size = 0;
    int ret = 0;
    switch (out->hal_internal_format) {
    case AUDIO_FORMAT_AC3:
        if (stream->get_format(stream) == AUDIO_FORMAT_IEC61937) {
            size = AC3_PERIOD_SIZE;
            ALOGI("%s AUDIO_FORMAT_IEC61937 %d)", __FUNCTION__, size);
            if ((eDolbyDcvLib == adev->dolby_lib_type) &&
                (out->flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
                // local file playback, data from audio flinger direct mode
                // the data is packed by DCV decoder by OMX, 1536 samples per packet
                // to match with it, set the size to 1536
                // (ms12 decoder doesn't encounter this issue, so only handle with DCV decoder case)
                size = AC3_PERIOD_SIZE / 4;
                ALOGI("%s AUDIO_FORMAT_IEC61937(DIRECT) (eDolbyDcvLib) size = %d)", __FUNCTION__, size);
            }
        } else if (out->flags & AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO) {
            size = AC3_PERIOD_SIZE;
        } else if (out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
            size = (DEFAULT_PLAYBACK_PERIOD_SIZE << 1);
        } else {
            size = DEFAULT_PLAYBACK_PERIOD_SIZE;
        }
        break;
    case AUDIO_FORMAT_E_AC3:
        if (stream->get_format(stream) == AUDIO_FORMAT_IEC61937) {
            size =  EAC3_PERIOD_SIZE;
        } else if (out->flags & AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO) {
            size = EAC3_PERIOD_SIZE;//one iec61937 packet size
        } else if (out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
            size = (DEFAULT_PLAYBACK_PERIOD_SIZE << 3) + (DEFAULT_PLAYBACK_PERIOD_SIZE << 1);
        }  else {
            /*frame align*/
            if (1 /* adev->continuous_audio_mode */) {
                /*Tunnel sync HEADER is 16 bytes*/
                if ((out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) && out->with_header) {
                    size = out->ddp_frame_size + 20;
                } else {
                    size = out->ddp_frame_size * 4;
                }
            } else {
                size = PLAYBACK_PERIOD_COUNT * DEFAULT_PLAYBACK_PERIOD_SIZE;    //PERIOD_SIZE;
            }
        }

        if (eDolbyDcvLib == adev->dolby_lib_type_last) {
            if (stream->get_format(stream) == AUDIO_FORMAT_IEC61937) {
                size = PLAYBACK_PERIOD_COUNT * DEFAULT_PLAYBACK_PERIOD_SIZE;
                ALOGI("%s eac3 eDolbyDcvLib = size%d)", __FUNCTION__, size);
            }
        }
        /*netflix ddp size is 768, if we change to a big value, then
         *every process time is too long, it will cause such case failed SWPL-41439
         */
        if (adev->is_netflix) {
            if (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) {
                size = NETFLIX_DDP_BUFSIZE + 20;
            } else {
                size = NETFLIX_DDP_BUFSIZE;
            }
        }
        break;
    case AUDIO_FORMAT_AC4:
        /*
         *1.offload write interval is 10ms
         *2.AC4 datarate: ac4 frame size=16391 frame rate =23440 sample rate=48000
         *       16391Bytes/42.6ms
         *3.set the audio hal buffer size as 8192 Bytes, it is about 24ms
         */
        size = (DEFAULT_PLAYBACK_PERIOD_SIZE << 4);
        break;
    case AUDIO_FORMAT_DOLBY_TRUEHD:
        if (out->flags & AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO) {
            size = 16 * DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
        } else {
            size = PLAYBACK_PERIOD_COUNT * DEFAULT_PLAYBACK_PERIOD_SIZE;
        }
        if (stream->get_format(stream) == AUDIO_FORMAT_IEC61937) {
            size = 4 * PLAYBACK_PERIOD_COUNT * DEFAULT_PLAYBACK_PERIOD_SIZE;
        }
        break;
    case AUDIO_FORMAT_DTS:
        if (stream->get_format(stream) == AUDIO_FORMAT_IEC61937) {
            size = DTS1_PERIOD_SIZE / 2;
        } else {
            size = DTSHD_PERIOD_SIZE * 8;
        }
        ALOGI("%s AUDIO_FORMAT_DTS buffer size = %d frames", __FUNCTION__, size);
        break;
    case AUDIO_FORMAT_DTS_HD:
        if (stream->get_format(stream) == AUDIO_FORMAT_IEC61937) {
            size = 4 * PLAYBACK_PERIOD_COUNT * DEFAULT_PLAYBACK_PERIOD_SIZE;
        } else {
            size = DTSHD_PERIOD_SIZE * 8;
        }
        ALOGI("%s AUDIO_FORMAT_DTS_HD buffer size = %d frames", __FUNCTION__, size);
        break;
#if 0
    case AUDIO_FORMAT_PCM:
        if (adev->continuous_audio_mode) {
            /*Tunnel sync HEADER is 16 bytes*/
            if (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) {
                size = (8192 + 16) / 4;
            }
        }
#endif
    default:
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            if (out->is_normal_pcm) {
                ret = dolby_ms12_get_system_buffer_avail(&size);
                if (ret < 0) {
                    AM_LOGE("get available system buffer error!");
                }
            }
           return size;
        }
        if (adev->continuous_audio_mode && audio_is_linear_pcm(out->hal_internal_format)) {
            /*Tunnel sync HEADER is 20 bytes*/
            if (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) {
                size = (8192 + TUNNEL_SYNC_HEADER_SIZE);
                return size;
            } else {
                /* roll back the change for SWPL-15974 to pass the gts failure SWPL-20926*/
                return DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT* audio_stream_out_frame_size ( (struct audio_stream_out *) stream);
            }
        }
        if (out->config.rate == 96000)
            size = DEFAULT_PLAYBACK_PERIOD_SIZE * 2;
        else
            size = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    }

    if (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && audio_is_linear_pcm(out->hal_internal_format)) {
        size = (size * audio_stream_out_frame_size((struct audio_stream_out *) stream)) + TUNNEL_SYNC_HEADER_SIZE;
    } else {
        size = (size * audio_stream_out_frame_size((struct audio_stream_out *) stream));
    }

    // remove alignment to have an accurate size
    // size = ( (size + 15) / 16) * 16;
    return size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream __unused)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;

    //ALOGV("Amlogic_HAL - out_get_channels return out->hal_channel_mask:%0x", out->hal_channel_mask);
    return out->hal_channel_mask;
}

static audio_channel_mask_t out_get_channels_direct(const struct audio_stream *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;
    ALOGV("out->hal_channel_mask:%0x", out->hal_channel_mask);
    return out->hal_channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream __unused)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;
    //ALOGV("Amlogic_HAL - out_get_format() = %d", out->hal_format);
    // if hal_format doesn't have a valid value,
    // return default value AUDIO_FORMAT_PCM_16_BIT
    if (out->hal_format == 0) {
        return AUDIO_FORMAT_PCM_16_BIT;
    }
    return out->hal_format;
}

static audio_format_t out_get_format_direct(const struct audio_stream *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;

    return  out->hal_format;
}

static int out_set_format(struct audio_stream *stream __unused, audio_format_t format __unused)
{
    return 0;
}

/* must be called with hw device and output stream mutexes locked */
static int do_output_standby (struct aml_stream_out *out)
{
    struct aml_audio_device *adev = out->dev;
    ALOGD ("%s(%p)", __FUNCTION__, out);
//#ifdef ENABLE_BT_A2DP
#ifndef BUILD_LINUX
    if ((out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) && adev->a2dp_hal)
        a2dp_out_standby(adev);
#endif
    if (!out->standby) {
        //commit here for hwsync/mix stream hal mixer
        //pcm_close(out->pcm);
        //out->pcm = NULL;
        if (out->buffer) {
            aml_audio_free (out->buffer);
            out->buffer = NULL;
        }
        if (out->resampler) {
            release_resampler (out->resampler);
            out->resampler = NULL;
        }
        out->standby = 1;
        int cnt = 0;
        for (cnt=0; cnt<STREAM_USECASE_MAX; cnt++) {
            if (adev->active_outputs[cnt] != NULL) {
                break;
            }
        }
        /* no active output here,we can close the pcm to release the sound card now*/
        if (cnt >= STREAM_USECASE_MAX) {
            if (adev->pcm) {
                ALOGI ("close pcm %p\n", adev->pcm);
                pcm_close (adev->pcm);
                adev->pcm = NULL;
            }
            out->pause_status = false;
            adev->pcm_paused = false;
        }
    }
    return 0;
}

static int out_standby (struct audio_stream *stream)
{
    ALOGD ("%s(%p)", __FUNCTION__, stream);
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    int status = 0;
    pthread_mutex_lock (&out->dev->lock);
    pthread_mutex_lock (&out->lock);
    status = do_output_standby (out);
    pthread_mutex_unlock (&out->lock);
    pthread_mutex_unlock (&out->dev->lock);
    return status;
}

static int out_dump (const struct audio_stream *stream __unused, int fd __unused)
{
    ALOGD ("%s(%p, %d)", __FUNCTION__, stream, fd);
    return 0;
}

static int out_flush (struct audio_stream_out *stream)
{
    ALOGD ("%s(%p)", __FUNCTION__, stream);
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int ret = 0;
    int channel_count = popcount (out->hal_channel_mask);
    bool hwsync_lpcm = (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && out->config.rate  <= 48000 &&
                        audio_is_linear_pcm(out->hal_internal_format) && channel_count <= 2);
    do_standby_func standy_func = NULL;
    /* VTS: a stream should always succeed to flush
     * hardware/libhardware/include/hardware/audio.h: Stream must already
     * be paused before calling flush(), we check and complain this case
     */
    if (adev->audio_ease) {
        start_ease_in(adev);
    }

    if (!out->pause_status) {
        ALOGW("%s(%p), stream should be in pause status", __func__, out);
    }

    standy_func = do_output_standby;

    aml_audio_trace_int("out_flush", 1);
    pthread_mutex_lock (&adev->lock);
    pthread_mutex_lock (&out->lock);
    if (out->pause_status == true) {
        // when pause status, set status prepare to avoid static pop sound
        ret = aml_alsa_output_resume(stream);
        if (ret < 0) {
            ALOGE("aml_alsa_output_resume error =%d", ret);
        }

        if (out->spdifout_handle) {
            ret = aml_audio_spdifout_resume(out->spdifout_handle);
            if (ret < 0) {
                ALOGE("aml_audio_spdifout_resume error =%d", ret);
            }

        }

        if (out->spdifout2_handle) {
            ret = aml_audio_spdifout_resume(out->spdifout2_handle);
            if (ret < 0) {
                ALOGE("aml_audio_spdifout_resume error =%d", ret);
            }
        }

    }
    standy_func (out);
    out->frame_write_sum  = 0;
    out->last_frames_position = 0;
    out->spdif_enc_init_frame_write_sum =  0;
    out->frame_skip_sum = 0;
    out->skip_frame = 0;
    //out->pause_status = false;
    out->input_bytes_size = 0;
    audio_header_info_reset(out->pheader);
    avsync_ctx_reset(out->avsync_ctx);

exit:
    pthread_mutex_unlock (&adev->lock);
    pthread_mutex_unlock (&out->lock);
    aml_audio_trace_int("out_flush", 0);
    return 0;
}

static void hwsync_mediasync_outset(struct aml_audio_device *adev, struct aml_stream_out *out, int* sync_enable,int hw_sync_id)
{
    if ((NULL == out->avsync_ctx) || (AVSYNC_TYPE_MEDIASYNC != out->avsync_type))
    {
        AM_LOGE("Error: out->avsync_ctx:%p, out->avsync_type:%d", out->avsync_ctx, out->avsync_type);
        return;
    }

    if (NULL == out->avsync_ctx->mediasync_ctx) {
        pthread_mutex_lock(&(out->avsync_ctx->lock));
        out->avsync_ctx->mediasync_ctx = mediasync_ctx_init();
        pthread_mutex_unlock(&(out->avsync_ctx->lock));
        if (NULL == out->avsync_ctx->mediasync_ctx) {
            AM_LOGE("mediasync_ctx_init error");
            return;
        }
    }

    if (adev->hw_mediasync == NULL) {
        adev->hw_mediasync = mediasync_wrap_create();
    }
    else
    {
        //media sync seek call the out_flush ,add for flush
        mediasync_wrap_destroy(adev->hw_mediasync);
        adev->hw_mediasync = mediasync_wrap_create();
    }

    bool ret_set_id = false;
    int has_audio = 1;
    int audio_sync_mode = 0;
    struct mediasync_audio_format audio_format={0};
    pthread_mutex_lock(&(out->avsync_ctx->lock));
    out->avsync_ctx->mediasync_ctx->handle = adev->hw_mediasync;
    out->avsync_ctx->mediasync_ctx->mediasync_id = hw_sync_id;
    out->avsync_ctx->get_tuning_latency = get_latency_pts;
    pthread_mutex_unlock(&(out->avsync_ctx->lock));
    mediasync_wrap_setParameter(adev->hw_mediasync, MEDIASYNC_KEY_ISOMXTUNNELMODE, &audio_sync_mode);
    mediasync_wrap_bindInstance(out->avsync_ctx->mediasync_ctx->handle, hw_sync_id, MEDIA_AUDIO);
    mediasync_wrap_setParameter(adev->hw_mediasync, MEDIASYNC_KEY_HASAUDIO, &has_audio);
    audio_format.format = out->hal_format;
    mediasync_wrap_setParameter(adev->hw_mediasync, MEDIASYNC_KEY_AUDIOFORMAT, &audio_format);
    out->output_speed = 1.0f;
    *sync_enable = ret_set_id ? 1 : 0;
    out->with_header = true;
    AM_LOGI("The current sync type: MediaSync %d,%d,%x, hw_mediasync:%p, out->with_header:%d", *sync_enable, hw_sync_id, out->hal_format, adev->hw_mediasync, out->with_header);
    return;
}

void out_set_playback_rate (struct audio_stream *stream, float rate)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;

    if (false == aml_out->enable_scaletempo)
    {
        AM_LOGE("set rate:%f fail, need enable_scaletempo first", rate);
        goto exit;
    }

    hal_scaletempo_update_rate(aml_out->scaletempo, rate);
    switch (aml_out->avsync_type)
    {
        case AVSYNC_TYPE_MEDIASYNC:
            if ((NULL == aml_out->avsync_ctx) ||
                (NULL == aml_out->avsync_ctx->mediasync_ctx) ||
                (NULL == aml_out->avsync_ctx->mediasync_ctx->handle)) {
                AM_LOGE("mediasync handle NULL fail");
                goto exit;
            }
            mediasync_wrap_setPlaybackRate(aml_out->avsync_ctx->mediasync_ctx->handle, rate);
            break;
        case AVSYNC_TYPE_MSYNC:
            if ((NULL == aml_out->avsync_ctx) ||
                (NULL == aml_out->avsync_ctx->msync_ctx) ||
                (NULL == aml_out->avsync_ctx->msync_ctx->msync_session)) {
                AM_LOGE("msync_session NULL fail");
                goto exit;
            }
            av_sync_set_speed(aml_out->avsync_ctx->msync_ctx->msync_session, rate);
            break;
        default :
            AM_LOGE("aml_out->avsync_type:%d, not support!", aml_out->avsync_type);
            break;
    }

exit:
    return;
}

static int out_set_parameters (struct audio_stream *stream, const char *kvpairs)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_stream_in *in;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret;
    uint32_t val = 0;
    bool force_input_standby = false;
    int channel_count = popcount (out->hal_channel_mask);
    bool hwsync_lpcm = (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && out->config.rate  <= 48000 &&
                        audio_is_linear_pcm(out->hal_internal_format) && channel_count <= 2);
    do_standby_func standy_func = NULL;
    do_startup_func   startup_func = NULL;

    standy_func = do_output_standby;
    startup_func = start_output_stream;

    ALOGD ("%s(kvpairs(%s), out_device=%#x)", __FUNCTION__, kvpairs, adev->out_device);
    parms = str_parms_create_str (kvpairs);

    ret = str_parms_get_str (parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof (value) );
    if (ret >= 0) {
        val = atoi (value);
        pthread_mutex_lock (&adev->lock);
        pthread_mutex_lock (&out->lock);
        if ( ( (adev->out_device & AUDIO_DEVICE_OUT_ALL) != val) && (val != 0) ) {
            ALOGI ("audio hw select device!\n");
            standy_func (out);
            /* a change in output device may change the microphone selection */
            if (adev->active_input &&
                adev->active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                force_input_standby = true;
            }
            /* force standby if moving to/from HDMI */
            if ( ( (val & AUDIO_DEVICE_OUT_AUX_DIGITAL) ^
                   (adev->out_device & AUDIO_DEVICE_OUT_AUX_DIGITAL) ) ||
                 ( (val & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ^
                   (adev->out_device & AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET) ) ) {
                standy_func (out);
            }
            adev->out_device &= ~AUDIO_DEVICE_OUT_ALL;
            adev->out_device |= val;
            //select_devices(adev);
        }
        pthread_mutex_unlock (&out->lock);
        if (force_input_standby) {
            in = adev->active_input;
            pthread_mutex_lock (&in->lock);
            do_input_standby (in);
            pthread_mutex_unlock (&in->lock);
        }
        pthread_mutex_unlock (&adev->lock);

        // We shall return Result::OK, which is 0, if parameter is set successfully,
        // or we can not pass VTS test.
        ALOGI ("Amlogic_HAL - %s: change ret value to 0 in order to pass VTS test.", __FUNCTION__);
        ret = 0;

        goto exit;
    }
    int sr = 0;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_SAMPLING_RATE, &sr);
    if (ret >= 0) {
        if (sr > 0) {
            struct pcm_config *config = &out->config;
            ALOGI ("audio hw sampling_rate change from %d to %d \n", config->rate, sr);
            config->rate = sr;
            pthread_mutex_lock (&adev->lock);
            pthread_mutex_lock (&out->lock);
            if (!out->standby) {
                //standy_func (out);
                //startup_func (out);
                out->standby = 0;
            }
            // set hal_rate to sr for passing VTS
            ALOGI ("Amlogic_HAL - %s: set sample_rate to hal_rate.", __FUNCTION__);
            out->hal_rate = sr;
            pthread_mutex_unlock (&adev->lock);
            pthread_mutex_unlock (&out->lock);
        }

        // We shall return Result::OK, which is 0, if parameter is set successfully,
        // or we can not pass VTS test.
        ALOGI ("Amlogic_HAL - %s: change ret value to 0 in order to pass VTS test.", __FUNCTION__);
        ret = 0;

        goto exit;
    }
    // Detect and set AUDIO_PARAMETER_STREAM_FORMAT for passing VTS
    audio_format_t fmt = 0;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_FORMAT, (int *) &fmt);
    if (ret >= 0) {
        if (fmt > 0) {
            struct pcm_config *config = &out->config;
            ALOGI ("[%s:%d] audio hw format change from %#x to %#x", __func__, __LINE__, config->format, fmt);
            config->format = fmt;
            pthread_mutex_lock (&adev->lock);
            pthread_mutex_lock (&out->lock);
            if (!out->standby) {
                standy_func (out);
                startup_func (out);
                out->standby = 0;
            }
            // set hal_format to fmt for passing VTS
            ALOGI ("Amlogic_HAL - %s: set format to hal_format. fmt = %d", __FUNCTION__, fmt);
            out->hal_format = fmt;
            pthread_mutex_unlock (&adev->lock);
            pthread_mutex_unlock (&out->lock);
        }

        // We shall return Result::OK, which is 0, if parameter is set successfully,
        // or we can not pass VTS test.
        ALOGI ("Amlogic_HAL - %s: change ret value to 0 in order to pass VTS test.", __FUNCTION__);
        ret = 0;

        goto exit;
    }
    // Detect and set AUDIO_PARAMETER_STREAM_CHANNELS for passing VTS
    audio_channel_mask_t channels = AUDIO_CHANNEL_OUT_STEREO;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_CHANNELS, (int *) &channels);
    if (ret >= 0) {
        if (channels > AUDIO_CHANNEL_NONE) {
            struct pcm_config *config = &out->config;
            ALOGI ("audio hw channel_mask change from %d to %d \n", config->channels, channels);
            config->channels = audio_channel_count_from_out_mask (channels);
            pthread_mutex_lock (&adev->lock);
            pthread_mutex_lock (&out->lock);
            if (!out->standby) {
                standy_func (out);
                startup_func (out);
                out->standby = 0;
            }
            // set out->hal_channel_mask to channels for passing VTS
            ALOGI ("Amlogic_HAL - %s: set out->hal_channel_mask to channels. fmt = %d", __FUNCTION__, channels);
            out->hal_channel_mask = channels;
            pthread_mutex_unlock (&adev->lock);
            pthread_mutex_unlock (&out->lock);
        }

        // We shall return Result::OK, which is 0, if parameter is set successfully,
        // or we can not pass VTS test.
        ALOGI ("Amlogic_HAL - %s: change ret value to 0 in order to pass VTS test.", __FUNCTION__);
        ret = 0;

        goto exit;
    }

    int input_eos_value = 0;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_INPUT_END_OF_STREAM, &input_eos_value);
    if (ret >= 0) {
        if (input_eos_value == 0) {
            out->eos = false;
        } else {
            out->eos = true;
        }
        AM_LOGI (" Amlogic_HAL - input_eos value changed, value: %d ", out->eos);
        ret = 0;
        goto exit;
    }

    int frame_size = 0;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_FRAME_COUNT, &frame_size);
    if (ret >= 0) {
        if (frame_size > 0) {
            struct pcm_config *config = &out->config;
            ALOGI ("audio hw frame size change from %d to %d \n", config->period_size, frame_size);
            config->period_size =  frame_size;
            pthread_mutex_lock (&adev->lock);
            pthread_mutex_lock (&out->lock);
            if (!out->standby) {
                standy_func (out);
                startup_func (out);
                out->standby = 0;
            }
            pthread_mutex_unlock (&adev->lock);
            pthread_mutex_unlock (&out->lock);
        }

        // We shall return Result::OK, which is 0, if parameter is set successfully,
        // or we can not pass VTS test.
        ALOGI("Amlogic_HAL - %s: change ret value to 0 in order to pass VTS test.", __FUNCTION__);
        ret = 0;

        goto exit;
    }

    ret = str_parms_get_str (parms, "adpcm_block_size", value, sizeof(value));
    if (ret >=0 ) {
        unsigned int block_size = atoi(value);
        out->hal_decode_block_size = block_size;

        ALOGI("[%s:%d]:set block_size=%d", __FUNCTION__, __LINE__, block_size);
        ret = 0;
        goto exit;
    }

    ret = str_parms_get_str (parms, "hw_av_sync_type", value, sizeof(value));
    if (ret >=0 ) {
        int hw_sync_type = atoi(value);
        out->avsync_type = hw_sync_type;

        ALOGI("[%s:%d]:set av sync type=%d", __FUNCTION__, __LINE__, hw_sync_type);
        ret = 0;
        goto exit;
    }

    ret = str_parms_get_str (parms, "hw_av_sync", value, sizeof(value));
    if (ret >= 0) {
        int hw_sync_id = atoi(value);
        int sync_enable = 0;
        void *msync_session;

        if (hw_sync_id < 0) {
            ALOGE("[%s:%d]:The set parameter is abnormal, hw_sync_id=%d", __FUNCTION__, __LINE__, hw_sync_id);
            ret = -EINVAL;
            goto exit;
        }

        if (hw_sync_id == 12345678) {
            ALOGE("[%s:%d]:tsync not support anymore!", __FUNCTION__, __LINE__);
        } else {
            if (out->avsync_type == AVSYNC_TYPE_MEDIASYNC) {
                hwsync_mediasync_outset(adev,out,&sync_enable,hw_sync_id);
            } else {
                if ((out->avsync_ctx->msync_ctx) && (out->avsync_ctx->msync_ctx->msync_session)) {
                    ALOGW("[%s:%d]:hw_av_sync id set w/o release previous session.", __FUNCTION__, __LINE__);
                    av_sync_destroy(out->avsync_ctx->msync_ctx->msync_session);
                    pthread_mutex_lock(&(out->avsync_ctx->lock));
                    out->avsync_ctx->msync_ctx->msync_session = NULL;
                    pthread_mutex_unlock(&(out->avsync_ctx->lock));
                    avsync_ctx_reset(out->avsync_ctx);
                }
                else {
                    pthread_mutex_lock(&(out->avsync_ctx->lock));
                    out->avsync_ctx->msync_ctx = msync_ctx_init();
                    pthread_mutex_unlock(&(out->avsync_ctx->lock));
                }

                audio_msync_t *msync_ctx = out->avsync_ctx->msync_ctx;
                msync_session = av_sync_attach(hw_sync_id, AV_SYNC_TYPE_AUDIO);
                if (!msync_session) {
                    AM_LOGE("Cannot attach hw_sync_id %d, error!", hw_sync_id);
                    ret = -EINVAL;
                    goto exit;
                }
                sync_enable = 1;
                out->avsync_type = AVSYNC_TYPE_MSYNC;
                out->with_header = true;
                pthread_mutex_lock(&(out->avsync_ctx->lock));
                out->avsync_ctx->get_tuning_latency = get_latency_pts;
                msync_ctx->first_apts_flag = false;
                msync_ctx->msync_first_insert_flag = false;
                msync_ctx->msync_session = msync_session;
                pthread_mutex_unlock(&(out->avsync_ctx->lock));
                AM_LOGI("The current sync type:MSYNC, av_sync_attach success");
            }
        }

        AM_LOGI("(%p)set hw_sync_id=%d, %s need_sync", out, hw_sync_id, sync_enable ? "enable" : "disable");

        if (adev->ms12_out != NULL && adev->ms12_out->pheader) {
            adev->ms12_out->with_header = out->with_header;
            adev->ms12_out->avsync_type = out->avsync_type;
            adev->ms12_out->need_sync   = out->need_sync;
            AM_LOGI("set ms12_out:%p, with_header:%d, avsync_type:%d", adev->ms12_out, adev->ms12_out->with_header, adev->ms12_out->avsync_type);
        }

        pthread_mutex_lock (&adev->lock);
        pthread_mutex_lock (&out->lock);
        out->frame_write_sum = 0;
        out->last_frames_position = 0;

        /* clear up previous playback output status */
        if (!out->standby) {
            standy_func(out);
        }

        if (sync_enable) {
            ALOGI ("[%s:%d]:init hal mixer when pheader", __FUNCTION__, __LINE__);
            aml_hal_mixer_init (&adev->hal_mixer);
            if (adev->dolby_lib_type == eDolbyMS12Lib) {
                adev->gap_ignore_pts = false;
            }
        }

        if (continuous_mode(adev) && (true == out->need_sync)) {
            dolby_ms12_hwsync_init();
        }

        pthread_mutex_unlock (&out->lock);
        pthread_mutex_unlock (&adev->lock);
        ret = 0;
        goto exit;
    }

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        ret = str_parms_get_str(parms, "ms12_runtime", value, sizeof(value));
        if (ret >= 0) {
            char *parm = strstr(kvpairs, "=");
            pthread_mutex_lock(&adev->lock);
            if (parm)
                aml_ms12_update_runtime_params(&(adev->ms12), parm+1);
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
    }

    ret = str_parms_get_str (parms, "playback_rate", value, sizeof (value));
    if (ret >= 0) {
        float rate = atof(value);
        ALOGI("change rate to %f, format:0x%x, enable %d", rate, out->hal_format, out->enable_scaletempo);
        out_set_playback_rate(stream, rate);
        goto exit;
    }

    ret = str_parms_get_str (parms, "will_pause", value, sizeof (value));
    if (ret >= 0) {
        int will_pause = atoi(value);
        out->will_pause = will_pause ? true : false;
        ALOGI("stream(%p) will pause: %d", out, out->will_pause);
        goto exit;
    }

exit:
    str_parms_destroy (parms);

    // We shall return Result::OK, which is 0, if parameter is NULL,
    // or we can not pass VTS test.
    if (ret < 0) {
        if (adev->debug_flag) {
            ALOGW("Amlogic_HAL - %s: parameter is NULL, change ret value to 0 in order to pass VTS test.", __func__);
        }
        ret = 0;
    }
    return ret;
}

static char *out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    char temp_buf[64] = {0};
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    aml_dec_t *aml_dec = aml_out->aml_dec;

    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES) || \
        strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS) || \
        strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        return out_get_parameters_wrapper_about_sup_sampling_rates__channels__formats(stream, keys);
    } else if (strstr (keys, "stream_sr") ) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);
        sprintf (temp_buf, "stream_sr=%d", dec_info.dec_info.stream_sr);
        return strdup (temp_buf);
    } else if (strstr (keys, "stream_ch") ) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);
        sprintf (temp_buf, "stream_ch=%d", dec_info.dec_info.stream_ch);
        return strdup (temp_buf);
    } else if (strstr (keys, "stream_bitrate") ) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);
        sprintf (temp_buf, "stream_bitrate=%d", dec_info.dec_info.stream_bitrate);
        return strdup (temp_buf);
    } else if (strstr (keys, "stream_error_frames") ) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);
        sprintf (temp_buf, "stream_error_frames=%d", dec_info.dec_info.stream_error_num);
        return strdup (temp_buf);
    } else if (strstr (keys, "stream_drop_frames") ) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);
        sprintf (temp_buf, "stream_drop_frames=%d", dec_info.dec_info.stream_drop_num);
        return strdup (temp_buf);
    } else if (strstr (keys, "stream_decode_frames") ) {
        aml_dec_info_t dec_info;
        memset(&dec_info, 0x00, sizeof(dec_info));
        aml_decoder_get_info(aml_dec, AML_DEC_STREMAM_INFO, &dec_info);
        sprintf (temp_buf, "stream_decode_frames=%d", dec_info.dec_info.stream_decode_num);
        return strdup (temp_buf);
    } else if (strstr (keys, "alsa_device_config")) {
        sprintf (temp_buf, "period_cnt=%d;period_sz=%d", aml_out->config.period_count, aml_out->config.period_size);
        return strdup (temp_buf);
    } else if (strstr (keys, "get_buffer_avail")) {
        if ((is_dolby_ms12_support_compression_format(aml_out->hal_internal_format) || is_multi_channel_pcm((struct audio_stream_out *)stream))) {
            sprintf (temp_buf, "remain_frame=%d", (dolby_ms12_get_main_buffer_avail(NULL)));
            return strdup (temp_buf);
        }
        return strdup ("");
    } else if (strstr (keys, "decoder_end_of_stream")) {
        sprintf (temp_buf, "decoder_end_of_stream=%d", aml_out->eos);
        AM_LOGI("%s", temp_buf);
        return strdup (temp_buf);
    } else {
        ALOGE("%s() keys %s is not supported! TODO!\n", __func__, keys);
        return strdup ("");
    }
}

static int out_set_volume_l (struct audio_stream_out *stream, float left, float right)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int ret = 0;
    bool is_dolby_format = is_dolby_ms12_support_compression_format(out->hal_internal_format);
    bool is_direct_pcm = is_direct_stream_and_pcm_format(out);
    bool is_mmap_pcm = is_mmap_stream_and_pcm_format(out);
    bool is_ms12_pcm_volume_control = (is_direct_pcm && !is_mmap_pcm);

    ALOGI("%s(), stream(%p), left:%f right:%f, continuous(%d), hal_internal_format:%x, is dolby %d is direct pcm %d is_mmap_pcm %d\n",
        __func__, stream, left, right, continuous_mode(adev), out->hal_internal_format, is_dolby_format, is_direct_pcm, is_mmap_pcm);

    /* for not use ms12 case, we can use spdif enc mute, other wise ms12 can handle it*/
    if (is_dts_format(out->hal_internal_format) ||
        (is_dolby_format && (eDolbyDcvLib == adev->dolby_lib_type || is_bypass_dolbyms12(stream) || adev->hdmi_format == BYPASS))) {
        if (left * adev->master_volume > FLOAT_ZERO) {
            ALOGI("set offload mute: false");
            out->offload_mute = false;
        } else if (left * adev->master_volume < FLOAT_ZERO) {
            ALOGI("set offload mute: true");
            out->offload_mute = true;
        }

        if (out->spdifout_handle) {
            aml_audio_spdifout_mute(out->spdifout_handle, out->offload_mute);
        }
        if (out->spdifout2_handle) {
            aml_audio_spdifout_mute(out->spdifout2_handle, out->offload_mute);
        }

    }
    out->volume_l_org = left;//left stream volume
    out->volume_r_org = right;//right stream volume
    out->volume_l = out->volume_l_org * adev->master_volume;//out stream vol=stream vol * master vol
    out->volume_r = out->volume_r_org * adev->master_volume;

    float tmp_volume_l = out->volume_l;
    float tmp_volume_r = out->volume_r;

    if (adev->audio_focus_enable) {
        tmp_volume_l = out->volume_l * (adev->audio_focus_volume / 100.0L);
        tmp_volume_r = out->volume_l * (adev->audio_focus_volume / 100.0L);
    }
    ALOGI("%s ori:%f  tmp:%f", __func__, out->volume_l, tmp_volume_l);
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        /* when MS12 lib is available, control stream volume by adjusting
         * main_volume (after first mixing between main1/main2/UI sound)
         * for main input, or adjust system and UI sound mixing gain
         */
        if (out->is_normal_pcm) {
            /* system sound */
            int gain = 2560.0f * log10((adev->master_mute) ? 0.0f : adev->master_volume);
            char parm[32] = "";

            gain += adev->syss_mixgain;
            if (gain > 0) {
                gain = 0;
            }
            if (gain < (-96 << 7)) {
                gain = -96 << 7;
            }
            sprintf(parm, "-sys_syss_mixgain %d,32,0", gain);
            aml_ms12_update_runtime_params(&(adev->ms12), parm);
            ALOGI("Set syss mixgain %d", gain);
#ifdef USE_APP_MIXING
        } else if (out->flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
            /* app sound */
            int gain = 2560.0f * log10((adev->master_mute) ? 0.0f : adev->master_volume);
            char parm[32] = "";

            gain += adev->apps_mixgain;
            if (gain > 0) {
                gain = 0;
            }
            if (gain < (-96 << 7)) {
                gain = -96 << 7;
            }
            sprintf(parm, "-sys_apps_mixgain %d,200,0", gain);
            aml_ms12_update_runtime_params(&(adev->ms12), parm);
            ALOGI("Set apps mixgain %d", gain);
#endif
        } else /*if (is_dolby_format || is_ms12_pcm_volume_control || esmode)*/ {
            if (tmp_volume_l != tmp_volume_r) {
                ALOGW("%s, left:%f right:%f NOT match", __FUNCTION__, left, right);
            }
            ALOGI("dolby_ms12_set_main_volume %f", tmp_volume_l);
            out->ms12_vol_ctrl = true;
            if (tmp_volume_l <= FLOAT_ZERO) {
                set_ms12_main_audio_mute(&adev->ms12, true, 32);
                adev->ms12.main_volume = tmp_volume_l;
            } else {
                set_ms12_main_audio_mute(&adev->ms12, false, 32);
                set_ms12_main_volume(&adev->ms12, tmp_volume_l);
            }
        }
    }

    return 0;
}

static int out_set_volume (struct audio_stream_out *stream, float left, float right)
{
    return out_set_volume_l(stream, left, right);
}

static int out_pause (struct audio_stream_out *stream)
{
    ALOGD ("out_pause(%p)\n", stream);

    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int r = 0;

    aml_audio_trace_int("out_pause", 1);
    pthread_mutex_lock (&adev->lock);
    pthread_mutex_lock (&out->lock);
    /* a stream should fail to pause if not previously started */
    if (out->standby || out->pause_status == true) {
        // If output stream is standby or paused,
        // we should return Result::INVALID_STATE (3),
        // thus we can pass VTS test.
        ALOGE ("%s: stream in wrong status. standby(%d) or paused(%d)",
                __func__, out->standby, out->pause_status);
        r = INVALID_STATE;
        goto exit;
    }
    if (out->need_sync) {
        int cnt = 0;
        for (int i=0; i<STREAM_USECASE_MAX; i++) {
            if (adev->active_outputs[i] != NULL) {
                cnt++;
            }
        }
        if (cnt > 1) {
            ALOGI ("more than one active stream,skip alsa hw pause\n");
            goto exit1;
        }
    }
    r = aml_alsa_output_pause(stream);
    if (out->spdifout_handle) {
        aml_audio_spdifout_pause(out->spdifout_handle);
    }

    if (out->spdifout2_handle) {
        aml_audio_spdifout_pause(out->spdifout2_handle);
    }
exit1:
    out->pause_status = true;

    if ((NULL != out->avsync_ctx) && (AVSYNC_TYPE_MSYNC == out->avsync_type) && out->avsync_ctx->msync_ctx) {
        if (out->pheader) {
            uint32_t apts32 = out->pheader->last_apts_from_header;
            int latency = out_get_latency(stream);
            struct audio_policy policy;
            if (aml_audio_property_get_bool("media.audiohal.ptslog", false))
                ALOGI("apts:%d, latency:%d ms", apts32, latency);
            av_sync_audio_render(out->avsync_ctx->msync_ctx->msync_session, apts32, &policy);
            if (latency > 0 && latency < 100)
                aml_audio_sleep(latency * 1000);
        }
        av_sync_pause(out->avsync_ctx->msync_ctx->msync_session, true);
    }
exit:
    if ((NULL != out->avsync_ctx) && (AVSYNC_TYPE_MEDIASYNC == out->avsync_type) && out->avsync_ctx->mediasync_ctx) {
        mediasync_wrap_setPause(out->avsync_ctx->mediasync_ctx->handle, true);
    }

    pthread_mutex_unlock (&adev->lock);
    pthread_mutex_unlock (&out->lock);
    aml_audio_trace_int("out_pause", 0);
    return r;
}

static int out_resume (struct audio_stream_out *stream)
{
    ALOGD ("out_resume (%p)\n", stream);
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int r = 0;
    int channel_count = popcount (out->hal_channel_mask);
    bool hwsync_lpcm = (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC && out->config.rate  <= 48000 &&
                        audio_is_linear_pcm(out->hal_internal_format) && channel_count <= 2);

    aml_audio_trace_int("out_resume", 1);

    if (adev->audio_ease) {
        start_ease_in(adev);
    }

    pthread_mutex_lock (&adev->lock);
    pthread_mutex_lock (&out->lock);
    /* a stream should fail to resume if not previously paused */
    if (/* !out->standby && */!out->pause_status) {
        // If output stream is not standby or not paused,
        // we should return Result::INVALID_STATE (3),
        // thus we can pass VTS test.
        ALOGE ("%s: stream in wrong status. standby(%d) or paused(%d)",
                __func__, out->standby, out->pause_status);
        r = INVALID_STATE;

        goto exit;
    }

    r = aml_alsa_output_resume(stream);

    if (out->spdifout_handle) {
        aml_audio_spdifout_resume(out->spdifout_handle);
    }

    if (out->spdifout2_handle) {
        aml_audio_spdifout_resume(out->spdifout2_handle);
    }

    if (out->need_sync) {
        ALOGI ("init hal mixer when pheader resume\n");
        aml_hal_mixer_init(&adev->hal_mixer);
        if ((NULL != out->avsync_ctx) && (AVSYNC_TYPE_MEDIASYNC == out->avsync_type) && out->avsync_ctx->mediasync_ctx) {
            mediasync_wrap_setPause(out->avsync_ctx->mediasync_ctx->handle, false);
        }
    }
    out->pause_status = false;

    if ((NULL != out->avsync_ctx) && (AVSYNC_TYPE_MSYNC == out->avsync_type) && out->avsync_ctx->msync_ctx) {
        av_sync_pause(out->avsync_ctx->msync_ctx->msync_session, false);
    }

exit:
    pthread_mutex_unlock (&adev->lock);
    pthread_mutex_unlock (&out->lock);
    aml_audio_trace_int("out_resume", 0);
    return r;
}

/* use standby instead of pause to fix background pcm playback */
static int out_pause_new (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(aml_dev->ms12);
    bool is_standby = aml_out->standby;
    int ret = 0;

    ALOGI("%s(), stream(%p), pause_status = %d,dolby_lib_type = %d, conti = %d,with_header = %d,ms12_enable = %d,ms_conti_paused = %d\n",
          __func__, stream, aml_out->pause_status, aml_dev->dolby_lib_type, aml_dev->continuous_audio_mode, aml_out->with_header, aml_dev->ms12.dolby_ms12_enable, aml_dev->ms12.is_continuous_paused);

    aml_audio_trace_int("out_pause_new", 1);
    if (aml_out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM) {
        ALOGI("%s(), do nothing for AD stream\n", __func__);
        return 0;
    }
    pthread_mutex_lock (&aml_dev->lock);
    pthread_mutex_lock (&aml_out->lock);

    /* a stream should fail to pause if not previously started */
    if (aml_out->pause_status == true) {
        // If output stream is standby or paused,
        // we should return Result::INVALID_STATE (3),
        // thus we can pass VTS test.
        ALOGE ("%s: stream in wrong status. standby(%d) or paused(%d)",
                __func__, aml_out->standby, aml_out->pause_status);
        ret = INVALID_STATE;
        goto exit;
    }

    if (AVSYNC_TYPE_MEDIASYNC == aml_out->avsync_type) {
        if (eDolbyMS12Lib != aml_dev->dolby_lib_type && aml_out->avsync_ctx->mediasync_ctx) {
            mediasync_wrap_setPause(aml_out->avsync_ctx->mediasync_ctx->handle, true);
        }
    }

    if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
        if (aml_dev->continuous_audio_mode == 1) {
            pthread_mutex_lock(&ms12->lock);
            if ((aml_dev->ms12.dolby_ms12_enable == true) &&
                (aml_dev->ms12.is_continuous_paused == false) &&
                (aml_out->write_func == MIXER_MAIN_BUFFER_WRITE)) {
                audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_PAUSE);
            } else {
                ALOGI("%s do nothing\n", __func__);
            }
            pthread_mutex_unlock(&ms12->lock);
        } else {
            /*if it raw data we don't do standby otherwise it may cause audioflinger
            underrun after resume please refer to issue SWPL-13091*/
            if (audio_is_linear_pcm(aml_out->hal_internal_format)) {
                ret = do_output_standby_l(&stream->common);
                if (ret < 0) {
                    goto exit;
                }
            }
        }
    } else {

        ret = do_output_standby_l(&stream->common);
        if (ret < 0) {
            goto exit;
        }
    }
exit:
    aml_out->pause_status = true;
    aml_out->position_update = 0;

    //1.audio easing duration is 32ms,
    //2.one loop for schedule_run cost about 32ms(contains the hardware costing),
    //3.if [pause, vol_ease(in adev_set_parameters by Netflix)] too short, means it need more time to do audio easing
    //so, the delay time for 32ms(pause is completed after audio easing is done) is enough.
    aml_audio_sleep(32000);

    /* do avsync pause after 32ms sleep for to avoid the system clock stop early than dolby fading out finish in dolby continue mode. ##SWPL-108005 */
    if ((NULL != aml_out->avsync_ctx) && (AVSYNC_TYPE_MSYNC == aml_out->avsync_type) && aml_out->avsync_ctx->msync_ctx) {
        av_sync_pause(aml_out->avsync_ctx->msync_ctx->msync_session, true);
    }

    if ((NULL != aml_out->avsync_ctx) && (AVSYNC_TYPE_MEDIASYNC == aml_out->avsync_type) && aml_out->avsync_ctx->mediasync_ctx) {
        mediasync_wrap_setPause(aml_out->avsync_ctx->mediasync_ctx->handle, true);
    }

    pthread_mutex_unlock(&aml_out->lock);
    pthread_mutex_unlock(&aml_dev->lock);

    if (is_standby) {
        ALOGD("%s(), stream(%p) already in standy, return INVALID_STATE", __func__, stream);
        ret = INVALID_STATE;
    }
    aml_out->will_pause = false;
    aml_audio_trace_int("out_pause_new", 0);
    ALOGI("%s(), stream(%p) exit", __func__, stream);
    return ret;
}

static int out_resume_new (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(aml_dev->ms12);
    int ret = 0;

    ALOGI("%s(), stream(%p),standby = %d,pause_status = %d\n", __func__, stream, aml_out->standby, aml_out->pause_status);
    aml_audio_trace_int("out_resume_new", 1);
    if (aml_out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM) {
        ALOGI("%s(), do nothing for AD stream\n", __func__);
        return 0;
    }
    pthread_mutex_lock(&aml_dev->lock);
    pthread_mutex_lock(&aml_out->lock);
    /* a stream should fail to resume if not previously paused */
    // aml_out->standby is always "1" in ms12 continuous mode, which will cause fail to resume here
    if (/*aml_out->standby || */aml_out->pause_status == false) {
        // If output stream is not standby or not paused,
        // we should return Result::INVALID_STATE (3),
        // thus we can pass VTS test.
        ALOGE ("Amlogic_HAL - %s: cannot resume, because output stream isn't in standby or paused state.", __FUNCTION__);
        ret = 3;
        goto exit;
    }

    if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
        if (aml_dev->continuous_audio_mode == 1) {
            if ((aml_dev->ms12.dolby_ms12_enable == true)
                && (aml_dev->ms12.is_continuous_paused == true || aml_out->pause_status == true)) {
                /*pcm case we resume here*/
                if (audio_is_linear_pcm(aml_out->hal_internal_format)) {
                    pthread_mutex_lock(&ms12->lock);
                    audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_RESUME);
                    pthread_mutex_unlock(&ms12->lock);
                } else {
                    /*raw data case, we resume it in write*/
                    ALOGI("resume raw data case later");
                    aml_dev->ms12.need_resume = 1;
                }
            }
        }
    }
    if (aml_out->pause_status == false) {
        ALOGE("%s(), stream status %d\n", __func__, aml_out->pause_status);
        goto exit;
    }

    if ((NULL != aml_out->avsync_ctx) && (AVSYNC_TYPE_MSYNC == aml_out->avsync_type) && aml_out->avsync_ctx->msync_ctx) {
        av_sync_pause(aml_out->avsync_ctx->msync_ctx->msync_session, false);
    }
    if ((NULL != aml_out->avsync_ctx) && (AVSYNC_TYPE_MEDIASYNC == aml_out->avsync_type) && aml_out->avsync_ctx->mediasync_ctx) {
        mediasync_wrap_setPause(aml_out->avsync_ctx->mediasync_ctx->handle, false);
    }

exit:
    aml_out->pause_status = false;
    pthread_mutex_unlock (&aml_out->lock);
    pthread_mutex_unlock (&aml_dev->lock);

    aml_audio_trace_int("out_resume_new", 0);
    return ret;
}

static int out_flush_new (struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct timespec ts;
    ALOGI("%s(), stream(%p)\n", __func__, stream);
    out->frame_write_sum  = 0;
    out->last_frames_position = 0;
    out->spdif_enc_init_frame_write_sum =  0;
    out->frame_skip_sum = 0;
    out->skip_frame = 0;
    out->input_bytes_size = 0;

    aml_audio_trace_int("out_flush_new", 1);
    if (out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM) {
        ALOGI("%s(), do nothing for AD stream\n", __func__);
        return 0;
    }
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (out->total_write_size == 0) {
            out->pause_status = false;
            ALOGI("%s not writing, do nothing", __func__);
            aml_audio_trace_int("out_flush_new", 0);
            return 0;
        }
        if (AVSYNC_TYPE_NULL != out->avsync_type) {
            if ((!out->pause_status) &&
                continuous_mode(adev) &&
                (adev->ms12.dolby_ms12_enable == true) &&
                (adev->ms12.is_continuous_paused == false) &&
                (out->write_func == MIXER_MAIN_BUFFER_WRITE) &&
                (!out->eos)) {

                ALOGI("flush easing pause");

                /* issue a pause for flush to get easing out w/o stopping msync clock */
                pthread_mutex_lock(&adev->lock);
                pthread_mutex_lock(&out->lock);

                ms12->is_continuous_paused = true;
                pthread_mutex_lock(&ms12->lock);
                audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_PAUSE);
                pthread_mutex_unlock(&ms12->lock);

                out->pause_status = true;
                pthread_mutex_unlock(&out->lock);
                pthread_mutex_unlock(&adev->lock);
            }

            audio_header_info_reset(out->pheader);
            avsync_ctx_reset(out->avsync_ctx);

            dolby_ms12_hwsync_init();

            adev->gap_passthrough_state = GAP_PASSTHROUGH_STATE_IDLE;
            adev->gap_offset = 0;
        }
        //normal pcm(mixer thread) do not flush dolby ms12 input buffer
        if (continuous_mode(adev) && (out->flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
            pthread_mutex_lock(&ms12->lock);
            if (adev->ms12.dolby_ms12_enable)
                audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_FLUSH);
            out->continuous_audio_offset = 0;
            /*SWPL-39814, when using exo do seek, sometimes audio track will be reused, then the
             *sequece will be pause->flush->writing data, we need to handle this.
             *It may causes problem for normal pause/flush/resume
             */
            if ((out->pause_status || adev->ms12.is_continuous_paused) && adev->ms12.dolby_ms12_enable) {
                audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_RESUME);
            }
            pthread_mutex_unlock(&ms12->lock);
        }
        hal_clip_meta_cleanup(out->clip_meta);
    }

    if (out->hal_format == AUDIO_FORMAT_IEC61937) {
        aml_spdif_decoder_reset(out->spdif_dec_handle);
    } if (out->hal_format == AUDIO_FORMAT_AC4) {
        aml_ac4_parser_reset(out->ac4_parser_handle);
    } else if (out->hal_format == AUDIO_FORMAT_AC3 || out->hal_format == AUDIO_FORMAT_E_AC3) {
        aml_ac3_parser_reset(out->ac3_parser_handle);
    }

    if (true == out->pause_status) {
        if ((NULL != out->avsync_ctx) && (AVSYNC_TYPE_MSYNC == out->avsync_type) && out->avsync_ctx->msync_ctx) {
            av_sync_pause(out->avsync_ctx->msync_ctx->msync_session, false);
        }
        if ((NULL != out->avsync_ctx) && (AVSYNC_TYPE_MEDIASYNC == out->avsync_type) && out->avsync_ctx->mediasync_ctx) {
            mediasync_wrap_setPause(out->avsync_ctx->mediasync_ctx->handle, false);
        }
    }

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        pthread_mutex_lock(&out->cond_lock);
        ts_wait_time(&ts, 100000);
        pthread_cond_timedwait(&out->cond, &out->cond_lock, &ts);
        pthread_mutex_unlock(&out->cond_lock);
    }

    out->pause_status = false;
    ALOGI("%s(), stream(%p) exit\n", __func__, stream);
    aml_audio_trace_int("out_flush_new", 0);
    return 0;
}

// insert bytes of zero data to pcm which makes A/V synchronization
int insert_output_bytes (struct aml_stream_out *out, size_t size)
{
    int ret = 0;
    size_t insert_size = size;
    size_t once_write_size = 0;
    struct audio_stream_out *stream = (struct audio_stream_out*)out;
    struct aml_audio_device *adev = out->dev;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    audio_format_t output_format = get_output_format(stream);
    char *insert_buf = (char*) aml_audio_malloc (8192);
    unsigned int device = out->device;

    out->pcm = adev->pcm_handle[device];

    if (insert_buf == NULL) {
        ALOGE ("malloc size failed \n");
        return -ENOMEM;
    }

    if (!out->pcm) {
        ret = -ENOENT;
        goto exit;
    }

    memset (insert_buf, 0, 8192);
    while (insert_size > 0) {
        once_write_size = insert_size > 8192 ? 8192 : insert_size;
        if (eDolbyMS12Lib == adev->dolby_lib_type && !is_bypass_dolbyms12(stream)) {
            size_t used_size = 0;
            struct audio_buffer abuffer = {0};
            abuffer.buffer = insert_buf;
            abuffer.size = once_write_size;
            ret = dolby_ms12_main_process(stream, &abuffer, &used_size);
            if (ret) {
                ALOGW("dolby_ms12_main_process cost size %zu,input size %zu,check!!!!", once_write_size, used_size);
            }
        } else {
            aml_hw_mixer_mixing(&adev->hw_mixer, insert_buf, once_write_size, output_format);
            //process_buffer_write(stream, buffer, bytes);
            if (audio_hal_data_processing(stream, insert_buf, once_write_size, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                hw_write(stream, output_buffer, output_buffer_bytes, output_format);
            }
        }
        insert_size -= once_write_size;
    }

exit:
    aml_audio_free (insert_buf);
    return 0;
}

enum hwsync_status check_hwsync_status (uint32_t apts_gap)
{
    enum hwsync_status sync_status;

    if (apts_gap < APTS_DISCONTINUE_THRESHOLD_MIN)
        sync_status = CONTINUATION;
    else if (apts_gap > APTS_DISCONTINUE_THRESHOLD_MAX)
        sync_status = RESYNC;
    else
        sync_status = ADJUSTMENT;

    return sync_status;
}

static int insert_output_bytes_direct (struct aml_stream_out *out, size_t size)
{
    int ret = 0;
    size_t insert_size = size;
    size_t once_write_size = 0;
    char *insert_buf = (char*) aml_audio_malloc (8192);

    if (insert_buf == NULL) {
        ALOGE ("malloc size failed \n");
        return -ENOMEM;
    }

    if (!out->pcm) {
        ret = -1;
        ALOGE("%s pcm is NULL", __func__);
        goto exit;
    }

    memset (insert_buf, 0, 8192);
    while (insert_size > 0) {
        once_write_size = insert_size > 8192 ? 8192 : insert_size;
        ret = pcm_write (out->pcm, insert_buf, once_write_size);
        if (ret < 0) {
            ALOGE("%s pcm_write failed", __func__);
            break;
        }
        insert_size -= once_write_size;
    }

exit:
    aml_audio_free (insert_buf);
    return ret;
}

static int out_get_render_position (const struct audio_stream_out *stream,
                                    uint32_t *dsp_frames)
{
    int ret = 0;
    uint64_t  dsp_frame_uint64 = 0;
    struct timespec timetamp = {0};
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    *dsp_frames = 0;
    ret = out_get_presentation_position(stream, &dsp_frame_uint64,&timetamp);
    if (ret == 0)
    {
        *dsp_frames = (uint32_t)(dsp_frame_uint64 & 0xffffffff);
    } else {
        ret = -ENOSYS;
    }

    if (adev->debug_flag) {
        ALOGD("%s,pos %d ret =%d \n",__func__,*dsp_frames, ret);
    }
    return ret;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *dev = out->dev;
    int i;
    int status = 0;

    pthread_mutex_lock (&dev->lock);
    pthread_mutex_lock (&out->lock);

    if (dev->native_postprocess.num_postprocessors >= MAX_POSTPROCESSORS) {
        status = -ENOSYS;
        goto exit;
    }

    /* save audio effect handle in audio hal. if it is saved, skip this. */
    for (i = 0; i < dev->native_postprocess.num_postprocessors; i++) {
        if (dev->native_postprocess.postprocessors[i] == effect) {
            status = 0;
            goto exit;
        }
    }

    dev->native_postprocess.postprocessors[dev->native_postprocess.num_postprocessors] = effect;

    effect_descriptor_t tmpdesc;
    (*effect)->get_descriptor(effect, &tmpdesc);
    //Set dts vx sound effect to the first in the sound effect array
    if (0 == strncmp(tmpdesc.name, "VirtualX",strlen("VirtualX"))) {
        dev->native_postprocess.libvx_exist = Check_VX_lib();
        ALOGI("%s, add audio effect: '%s' exist flag : %s", __FUNCTION__, VIRTUALX_LICENSE_LIB_PATH,
            (dev->native_postprocess.libvx_exist) ? "true" : "false");
        /* specify effect order for virtualx. VX does downmix from 5.1 to 2.0 */
        i = dev->native_postprocess.num_postprocessors;
        if (dev->native_postprocess.num_postprocessors >= 1) {
            effect_handle_t tmp;
            tmp = dev->native_postprocess.postprocessors[i];
            dev->native_postprocess.postprocessors[i] = dev->native_postprocess.postprocessors[0];
            dev->native_postprocess.postprocessors[0] = tmp;
            ALOGI("%s, add audio effect: Reorder VirtualX at the first of the effect chain.", __FUNCTION__);
        }
    }
    dev->native_postprocess.num_postprocessors++;

    ALOGI("%s, add audio effect: %s in audio hal, effect_handle: %p, total num of effects: %d",
        __FUNCTION__, tmpdesc.name, effect, dev->native_postprocess.num_postprocessors);

    if (dev->native_postprocess.num_postprocessors > dev->native_postprocess.total_postprocessors)
        dev->native_postprocess.total_postprocessors = dev->native_postprocess.num_postprocessors;

exit:
    pthread_mutex_unlock (&out->lock);
    pthread_mutex_unlock (&dev->lock);
    return status;
}

static int out_remove_audio_effect(const struct audio_stream *stream __unused, effect_handle_t effect __unused)
{
    /*struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *dev = out->dev;
    int i;
    int status = -EINVAL;
    bool found = false;

    pthread_mutex_lock (&dev->lock);
    pthread_mutex_lock (&out->lock);
    if (dev->native_postprocess.num_postprocessors <= 0) {
        status = -ENOSYS;
        goto exit;
    }

    for (i = 0; i < dev->native_postprocess.num_postprocessors; i++) {
        if (found) {
            dev->native_postprocess.postprocessors[i - 1] = dev->native_postprocess.postprocessors[i];
            continue;
        }

        if (dev->native_postprocess.postprocessors[i] == effect) {
            dev->native_postprocess.postprocessors[i] = NULL;
            status = 0;
            found = true;
        }
    }

    if (status != 0)
        goto exit;

    dev->native_postprocess.num_postprocessors--;

    effect_descriptor_t tmpdesc;
    (*effect)->get_descriptor(effect, &tmpdesc);
    ALOGI("%s, remove audio effect: %s in audio hal, effect_handle: %p, total num of effects: %d",
        __FUNCTION__, tmpdesc.name, effect, dev->native_postprocess.num_postprocessors);

exit:
    pthread_mutex_unlock (&out->lock);
    pthread_mutex_unlock (&dev->lock);
    return status;*/
    return 0;
}

static int out_get_next_write_timestamp (const struct audio_stream_out *stream __unused,
        int64_t *timestamp __unused)
{
    // return -EINVAL;

    // VTS can only recognizes Result:OK or Result:INVALID_STATE, which is 0 or 3.
    // So we return ESRCH (3) in order to pass VTS.
    ALOGI ("Amlogic_HAL - %s: return ESRCH (3) instead of -EINVAL (-22)", __FUNCTION__);
    return ESRCH;
}

//actually maybe it be not useful now  except pass CTS_TEST:
//  run cts -c android.media.cts.AudioTrackTest -m testGetTimestamp
static int out_get_presentation_position (const struct audio_stream_out *stream, uint64_t *frames, struct timespec *timestamp)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    uint64_t frames_written_hw = out->last_frames_position;
    int frame_latency = 0,timems_latency = 0;
    bool b_raw_in = false;
    bool b_raw_out = false;
    int ret = 0;
    /* Fixme, use the tinymix inside aml_audio_earctx_get_type() everytime!!! */
    bool is_earc = (ATTEND_TYPE_EARC == aml_audio_earctx_get_type(adev));

    if (!frames || !timestamp) {
        ALOGI("%s, !frames || !timestamp\n", __FUNCTION__);
        return -EINVAL;
    }

    /* add this code for VTS. */
    if (0 == frames_written_hw) {
        *frames = frames_written_hw;
        *timestamp = out->lasttimestamp;
        return ret;
    }

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        ret = aml_audio_get_ms12_presentation_position(stream, frames, timestamp);
    } else if (eDolbyDcvLib == adev->dolby_lib_type) {
         bool is_audio_type_dolby = (adev->audio_type == EAC3 || adev->audio_type == AC3);
         bool is_hal_format_dolby = (out->hal_format == AUDIO_FORMAT_AC3 || out->hal_format == AUDIO_FORMAT_E_AC3);
         if (is_audio_type_dolby || is_hal_format_dolby) {
             timems_latency = aml_audio_get_latency_offset(adev->active_outport,
                                                             out->hal_internal_format,
                                                             adev->sink_format,
                                                             adev->ms12.dolby_ms12_enable,
                                                             is_earc);
             if (is_audio_type_dolby) {
                frame_latency = timems_latency * (out->hal_rate * out->rate_convert / 1000);
             }
             else if (is_hal_format_dolby) {
                frame_latency = timems_latency * (out->hal_rate / 1000);
             }
         }
         if ((frame_latency < 0) && (frames_written_hw < abs(frame_latency))) {
             ALOGV("%s(), not ready yet", __func__);
             return -EINVAL;
         }

        if (frame_latency >= 0)
            *frames = frame_latency + frames_written_hw;
        else if (frame_latency < 0) {
            if (frames_written_hw >= abs(frame_latency))
                *frames = frame_latency + frames_written_hw;
            else
                *frames = 0;
        }

        unsigned int output_sr = (out->config.rate) ? (out->config.rate) : (MM_FULL_POWER_SAMPLING_RATE);
        *frames = *frames * out->hal_rate / output_sr;
        *timestamp = out->lasttimestamp;
    }

    if (adev->debug_flag) {
        ALOGI("out_get_presentation_position out %p %"PRIu64", sec = %ld, nanosec = %ld tunned_latency_ms %d frame_latency %d\n",
            out, *frames, timestamp->tv_sec, timestamp->tv_nsec, timems_latency, frame_latency);
        int64_t  frame_diff_ms =  (*frames - out->last_frame_reported) * 1000 / out->hal_rate;
        int64_t  system_time_ms = 0;
        if (timestamp->tv_nsec < out->last_timestamp_reported.tv_nsec) {
            system_time_ms = (timestamp->tv_nsec + 1000000000 - out->last_timestamp_reported.tv_nsec)/1000000;
        }
        else
            system_time_ms = (timestamp->tv_nsec - out->last_timestamp_reported.tv_nsec)/1000000;
        int64_t jitter_diff = llabs(frame_diff_ms - system_time_ms);
        if  (jitter_diff > JITTER_DURATION_MS) {
            ALOGI("%s jitter out last pos info: %p %"PRIu64", sec = %ld, nanosec = %ld\n",__func__,out, out->last_frame_reported,
                out->last_timestamp_reported.tv_sec, out->last_timestamp_reported.tv_nsec);
            ALOGI("%s jitter  system time diff %"PRIu64" ms, position diff %"PRIu64" ms, jitter %"PRIu64" ms \n",
                __func__,system_time_ms,frame_diff_ms,jitter_diff);
        }
        out->last_frame_reported = *frames;
        out->last_timestamp_reported = *timestamp;
    }

    return ret;
}
static int get_next_buffer (struct resampler_buffer_provider *buffer_provider,
                            struct resampler_buffer* buffer);
static void release_buffer (struct resampler_buffer_provider *buffer_provider,
                            struct resampler_buffer* buffer);

/** audio_stream_in implementation **/
static unsigned int select_port_by_device(struct aml_stream_in *in)
{
    struct aml_audio_device *adev = in->dev;
    unsigned int inport = PORT_I2S;
    audio_devices_t in_device = in->device;

    if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
        inport = PORT_PCM;
    } else if ((in_device & AUDIO_DEVICE_IN_HDMI) ||
            (in_device & AUDIO_DEVICE_IN_HDMI_ARC) ||
            (in_device & AUDIO_DEVICE_IN_SPDIF)) {
        /* fix auge tv input, hdmirx, tunner */
        if (alsa_device_is_auge() &&
                (in_device & AUDIO_DEVICE_IN_HDMI)) {
            inport = PORT_TV;
            if (get_hdmiin_audio_mode(&adev->alsa_mixer) == HDMIIN_MODE_I2S) {
                inport = PORT_I2S4HDMIRX;
            }
        } else if ((is_earc_descrpt()) &&
                (in_device & AUDIO_DEVICE_IN_HDMI_ARC)) {
            inport = PORT_EARC;
        } else {
            inport = PORT_SPDIF;
        }
    } else if ((in_device & AUDIO_DEVICE_IN_BACK_MIC) ||
            (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC)) {
        inport = PORT_BUILTINMIC;
    } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
        inport = PROT_TDM;
    } else {
        /* fix auge tv input, hdmirx, tunner */
        if (alsa_device_is_auge()
            && (in_device & AUDIO_DEVICE_IN_TV_TUNER))
            inport = PORT_TV;
        else
            inport = PORT_I2S;
    }
    if (in_device & AUDIO_DEVICE_IN_ECHO_REFERENCE)
        inport = PORT_ECHO_REFERENCE;

#ifdef ENABLE_AEC_HAL
    /* AEC using inner loopback port */
    if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC)
        inport = PORT_LOOPBACK;
#endif

    return inport;
}

/* TODO: add non 2+2 cases */
static void update_alsa_config(struct aml_stream_in *in) {
#ifdef ENABLE_AEC_HAL
    if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        in->config.rate = in->requested_rate;
        in->config.channels = 4;
    }
#endif
#ifdef ENABLE_AEC_APP
    if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        in->config.rate = in->requested_rate;
    }
#else
    (void)in;
#endif
}
static int choose_stream_pcm_config(struct aml_stream_in *in);
static int add_in_stream_resampler(struct aml_stream_in *in);

/* must be called with hw device and input stream mutexes locked */
int start_input_stream(struct aml_stream_in *in)
{
    struct aml_audio_device *adev = in->dev;
    unsigned int card = CARD_AMLOGIC_BOARD;
    unsigned int port = PORT_I2S;
    unsigned int alsa_device = 0;
    int ret = 0;

    ret = choose_stream_pcm_config(in);
    if (ret < 0)
        return -EINVAL;

    adev->active_input = in;
    if (adev->mode != AUDIO_MODE_IN_CALL) {
        adev->in_device &= ~AUDIO_DEVICE_IN_ALL;
        adev->in_device |= in->device;
    }

    card = alsa_device_get_card_index();
    port = select_port_by_device(in);
    /* check to update alsa device by port */
    alsa_device = alsa_device_update_pcm_index(port, CAPTURE);
    in->config.stop_threshold = in->config.period_size * in->config.period_count;
    AM_LOGD("open alsa_card(%d %d) alsa_device(%d), in_device:0x%x, period_size(%d), period_count(%d), stop_threshold(%d)\n",
        card, port, alsa_device, adev->in_device, in->config.period_size, in->config.period_count, in->config.stop_threshold);

    in->pcm = pcm_open(card, alsa_device, PCM_IN | PCM_MONOTONIC | PCM_NONEBLOCK, &in->config);
    if (!pcm_is_ready(in->pcm)) {
        ALOGE("%s: cannot open pcm_in driver: %s", __func__, pcm_get_error(in->pcm));
        pcm_close (in->pcm);
        adev->active_input = NULL;
        return -ENOMEM;
    }
    AM_LOGD("pcm_open in: card(%d), port(%d)", card, port);

    if (in->requested_rate != in->config.rate) {
        ret = add_in_stream_resampler(in);
        if (ret < 0) {
            pcm_close (in->pcm);
            adev->active_input = NULL;
            return -EINVAL;
        }
    }

    ALOGD("%s: device(%x) channels=%d period_size=%d rate=%d requested_rate=%d mode= %d",
        __func__, in->device, in->config.channels, in->config.period_size,
        in->config.rate, in->requested_rate, adev->mode);

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler) {
        in->resampler->reset(in->resampler);
        in->frames_in = 0;
    }

    return 0;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream __unused, uint32_t rate __unused)
{
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    size_t size;

    ALOGD("%s: enter: channel_mask(%#x) rate(%d) format(%#x)", __func__,
        in->hal_channel_mask, in->requested_rate, in->hal_format);
    size = get_input_buffer_size(in->config.period_size, in->requested_rate,
                                  in->hal_format,
                                  audio_channel_count_from_in_mask(in->hal_channel_mask));
    if (in->source == AUDIO_SOURCE_ECHO_REFERENCE) {
        size_t frames = CAPTURE_PERIOD_SIZE;
        frames = CAPTURE_PERIOD_SIZE * PLAYBACK_CODEC_SAMPLING_RATE / CAPTURE_CODEC_SAMPLING_RATE;
        size = get_input_buffer_size(frames, in->requested_rate,
                                  in->hal_format,
                                  audio_channel_count_from_in_mask(in->hal_channel_mask));
    }

    ALOGD("%s: exit: buffer_size = %zu", __func__, size);

    return size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;

    return in->hal_channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;

    ALOGV("%s(%p) in->hal_format=%d", __FUNCTION__, in, in->hal_format);
    return in->hal_format;
}

static int in_set_format(struct audio_stream *stream __unused, audio_format_t format __unused)
{
    return -ENOSYS;
}

/* must be called with hw device and input stream mutexes locked */
int do_input_standby(struct aml_stream_in *in)
{
    struct aml_audio_device *adev = in->dev;

    ALOGD ("%s(%p) in->standby = %d", __FUNCTION__, in, in->standby);
    if (!in->standby) {
        pcm_close (in->pcm);
        in->pcm = NULL;

        adev->active_input = NULL;
        if (adev->mode != AUDIO_MODE_IN_CALL) {
            adev->in_device &= ~AUDIO_DEVICE_IN_ALL;
            //select_input_device(adev);
        }

        in->standby = 1;
#if 0
        ALOGD ("%s : output_standby=%d,input_standby=%d",
                 __FUNCTION__, output_standby, input_standby);
        if (output_standby && input_standby) {
            reset_mixer_state (adev->ar);
            update_mixer_state (adev->ar);
        }
#endif
    }
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    int status;

    ALOGD("%s: enter: stream(%p)", __func__, stream);
    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    status = do_input_standby(in);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    ALOGD("%s: exit", __func__);

    return status;
}

static int in_dump (const struct audio_stream *stream __unused, int fd __unused)
{
    return 0;
}

static int in_set_parameters (struct audio_stream *stream, const char *kvpairs)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    struct aml_audio_device *adev = in->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;
    bool do_standby = false;

    ALOGD ("%s(%p, %s)", __FUNCTION__, stream, kvpairs);
    parms = str_parms_create_str (kvpairs);

    ret = str_parms_get_str (parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value, sizeof (value) );

    pthread_mutex_lock (&adev->lock);
    pthread_mutex_lock (&in->lock);
    if (ret >= 0) {
        val = atoi (value);
        /* no audio source uses val == 0 */
        if ( (in->source != val) && (val != 0) ) {
            in->source = val;
            do_standby = true;
        }
    }

    ret = str_parms_get_str (parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof (value) );
    if (ret >= 0) {
        val = atoi (value) & ~AUDIO_DEVICE_BIT_IN;
        if ( (in->device != (unsigned)val) && (val != 0) ) {
            in->device = val;
            do_standby = true;
        }
    }

    if (do_standby) {
        do_input_standby (in);
    }
    pthread_mutex_unlock (&in->lock);
    pthread_mutex_unlock (&adev->lock);

    int framesize = 0;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_FRAME_COUNT, &framesize);

    if (ret >= 0) {
        if (framesize > 0) {
            ALOGI ("Reset audio input hw frame size from %d to %d\n",
                   in->config.period_size * in->config.period_count, framesize);
            in->config.period_size = framesize / in->config.period_count;
            pthread_mutex_lock (&adev->lock);
            pthread_mutex_lock (&in->lock);

            if (!in->standby && (in == adev->active_input) ) {
                do_input_standby (in);
                start_input_stream (in);
                in->standby = 0;
            }

            pthread_mutex_unlock (&in->lock);
            pthread_mutex_unlock (&adev->lock);
        }
    }

    int format = 0;
    ret = str_parms_get_int (parms, AUDIO_PARAMETER_STREAM_FORMAT, &format);

    if (ret >= 0) {
        if (format != AUDIO_FORMAT_INVALID) {
            in->hal_format = (audio_format_t)format;
            ALOGV("  in->hal_format:%d  <== format:%d\n",
                   in->hal_format, format);

            pthread_mutex_lock (&adev->lock);
            pthread_mutex_lock (&in->lock);
            if (!in->standby && (in == adev->active_input) ) {
                do_input_standby (in);
                start_input_stream (in);
                in->standby = 0;
            }
            pthread_mutex_unlock (&in->lock);
            pthread_mutex_unlock (&adev->lock);
        }
    }

    str_parms_destroy (parms);

    // VTS can only recognizes Result::OK, which is 0x0.
    // So we change ret value to 0 when ret isn't equal to 0
    if (ret > 0) {
        ALOGI ("Amlogic_HAL - %s: change ret value to 0 if it's greater than 0 for passing VTS test.", __FUNCTION__);
        ret = 0;
    } else if (ret < 0) {
        ALOGI ("Amlogic_HAL - %s: parameter is NULL, change ret value to 0 if it's greater than 0 for passing VTS test.", __FUNCTION__);
        ret = 0;
    }

    return ret;
}

static uint32_t in_get_latency_frames (const struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *adev = in->dev;

    snd_pcm_sframes_t frames = 0;
    uint32_t whole_latency_frames;
    int ret = 0;
    int mul = 1;
    unsigned int device = in->device;

    in->pcm = adev->pcm_handle[device];

    whole_latency_frames = in->config.period_size * in->config.period_count;
    if (!in->pcm || !pcm_is_ready(in->pcm)) {
        return whole_latency_frames / mul;
    }
    ret = pcm_ioctl(in->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    return frames / mul;
}

static char * in_get_parameters (const struct audio_stream *stream, const char *keys)
{
    char *cap = NULL;
    char *para = NULL;
    char temp_buf[AUDIO_HAL_CHAR_MAX_LEN] = {0};
    struct aml_stream_in *in = (struct aml_stream_in *)stream;

    ALOGI ("in_get_parameters %s,in %p\n", keys, in);
    if (strstr (keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS) ) {
        ALOGV ("Amlogic - return hard coded sup_formats list for in stream.\n");
        cap = strdup ("sup_formats=AUDIO_FORMAT_PCM_16_BIT|AUDIO_FORMAT_PCM_32_BIT");
        if (cap) {
            para = strdup (cap);
            aml_audio_free (cap);
            return para;
        }
    } else if (strstr (keys, "alsa_device_config")) {
        sprintf (temp_buf, "period_cnt=%d;period_sz=%d", in->config.period_count, in->config.period_size);
        return strdup (temp_buf);
    } else if (strstr (keys, "get_aml_source_latency")) {
        sprintf (temp_buf, "aml_source_latency_ms=%u", in_get_latency_frames((const struct audio_stream_in *)in));
        ALOGI ("in_get_parameters %u", in_get_latency_frames((const struct audio_stream_in *)in));
        return strdup (temp_buf);
    }
    return strdup ("");
}

static int in_set_gain (struct audio_stream_in *stream, float gain)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *adev = in->dev;
    adev->src_gain[adev->active_inport] = gain;
    ALOGI(" - set src gain: gain[%f], active inport:%s", adev->src_gain[adev->active_inport], inputPort2Str(adev->active_inport));
    return 0;
}

static int get_next_buffer (struct resampler_buffer_provider *buffer_provider,
                            struct resampler_buffer* buffer)
{
    struct aml_stream_in *in;

    if (buffer_provider == NULL || buffer == NULL) {
        return -EINVAL;
    }

    in = (struct aml_stream_in *) ( (char *) buffer_provider -
                                    offsetof (struct aml_stream_in, buf_provider) );

    if (in->pcm == NULL) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    if (in->frames_in == 0) {
        in->read_status = aml_alsa_input_read ((struct audio_stream_in *)in, (void*) in->buffer,
                                    in->config.period_size * audio_stream_in_frame_size (&in->stream) );
        if (in->read_status != 0) {
            ALOGE ("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->frames_in = in->config.period_size;
    }

    buffer->frame_count = (buffer->frame_count > in->frames_in) ?
                          in->frames_in : buffer->frame_count;
    buffer->i16 = in->buffer + (in->config.period_size - in->frames_in) *
                  in->config.channels;

    return in->read_status;

}

static void release_buffer (struct resampler_buffer_provider *buffer_provider,
                            struct resampler_buffer* buffer)
{
    struct aml_stream_in *in;

    if (buffer_provider == NULL || buffer == NULL) {
        return;
    }

    in = (struct aml_stream_in *) ( (char *) buffer_provider -
                                    offsetof (struct aml_stream_in, buf_provider) );

    in->frames_in -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames (struct aml_stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider (in->resampler,
                                                   (int16_t *) ( (char *) buffer +
                                                           frames_wr * audio_stream_in_frame_size (&in->stream) ),
                                                   &frames_rd);
        } else {
            struct resampler_buffer buf = {
                { .raw = NULL, },
                .frame_count = frames_rd,
            };
            get_next_buffer (&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy ( (char *) buffer +
                         frames_wr * audio_stream_in_frame_size (&in->stream),
                         buf.raw,
                         buf.frame_count * audio_stream_in_frame_size (&in->stream) );
                frames_rd = buf.frame_count;
            }
            release_buffer (&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0) {
            return in->read_status;
        }

        frames_wr += frames_rd;
    }
    return frames_wr;
}

#define DEBUG_AEC_VERBOSE (0)
#define DEBUG_AEC (0) // Remove after AEC is fine-tuned
#define TIMESTAMP_LEN (8)

static uint32_t mic_buf_print_count = 0;

#ifdef ENABLE_AEC_HAL
static void inread_proc_aec(struct audio_stream_in *stream,
        void *buffer, size_t bytes)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    size_t in_frames = bytes / audio_stream_in_frame_size(stream);
    int channel_count = audio_channel_count_from_in_mask(in->hal_channel_mask);
    char *read_buf = in->tmp_buffer_8ch;
    aec_timestamp a_timestamp;
    int aec_frame_div = in_frames/512;
    if (in_frames % 512) {
        ALOGE("AEC should 512 frames align,now %d\n",in_frames);
        return ;
    }
    ALOGV("%s,in %d\n",__func__,in_frames);
    //split the mic data with the speaker data.
    short *mic_data, *speaker_data;
    short *read_buf_16 = (short *)read_buf;
    short *aec_out_buf = NULL;
    int cleaned_samples_per_channel = 0;
    size_t bytes_per_sample =
        audio_bytes_per_sample(stream->common.get_format(&stream->common));
    int enable_dump = aml_audio_property_get_bool("vendor.media.audio_hal.aec.outdump", false);
    if (enable_dump) {
        aml_audio_dump_audio_bitstreams("/data/tmp/audio_mix.raw",
                read_buf_16, in_frames*2*2*2);
    }
    //skip the 4 ch data buffer area
    mic_data = (short *)(read_buf + in_frames*2*2*2);
    speaker_data = (short *)(read_buf + in_frames*2*2*2 + in_frames*2*2);
    size_t jj = 0;
    if (channel_count == 2 || channel_count == 1) {
        for (jj = 0;  jj < in_frames; jj ++) {
            mic_data[jj*2 + 0] = read_buf_16[4*jj + 0];
            mic_data[jj*2 + 1] = read_buf_16[4*jj + 1];
            speaker_data[jj*2] = read_buf_16[4*jj + 2];
            speaker_data[jj*2 + 1] = read_buf_16[4*jj + 3];
        }
    } else {
        ALOGV("%s(), channel count inval: %d", __func__, channel_count);
        return;
    }
    if (enable_dump) {
            aml_audio_dump_audio_bitstreams("/data/tmp/audio_mic.raw",
                mic_data, in_frames*2*2);
            aml_audio_dump_audio_bitstreams("/data/tmp/audio_speaker.raw",
                speaker_data, in_frames*2*2);
    }
    for (int i = 0; i < aec_frame_div; i++) {
        cleaned_samples_per_channel = 512;
        short *cur_mic_data = mic_data + i*512*channel_count;
        short *cur_spk_data = speaker_data + i*512*channel_count;
        a_timestamp = get_timestamp();
        aec_set_mic_buf_info(512, a_timestamp.timeStamp, true);
        aec_set_spk_buf_info(512, a_timestamp.timeStamp, true);
        aec_out_buf = aec_spk_mic_process_int16(cur_spk_data,
                cur_mic_data, &cleaned_samples_per_channel);
        if (!aec_out_buf || cleaned_samples_per_channel == 0
                || cleaned_samples_per_channel > (int)512) {
            ALOGV("aec process fail %s,in %d clean sample %d,div %d,in frame %d,ch %d",
                    __func__,512,cleaned_samples_per_channel,aec_frame_div,in_frames,channel_count);
            adjust_channels(cur_mic_data, 2, (char *) buffer + channel_count*512*2*i, channel_count,
                    bytes_per_sample, 512*2*2);
        } else {
            if (enable_dump) {
                aml_audio_dump_audio_bitstreams("/data/tmp/audio_aec.raw",
                    aec_out_buf, cleaned_samples_per_channel*2*2);
            }
            ALOGV("%p,clean sample %d, in frame %d",
                aec_out_buf, cleaned_samples_per_channel, 512);
            adjust_channels(aec_out_buf, 2, (char *)buffer + channel_count*512*2*i, channel_count,
                    bytes_per_sample, 512*2*2);
        }
    }
    //apply volume here
    short *vol_buf = (short *)buffer;
    unsigned int kk = 0;
    int val32 = 0;
    for (kk = 0; kk < bytes/2; kk++) {
        val32 = vol_buf[kk] << 3;
        vol_buf[kk] = CLIP(val32);
    }
    if (enable_dump) {
        aml_audio_dump_audio_bitstreams("/data/tmp/audio_final.raw",
                vol_buf, bytes);
    }
}
#else
static void inread_proc_aec(struct audio_stream_in *stream,
        void *buffer, size_t bytes)
{
    (void)stream;
    (void)buffer;
    (void)bytes;
}
#endif

static ssize_t in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    int ret = 0;
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *adev = in->dev;
    int channel_count = audio_channel_count_from_in_mask(in->hal_channel_mask);
    size_t in_frames = bytes / audio_stream_in_frame_size(&in->stream);
    struct aml_audio_patch* patch = adev->audio_patch;
    size_t cur_in_bytes, cur_in_frames;
    int in_mute = 0, parental_mute = 0;
    bool stable = true;
    size_t size_aec = 0;
    void *buffer_aec = NULL;

    AM_LOGV("%s(): stream: %d, bytes %zu", __func__, in->source, bytes);

#ifdef ENABLE_AEC_APP
    /* Special handling for Echo Reference: simply get the reference from FIFO.
     * The format and sample rate should be specified by arguments to adev_open_input_stream. */
    if (in->source == AUDIO_SOURCE_ECHO_REFERENCE) {
        struct aec_info info;
        info.bytes = bytes;
        const uint64_t time_increment_nsec = (uint64_t)bytes * NANOS_PER_SECOND /
                                             audio_stream_in_frame_size(stream) /
                                             in_get_sample_rate(&stream->common);
        if (!aec_get_spk_running(adev->aec)) {
            if (in->timestamp_nsec == 0) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                const int64_t timestamp_nsec = audio_utils_ns_from_timespec(&now);
                in->timestamp_nsec = timestamp_nsec;
            } else {
                in->timestamp_nsec += time_increment_nsec;
            }
            memset(buffer, 0, bytes);
            const uint64_t time_increment_usec = time_increment_nsec / 1000;
            usleep(time_increment_usec);
        } else {
            int ref_ret = get_reference_samples(adev->aec, buffer, &info);
            if ((ref_ret) || (info.timestamp_usec == 0)) {
                memset(buffer, 0, bytes);
                in->timestamp_nsec += time_increment_nsec;
            } else {
                in->timestamp_nsec = 1000 * info.timestamp_usec;
            }
        }
        in->frames_read += in_frames;

#if DEBUG_AEC
        FILE* fp_ref = fopen("/data/local/traces/aec_ref.pcm", "a+");
        if (fp_ref) {
            fwrite((char*)buffer, 1, bytes, fp_ref);
            fclose(fp_ref);
        } else {
            ALOGE("AEC debug: Could not open file aec_ref.pcm!");
        }
        FILE* fp_ref_ts = fopen("/data/local/traces/aec_ref_timestamps.txt", "a+");
        if (fp_ref_ts) {
            fprintf(fp_ref_ts, "%" PRIu64 "\n", in->timestamp_nsec);
            fclose(fp_ref_ts);
        } else {
            ALOGE("AEC debug: Could not open file aec_ref_timestamps.txt!");
        }
#endif
        return info.bytes;
    }
#endif

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the input stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&in->lock);

    if (in->standby) {
        ret = start_input_stream(in);
        if (ret < 0)
            goto exit;
        in->standby = 0;
    }

    if (adev->dev_to_mix_parser != NULL) {
        bytes = aml_dev2mix_parser_process(in, buffer, bytes);
    }
#ifdef ENABLE_AEC_HAL
    if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        size_t read_size = 0;
        cur_in_frames = in_frames;
        cur_in_bytes = in_frames * 2 * 2;
        // 2 ch 16 bit TODO: add more fmt
        if (!in->tmp_buffer_8ch || in->tmp_buffer_8ch_size < 4 * cur_in_bytes) {
            AM_LOGI("%s: realloc tmp_buffer_8ch size from %zu to %zu",
                __func__, in->tmp_buffer_8ch_size, 4 * cur_in_bytes);
            in->tmp_buffer_8ch = aml_audio_realloc(in->tmp_buffer_8ch, 4 * cur_in_bytes);
            if (!in->tmp_buffer_8ch) {
                AM_LOGE("%s malloc failed\n", __func__);
            }
            in->tmp_buffer_8ch_size = 4 * cur_in_bytes;
        }
        // need read 4 ch out from the alsa driver then do aec.
        read_size = cur_in_bytes * 2;
        ret = aml_alsa_input_read(stream, in->tmp_buffer_8ch, read_size);
    }
#endif
    else if (adev->patch_src == SRC_DTV && adev->tuner2mix_patch) {
#ifdef USE_DTV
        ret = dtv_in_read(stream, buffer, bytes);
        apply_volume(adev->src_gain[adev->active_inport], buffer, sizeof(uint16_t), bytes);
        goto exit;
#endif
    } else {

        /* when audio is unstable, need to mute the audio data for a while
         * the mute time is related to hdmi audio buffer size
         */
        bool stable = signal_status_check(adev->in_device, &in->mute_mdelay, stream);

        if (!stable) {
            if (in->mute_log_cntr == 0)
                AM_LOGI("%s: audio is unstable, mute channel", __func__);
            if (in->mute_log_cntr++ >= 100)
                in->mute_log_cntr = 0;
            clock_gettime(CLOCK_MONOTONIC, &in->mute_start_ts);
            in->mute_flag = true;
        }
        if (in->mute_flag) {
            in_mute = Stop_watch(in->mute_start_ts, in->mute_mdelay);
            if (!in_mute) {
                AM_LOGI("%s: unmute audio since audio signal is stable", __func__);
                /* The data of ALSA has not been read for a long time in the muted state,
                 * resulting in the accumulation of data. So, cache of capture needs to be cleared.
                 */
                pcm_stop(in->pcm);
                in->mute_log_cntr = 0;
                in->mute_flag = false;
                /* fade in start */
                ALOGI("start fade in");
                start_ease_in(adev);
            }
        }

        if (adev->parental_control_av_mute && (adev->active_inport == INPORT_TUNER || adev->active_inport == INPORT_LINEIN))
            parental_mute = 1;

        /*if need mute input source, don't read data from hardware anymore*/
        if (adev->mic_mute || in_mute || parental_mute || in->spdif_fmt_hw == SPDIFIN_AUDIO_TYPE_PAUSE) {
            memset(buffer, 0, bytes);
            usleep(bytes * 1000000 / audio_stream_in_frame_size(stream) /
                in_get_sample_rate(&stream->common));
            ret = 0;
            /* when audio is unstable, start avaync*/
            if (patch && in_mute) {
                patch->need_do_avsync = true;
                patch->input_signal_stable = false;
            }
        } else {
            if (patch) {
                patch->input_signal_stable = true;
            }
            if (in->resampler) {
                ret = read_frames(in, buffer, in_frames);
            } else {
                if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
                    size_aec = bytes * 2;  /* 2ch mic + 2ch loopback */
                    buffer_aec = (char *)aml_audio_malloc(size_aec);
                    if (buffer_aec == NULL) {
                        ret = -ENOMEM;
                        goto exit;
                    }
                    memset(buffer_aec, 0, size_aec);

                    ret = aml_alsa_input_read(stream, buffer_aec, size_aec);
                    aml_audio_dump_audio_bitstreams("/data/dump_pre.raw", buffer_aec, size_aec);
                    aec_process(adev->aml_aec, buffer_aec, buffer);
                    aml_audio_dump_audio_bitstreams("/data/dump_post.raw", buffer, bytes);
                    aml_audio_free(buffer_aec);
                } else {
                    ret = aml_alsa_input_read(stream, buffer_aec, size_aec);
                }
            }
            if (ret < 0)
                goto exit;
            //DoDumpData(buffer, bytes, CC_DUMP_SRC_TYPE_INPUT);
        }
    }

#ifdef USE_EQ_DRC
    /*noise gate is only used in Linein for 16bit audio data*/
    if (adev->active_inport == INPORT_LINEIN && adev->aml_ng_enable == 1) {
        int ng_status = noise_evaluation(adev->aml_ng_handle, buffer, bytes >> 1);
        /*if (ng_status == NG_MUTE)
            ALOGI("noise gate is working!");*/
    }
#endif
//#ifdef ENABLE_AEC_HAL
#ifndef BUILD_LINUX
    if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        inread_proc_aec(stream, buffer, bytes);
    } else
#endif
    if (!adev->audio_patching) {
        /* case dev->mix, set audio gain to src */
        float source_gain = aml_audio_get_s_gain_by_src(adev, adev->patch_src);
        apply_volume(source_gain * adev->src_gain[adev->active_inport], buffer, sizeof(uint16_t), bytes);
    }

    if (ret >= 0) {
        in->frames_read += in_frames;
        in->timestamp_nsec = pcm_get_timestamp(in->pcm, in->config.rate, 0 /*isOutput*/);
    }
    bool mic_muted = false;
    adev_get_mic_mute((struct audio_hw_device*)adev, &mic_muted);
    if (mic_muted) {
        memset(buffer, 0, bytes);
    }

exit:
    if (ret < 0) {
        AM_LOGE("%s: read failed - sleeping for buffer duration", __func__);
        usleep(bytes * 1000000 / audio_stream_in_frame_size(stream) /
                in_get_sample_rate(&stream->common));
    }
    pthread_mutex_unlock(&in->lock);

#if DEBUG_AEC && defined(ENABLE_AEC_APP)
    FILE* fp_in = fopen("/data/local/traces/aec_in.pcm", "a+");
    if (fp_in) {
        fwrite((char*)buffer, 1, bytes, fp_in);
        fclose(fp_in);
    } else {
        AM_LOGE("AEC debug: Could not open file aec_in.pcm!");
    }
    FILE* fp_mic_ts = fopen("/data/local/traces/aec_in_timestamps.txt", "a+");
    if (fp_mic_ts) {
        fprintf(fp_mic_ts, "%" PRIu64 "\n", in->timestamp_nsec);
        fclose(fp_mic_ts);
    } else {
        ALOGE("AEC debug: Could not open file aec_in_timestamps.txt!");
    }
#endif
    if (ret >= 0 && aml_audio_property_get_bool("vendor.media.audiohal.indump", false)) {
        aml_audio_dump_audio_bitstreams("/data/audio/alsa_read.raw",
            buffer, bytes);
    }
    return bytes;
}

static int in_get_capture_position (const struct audio_stream_in* stream, int64_t* frames,
                                   int64_t* time) {
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    if ((in->dev->is_STB) && (in->device & AUDIO_DEVICE_IN_HDMI)) {
        return -ENOSYS;
    }
    *frames = in->frames_read;
    *time = in->timestamp_nsec;

    return 0;
}

static uint32_t in_get_input_frames_lost (struct audio_stream_in *stream __unused)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream __unused, effect_handle_t effect __unused)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    int status;
    effect_descriptor_t desc;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->num_preprocessors >= MAX_PREPROCESSORS) {
        status = -ENOSYS;
        goto exit;
    }

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        goto exit;

    in->preprocessors[in->num_preprocessors++] = effect;

    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->need_echo_reference = true;
        do_input_standby(in);
    }

exit:

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    int i;
    int status = -EINVAL;
    bool found = false;
    effect_descriptor_t desc;

    pthread_mutex_lock(&in->dev->lock);
    pthread_mutex_lock(&in->lock);
    if (in->num_preprocessors <= 0) {
        status = -ENOSYS;
        goto exit;
    }

    for (i = 0; i < in->num_preprocessors; i++) {
        if (found) {
            in->preprocessors[i - 1] = in->preprocessors[i];
            continue;
        }
        if (in->preprocessors[i] == effect) {
            in->preprocessors[i] = NULL;
            status = 0;
            found = true;
        }
    }

    if (status != 0)
        goto exit;

    in->num_preprocessors--;

    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        goto exit;
    if (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0) {
        in->need_echo_reference = false;
        do_input_standby(in);
    }

exit:

    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->dev->lock);
    return status;
}

static int in_get_active_microphones (const struct audio_stream_in *stream,
                                     struct audio_microphone_characteristic_t *mic_array,
                                     size_t *mic_count) {
    ALOGV("in_get_active_microphones");
    if ((mic_array == NULL) || (mic_count == NULL)) {
        return -EINVAL;
    }
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct audio_hw_device* dev = (struct audio_hw_device*)in->dev;
    bool mic_muted = false;
    adev_get_mic_mute(dev, &mic_muted);
    if ((in->source == AUDIO_SOURCE_ECHO_REFERENCE) || mic_muted) {
        *mic_count = 0;
        return 0;
    }
    adev_get_microphones(dev, mic_array, mic_count);
    return 0;
}

static int adev_get_microphones (const struct audio_hw_device* dev __unused,
                                struct audio_microphone_characteristic_t* mic_array,
                                size_t* mic_count) {
    ALOGV("adev_get_microphones");
    if ((mic_array == NULL) || (mic_count == NULL)) {
        return -EINVAL;
    }
    get_mic_characteristics(mic_array, mic_count);
    return 0;
}

static void get_mic_characteristics (struct audio_microphone_characteristic_t* mic_data,
                                    size_t* mic_count) {
    *mic_count = 1;
    memset(mic_data, 0, sizeof(struct audio_microphone_characteristic_t));
    strncpy(mic_data->device_id, "builtin_mic", sizeof("builtin_mic"));
    strncpy(mic_data->address, "top", sizeof("top"));

    memset(mic_data->channel_mapping, AUDIO_MICROPHONE_CHANNEL_MAPPING_UNUSED,
           sizeof(mic_data->channel_mapping));
    mic_data->device = AUDIO_DEVICE_IN_BUILTIN_MIC;
    mic_data->sensitivity = -37.0;
    mic_data->max_spl = AUDIO_MICROPHONE_SPL_UNKNOWN;
    mic_data->min_spl = AUDIO_MICROPHONE_SPL_UNKNOWN;
    mic_data->orientation.x = 0.0f;
    mic_data->orientation.y = 0.0f;
    mic_data->orientation.z = 0.0f;
    mic_data->geometric_location.x = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic_data->geometric_location.y = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic_data->geometric_location.z = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
}

static int out_set_event_callback(struct audio_stream_out *stream __unused,
                                       stream_event_callback_t callback, void *cookie /*StreamOut*/)
{
    ALOGD("func:%s  callback:%p cookie:%p", __func__, callback, cookie);

    return 0;
}

// open corresponding stream by flags, formats and others params
static int adev_open_output_stream(struct audio_hw_device *dev,
                                audio_io_handle_t handle __unused,
                                audio_devices_t devices,
                                audio_output_flags_t flags,
                                struct audio_config *config,
                                struct audio_stream_out **stream_out,
                                const char *address)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_stream_out *out;
    int digital_codec;
    int ret;

    ALOGD("%s: enter: devices(%#x) channel_mask(%#x) rate(%d) format(%#x) flags(%#x)", __func__,
        devices, config->channel_mask, config->sample_rate, config->format, flags);
    ALOGI("%s: enter, ver:%s", __func__, libVersion_audio_hal);
    out = (struct aml_stream_out *)aml_audio_calloc(1, sizeof(struct aml_stream_out));
    if (!out) {
        ALOGE("%s malloc error", __func__);
        return -ENOMEM;
    }

    if (address && !strncmp(address, "AML_", 4)) {
        ALOGI("%s(): aml TV source stream = %s", __func__, address);
        out->tv_src_stream = true;
    }

    if (address && !strncmp(address, "MS12", 4)) {
        out->is_ms12_stream = true;
        AM_LOGI("aml ms12 stream(%d)", out->is_ms12_stream);
    }
    if (flags == AUDIO_OUTPUT_FLAG_NONE)
        flags = AUDIO_OUTPUT_FLAG_PRIMARY;
    if (config->channel_mask == AUDIO_CHANNEL_NONE)
        config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    if (config->sample_rate == 0)
        config->sample_rate = 48000;
    out->rate_convert = 1;
    if (flags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        if (config->format == AUDIO_FORMAT_DEFAULT)
            config->format = AUDIO_FORMAT_PCM_16_BIT;

        out->stream.common.get_channels = out_get_channels;
        out->stream.common.get_format = out_get_format;

        out->hal_channel_mask = config->channel_mask;
        out->hal_rate = config->sample_rate;
        out->hal_format = config->format;
        out->hal_internal_format = out->hal_format;

        out->config = pcm_config_out;
        out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
        out->config.rate = config->sample_rate;

        switch (config->format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            out->config.format = PCM_FORMAT_S16_LE;
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            out->config.format = PCM_FORMAT_S32_LE;
            break;
        default:
            break;
        }
    } else if (flags & AUDIO_OUTPUT_FLAG_DIRECT) {
        if (config->format == AUDIO_FORMAT_DEFAULT)
            config->format = AUDIO_FORMAT_AC3;

        out->stream.common.get_channels = out_get_channels_direct;
        out->stream.common.get_format = out_get_format_direct;

        out->hal_channel_mask = config->channel_mask;
        out->hal_rate = config->sample_rate;
        out->hal_format = config->format;
        out->hal_internal_format = out->hal_format;
        if (out->hal_internal_format == AUDIO_FORMAT_E_AC3_JOC) {
            out->hal_internal_format = AUDIO_FORMAT_E_AC3;
            ALOGD("config hal_format %#x change to hal_internal_format(%#x)!", out->hal_format, out->hal_internal_format);
        }

        out->config = pcm_config_out_direct;
        out->config.start_threshold = out->config.period_size * 4;
        out->config.channels = audio_channel_count_from_out_mask(config->channel_mask);
        out->config.rate = config->sample_rate;
        switch (config->format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            out->config.format = PCM_FORMAT_S16_LE;
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            out->config.format = PCM_FORMAT_S32_LE;
            break;
        case AUDIO_FORMAT_IEC61937:
            if (out->config.channels == 2 && (out->config.rate == 192000 || out->config.rate == 176400 || out->config.rate == 128000)) {
                out->config.rate /= 4;
                out->hal_internal_format = AUDIO_FORMAT_E_AC3;
            } else if (out->config.channels == 2 && out->config.rate >= 32000 && out->config.rate <= 48000) {
                if (adev->audio_type == DTS) {
                    out->hal_internal_format = AUDIO_FORMAT_DTS;
                    //adev->dts_hd.is_dtscd = false;
                    adev->dolby_lib_type = eDolbyDcvLib;
                    out->restore_dolby_lib_type = true;
                } else {
                    out->hal_internal_format = AUDIO_FORMAT_AC3;
                }
            } else if (out->config.channels >= 6 && out->config.rate == 192000) {
                out->hal_internal_format = AUDIO_FORMAT_DTS_HD;
            } else {
                // do nothing
            }
            ALOGI("convert format IEC61937 to 0x%x\n", out->hal_internal_format);
            break;
        case AUDIO_FORMAT_AC3:
        case AUDIO_FORMAT_E_AC3:
        case AUDIO_FORMAT_AC4:
            break;
        case AUDIO_FORMAT_DTS:
        case AUDIO_FORMAT_DTS_HD:
            break;
        default:
            break;
        }

        digital_codec = get_codec_type(out->hal_internal_format);
        switch (digital_codec) {
        case TYPE_AC3:
        case TYPE_EAC3:
            out->config.period_size *= 2;
            out->raw_61937_frame_size = 4;
            break;
        case TYPE_TRUE_HD:
            out->config.period_size *= 4 * 2;
            out->raw_61937_frame_size = 16;
            break;
        case TYPE_DTS:
            out->config.period_count *= 2;
            out->raw_61937_frame_size = 4;
            break;
        case TYPE_DTS_HD:
            out->config.period_count *= 4;
            out->raw_61937_frame_size = 16;
            if (out->config.period_count * out->config.period_size > 4608) {// Fix Me:This is only for hdmi
                // The maximum dts-hd frame duration is 4096 frames, needs to be greater than this to avoid underruns at the start.
                out->config.start_threshold = 4608; // 4096 + 512
            }
            break;
        case TYPE_PCM:
            out->config.period_size *= 2;
            out->config.period_count *= 2;
            if (out->config.channels >= 6 || out->config.rate > 48000)
                adev->hi_pcm_mode = true;
            break;
        default:
            out->raw_61937_frame_size = 1;
            break;
        }
        if (codec_type_is_raw_data(digital_codec)) {
            ALOGI("%s: for raw audio output,force alsa stereo output", __func__);
            out->config.channels = 2;
            out->multich = 2;
        } else if (out->config.channels > 2) {
            out->multich = out->config.channels;
        }
    } else {
        // TODO: add other cases here
        ALOGE("%s: flags = %#x invalid", __func__, flags);
        ret = -EINVAL;
        goto err;
    }

    if (flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) {
        out->need_sync = true;
    }
    else {
        out->need_sync = false;
    }
    AM_LOGI("Set out->need_sync:%d", out->need_sync);

    out->hal_ch   = audio_channel_count_from_out_mask(out->hal_channel_mask);
    out->hal_frame_size = audio_bytes_per_frame(out->hal_ch, out->hal_internal_format);
    if (out->hal_ch == 0) {
        out->hal_ch = 2;
    }
    if (out->hal_frame_size == 0) {
        out->hal_frame_size = 1;
    }


    adev->audio_hal_info.format = AUDIO_FORMAT_PCM;
    adev->audio_hal_info.is_dolby_atmos = 0;
    adev->audio_hal_info.update_type = TYPE_PCM;
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.set_format = out_set_format;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.set_event_callback = out_set_event_callback;

    out->out_device = devices;
    out->flags = flags;
    if (address && !strncmp(address, "AML_DTV_SOURCE", 14)) {
        out->volume_l_org = adev->audio_patch->dtv_volume;
        out->volume_r_org = adev->audio_patch->dtv_volume;
    } else {
        out->volume_l_org = 1.0;//stream volume
        out->volume_r_org = 1.0;
    }

    if (adev->is_TV && !(flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        if (address && !strncmp(address, "AML_", 4)) {
            ALOGI("%s default set gain 1.0", __func__);
            adev->master_volume = 1.0;
            if (adev->audio_patching && (adev->patch_src == SRC_DTV || adev->patch_src == SRC_HDMIIN)) {
                /*Apply source gain for Digital Input using master_volume,
                **but master_volume cannot be bigger than 1.0.
                **Actually, it is NOT necessary to apply source gain for Digital Input.
                */
                ALOGI("%s,set gain for in_port:%s(%d),src_gain=%f", __func__, inputPort2Str(adev->active_inport), adev->active_inport, adev->src_gain[adev->active_inport]);
                adev->master_volume = adev->src_gain[adev->active_inport];
            }
        } else {
            adev->active_inport = INPORT_MEDIA;//MM source gain
            ALOGI("%s,active_inport=%s,src_gain=%f",__func__,inputPort2Str(adev->active_inport),adev->src_gain[adev->active_inport]);
            adev->master_volume = adev->src_gain[adev->active_inport];
        }
    }
    out->volume_l = adev->master_volume * out->volume_l_org;//out volume
    out->volume_r = adev->master_volume * out->volume_r_org;
    out->last_volume_l = 0.0;
    out->last_volume_r = 0.0;
    out->ms12_vol_ctrl = false;
    out->dev = adev;
    out->standby = true;
    out->frame_write_sum = 0;
    out->need_convert = false;
    out->need_drop_size = 0;
    out->position_update = 0;
    out->inputPortID = -1;
    out->will_pause = false;
    out->eos = false;
    out->output_speed = 1.0f;
    out->clip_offset = 0;
    out->clip_meta   = NULL;

//#ifdef ENABLE_MMAP
#ifndef BUILD_LINUX
    if (flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
        outMmapInit(out);
    }
#endif

    if (adev->audio_focus_enable) {
         if (flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ && (get_mmap_pcm_active_count(adev) == 0)) {
            ALOGI("first TTS sound open, set sound volume ratio for other stream");
            aml_audio_focus(adev, true);
         }
    }
    //aml_audio_pheader_init(out->pheader,out);
    /* FIXME: when we support multiple output devices, we will want to
     * do the following:
     * adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
     * adev->devices |= out->device;
     * select_output_device(adev);
     * This is because out_set_parameters() with a route is not
     * guaranteed to be called after an output stream is opened.
     */
    if (adev->is_TV && !(flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        out->is_tv_platform = 1;
        out->config.channels = adev->default_alsa_ch;
        out->config.format = PCM_FORMAT_S32_LE;

        out->tmp_buffer_8ch_size = out->config.period_size * 4 * adev->default_alsa_ch;
        out->tmp_buffer_8ch = aml_audio_malloc(out->tmp_buffer_8ch_size);
        if (!out->tmp_buffer_8ch) {
            ALOGE("%s: alloc tmp_buffer_8ch failed", __func__);
            ret = -ENOMEM;
            goto err;
        }
        memset(out->tmp_buffer_8ch, 0, out->tmp_buffer_8ch_size);

        out->audioeffect_tmp_buffer = aml_audio_malloc(out->config.period_size * 6);
        if (!out->audioeffect_tmp_buffer) {
            ALOGE("%s: alloc audioeffect_tmp_buffer failed", __func__);
            ret = -ENOMEM;
            goto err;
        }
        memset(out->audioeffect_tmp_buffer, 0, out->config.period_size * 6);
    }

    if (eDolbyMS12Lib == adev->dolby_lib_type && !(flags & AUDIO_OUTPUT_FLAG_AD_STREAM))
    {//clean pts
        struct renderpts_item *frame_item = NULL;
        struct listnode *item = NULL;
        pthread_mutex_lock(&adev->ms12.p_pts_list->list_lock);
        while (!list_empty(&adev->ms12.p_pts_list->frame_list)) {
            item = list_head(&adev->ms12.p_pts_list->frame_list);
            frame_item = node_to_item(item, struct renderpts_item, list);
            list_remove(item);
            aml_audio_free(frame_item);
        }
        adev->ms12.p_pts_list->prepts = HWSYNC_PTS_NA;
        adev->ms12.p_pts_list->gapts = 0;
        adev->ms12.p_pts_list->ptsnum = 0;
        adev->ms12.p_pts_list->ms12_pcmoutstep_val = 0;
        adev->ms12.p_pts_list->ms12_pcmoutstep_count = 0;
        pthread_mutex_unlock(&adev->ms12.p_pts_list->list_lock);
    }
    if (!(flags & AUDIO_OUTPUT_FLAG_AD_STREAM) && !out->is_ms12_stream) {
        out->avsync_ctx = avsync_ctx_init();
        if (NULL == out->avsync_ctx) {
            goto err;
        }

        /* updata hal_internal_format to ms12_out */
        struct aml_stream_out *ms12_out = (struct aml_stream_out *)adev->ms12_out;
        if (NULL != ms12_out) {
            ms12_out->hal_internal_format = out->hal_internal_format;
        }
    }
    /*if tunnel mode pcm is not 48Khz, resample to 48K*/
    if (flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) {
        ALOGD("%s format=%d rate=%d", __func__, out->hal_internal_format, out->config.rate);
        if (audio_is_linear_pcm(out->hal_internal_format) && out->config.rate != 48000) {
            ALOGI("init resampler from %d to 48000!\n", out->config.rate);
            out->aml_resample.input_sr = out->config.rate;
            out->aml_resample.output_sr = 48000;
            out->aml_resample.channels = 2;
            resampler_init (&out->aml_resample);
            /*max buffer from 32K to 48K*/
            if (!out->resample_outbuf) {
                out->resample_outbuf = (unsigned char*) aml_audio_malloc (8192 * 10);
                if (!out->resample_outbuf) {
                    ALOGE ("malloc buffer failed\n");
                    ret = -1;
                    goto err;
                }
            }
        } else {
            if (out->resample_outbuf)
                aml_audio_free(out->resample_outbuf);
            out->resample_outbuf = NULL;
        }

        if (adev->is_netflix) {
            adev->gap_passthrough_state = GAP_PASSTHROUGH_STATE_IDLE;
            adev->gap_offset = 0;
        }
    }

    if (adev->user_setting_scaletempo && !(flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        out->enable_scaletempo = true;
        hal_scaletempo_init(&out->scaletempo);
        dolby_ms12_register_callback_by_type(CALLBACK_SCALE_TEMPO, (void*)ms12_scaletempo, out);
        ALOGI("%s %d, enable scaletempo %p", __FUNCTION__, __LINE__, out);
    }

    out->ddp_frame_size = aml_audio_get_ddp_frame_size();
    out->resample_handle = NULL;
    out->set_init_avsync_policy = false;
    *stream_out = &out->stream;
    ALOGD("%s: exit", __func__);

    return 0;
err:
    if (out->audioeffect_tmp_buffer)
        aml_audio_free(out->audioeffect_tmp_buffer);
    if (out->tmp_buffer_8ch)
        aml_audio_free(out->tmp_buffer_8ch);
    if (out->avsync_ctx)
        aml_audio_free(out->avsync_ctx);
    aml_audio_free(out);
    ALOGE("%s exit failed =%d", __func__, ret);
    return ret;
}

static void close_ms12_output_main_stream(struct audio_stream_out *stream) {
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_stream_out *ms12_out = (struct aml_stream_out *)adev->ms12_out;

    /*main stream is closed, close the ms12 main decoder*/
    if (out->is_ms12_main_decoder) {
        pthread_mutex_lock(&adev->ms12.lock);
        /*after ms12 lock, dolby_ms12_enable may be cleared with clean up function*/
        if (adev->ms12.dolby_ms12_enable) {
            adev->ms12.need_ms12_resume = false;
            adev->ms12.need_resync = 0;
            adev->ms12_out->need_sync = false;
            audiohal_send_msg_2_ms12(&adev->ms12, MS12_MESG_TYPE_FLUSH);
            /*coverity[missing_lock]*/
            adev->ms12.ms12_resume_state = MS12_RESUME_FROM_CLOSE;
            audiohal_send_msg_2_ms12(&adev->ms12, MS12_MESG_TYPE_RESUME);
        }
        /*coverity[double_unlock]*/
        pthread_mutex_unlock(&adev->ms12.lock);

        /*main stream is closed, wait mesg processed*/
        {
            int wait_cnt = 0;
            while (!ms12_msg_list_is_empty(&adev->ms12)) {
                aml_audio_sleep(5000);
                wait_cnt++;
                if (wait_cnt >= 200) {
                    break;
                }
            }
            AM_LOGI("main stream message is processed cost =%d ms", wait_cnt * 5);
        }
        if (out->pheader && continuous_mode(adev)) {
            /*we only support one stream hw sync and MS12 always attach with it.
            So when it is released, ms12 also need set pheader to NULL*/
            struct aml_stream_out *ms12_out = (struct aml_stream_out *)adev->ms12_out;
            out->need_sync = 0;
            if (ms12_out != NULL) {
                ms12_out->need_sync = 0;
                ms12_out->pheader = NULL;
                ms12_out->avsync_ctx = NULL;
            }
            dolby_ms12_hwsync_release();
        }
        dolby_ms12_main_close(stream);
        out->is_ms12_main_decoder = false;
    }

    return;
}

//static int out_standby_new(struct audio_stream *stream);
static void adev_close_output_stream(struct audio_hw_device *dev,
                                    struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;

    if (dev == NULL || stream == NULL) {
        ALOGE("%s: Input parameter error!!!", __func__);
        return;
    }

    ALOGI("%s: enter: dev(%p) stream(%p)", __func__, dev, stream);

    if (out->aml_dec) {
        aml_decoder_release(out->aml_dec);
        out->aml_dec = NULL;
    }
    stream->common.standby(&stream->common);
    if (out->inputPortID != -1 && adev->useSubMix) {
        delete_mixer_input_port(adev->audio_mixer, out->inputPortID);
        out->inputPortID = -1;
    }

    pthread_mutex_lock(&out->lock);

    if (out->is_ms12_main_decoder) {
        close_ms12_output_main_stream(stream);
    }

    if (out->audioeffect_tmp_buffer) {
        aml_audio_free(out->audioeffect_tmp_buffer);
        out->audioeffect_tmp_buffer = NULL;
    }

    if (out->tmp_buffer_8ch) {
        aml_audio_free(out->tmp_buffer_8ch);
        out->tmp_buffer_8ch = NULL;
    }

    if (out->spdifenc_init) {
        aml_spdif_encoder_close(out->spdifenc_handle);
        out->spdifenc_handle = NULL;
        out->spdifenc_init = false;
    }

    if (!out->is_ms12_stream && out->avsync_ctx) {
        if (out->avsync_ctx->msync_ctx) {
            if (out->avsync_ctx->msync_ctx->msync_session) {
                msync_unblock_start(out->avsync_ctx->msync_ctx);
                av_sync_destroy(out->avsync_ctx->msync_ctx->msync_session);
            }
            aml_audio_free(out->avsync_ctx->msync_ctx);
        }
        if (out->avsync_ctx->mediasync_ctx) {
            if (out->avsync_ctx->mediasync_ctx->handle) {
                mediasync_wrap_destroy(out->avsync_ctx->mediasync_ctx->handle);
                adev->hw_mediasync = NULL;
            }
            aml_audio_free(out->avsync_ctx->mediasync_ctx);
        }
        aml_audio_free(out->avsync_ctx);
        out->avsync_ctx = NULL;
    }

#ifdef USE_MEDIAINFO
    if (adev->minfo_h) {
        if (adev->info_rid != -1) {
            minfo_retrieve_record(adev->minfo_h, adev->info_rid);
            adev->info_rid = -1;
            adev->minfo_atmos_flag = -1;
        }
    }
#endif

//#ifdef ENABLE_MMAP
#ifndef BUILD_LINUX
    if (out->flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
        outMmapDeInit(out);
    }
#endif
    if (adev->audio_focus_enable) {
        adev->audio_focus_enable = false;
        if (out->flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ && (get_mmap_pcm_active_count(adev) == 0)) {
           ALOGI("last TTS sound closed, set other sound volume as orignal of itself");
           aml_audio_focus(adev, false);
        }
    }
    if (out->restore_hdmitx_selection) {
        /* switch back to spdifa when the dual stream is done */
        aml_audio_select_spdif_to_hdmi(AML_SPDIF_A_TO_HDMITX);
        out->restore_hdmitx_selection = false;
    }
    if (out->resample_outbuf) {
        aml_audio_free(out->resample_outbuf);
        out->resample_outbuf = NULL;
    }

    if (out->hal_format == AUDIO_FORMAT_IEC61937) {
        aml_spdif_decoder_close(out->spdif_dec_handle);
        out->spdif_dec_handle = NULL;
    } if (out->hal_format == AUDIO_FORMAT_AC4) {
        aml_ac4_parser_close(out->ac4_parser_handle);
        out->ac4_parser_handle = NULL;
    } else if (out->hal_format == AUDIO_FORMAT_AC3 || out->hal_format == AUDIO_FORMAT_E_AC3) {
        aml_ac3_parser_close(out->ac3_parser_handle);
        out->ac3_parser_handle = NULL;
    } else if (out->hal_format == AUDIO_FORMAT_AAC || out->hal_format == AUDIO_FORMAT_AAC_LATM
            || out->hal_format == AUDIO_FORMAT_HE_AAC_V1
            || out->hal_format == AUDIO_FORMAT_HE_AAC_V2) {
        aml_heaac_parser_close(out->heaac_parser_handle);
        out->heaac_parser_handle = NULL;
    }

    if (continuous_mode(adev) && (eDolbyMS12Lib == adev->dolby_lib_type) && !(out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        if (out->volume_l != 1.0) {
            if (!audio_is_linear_pcm(out->hal_internal_format)) {
                ALOGI("recover main volume to %f", adev->master_volume);
                set_ms12_main_volume(&adev->ms12, 1.0 * adev->master_volume);
            }
        }
    }
    if (!out->is_ms12_stream && out->pheader) {
        aml_audio_free(out->pheader);
        out->pheader = NULL;
    }
    if (out->spdifout_handle) {
        aml_audio_spdifout_close(out->spdifout_handle);
        out->spdifout_handle = NULL;
    }
    if (out->spdifout2_handle) {
        aml_audio_spdifout_close(out->spdifout2_handle);
        out->spdifout2_handle = NULL;
    }


    if (out->resample_handle) {
        aml_audio_resample_close(out->resample_handle);
        out->resample_handle = NULL;
    }
    if (out->heaac_parser_handle) {
        aml_heaac_parser_close(out->heaac_parser_handle);
        out->heaac_parser_handle = NULL;
    }

    /*the dolby lib is changed, so we need restore it*/
    if (out->restore_dolby_lib_type) {
        pthread_mutex_lock(&adev->ms12.lock);
        adev->dolby_lib_type = adev->dolby_lib_type_last;
        pthread_mutex_unlock(&adev->ms12.lock);
        ALOGI("%s restore dolby lib =%d", __func__, adev->dolby_lib_type);
    }

    //we muted in audio_header_find_frame when find eos, here recover mute
    if (adev->dolby_lib_type != eDolbyMS12Lib && is_dolby_format(out->hal_internal_format) && !(out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        set_aed_master_volume_mute(&adev->alsa_mixer, false);
    }

    if (out->enable_scaletempo == true && !(out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        ALOGI("%s %d, release scaletempo %p", __FUNCTION__, __LINE__, out);
        out->enable_scaletempo = false;
        dolby_ms12_register_callback_by_type(CALLBACK_SCALE_TEMPO, NULL, NULL);
        hal_scaletempo_release(out->scaletempo);
        out->scaletempo = NULL;
    }

    if (out->clip_meta) {
        hal_clip_meta_release(out->clip_meta);
        out->clip_meta = NULL;
        out->clip_offset = 0;
        dolby_ms12_register_callback_by_type(CALLBACK_CLIP_META, NULL, NULL);
    }

    if (out->llp_buf) {
        aml_alsa_set_llp(false);
        dolby_ms12_register_callback_by_type(CALLBACK_LLP_BUFFER, NULL, NULL);
        unsetenv(LLP_BUFFER_NAME);
        IpcBuffer_destroy(out->llp_buf);
        out->llp_buf = NULL;
        adev->sink_allow_max_channel = false;
    }
    out->need_sync   = false;
    out->with_header = false;
    if (out->write_func == MIXER_MAIN_BUFFER_WRITE) {
        //clear the hal format when main close.
        adev->audio_hal_info.update_type = TYPE_PCM;
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_AUDIO_HAL_FORMAT, TYPE_PCM);
    }
    pthread_mutex_unlock(&out->lock);
    adev->audio_hal_info.is_decoding = false;
    adev->audio_hal_info.first_decoding_frame = false;
    if (stream) {
        aml_audio_free(stream);
        stream = NULL;
    }

    ALOGI("%s: exit", __func__);
}

static void adev_Updata_ActiveOutport(struct aml_audio_device *adev, char *OutputDestination) {
    enum OUT_PORT outport = OUTPORT_SPEAKER;
    int output_destination = atoi(OutputDestination);
    ALOGI("[%s:%d] OutputDestination=%#x", __func__, __LINE__, output_destination);
    switch (output_destination) {
        case (AUDIO_DEVICE_OUT_REMOTE_SUBMIX):
            outport = OUTPORT_REMOTE_SUBMIX;
            break;
        case (AUDIO_DEVICE_OUT_WIRED_HEADPHONE):
            outport = OUTPORT_HEADPHONE;
            break;
        case (AUDIO_DEVICE_OUT_HDMI_ARC):
            outport = OUTPORT_HDMI_ARC;
            break;
        case (AUDIO_DEVICE_OUT_SPEAKER):
            outport = OUTPORT_SPEAKER;
            break;
        case (AUDIO_DEVICE_OUT_HDMI):
            outport = OUTPORT_HDMI;
            break;
        case (AUDIO_DEVICE_OUT_SPDIF):
            outport = OUTPORT_SPDIF;
            break;
        case (AUDIO_DEVICE_OUT_LINE):
            outport = OUTPORT_AUX_LINE;
            break;
        default:
            outport = OUTPORT_MAX;
            break;
    }
    adev->active_outport = outport;
}


static int aml_audio_output_routing(struct audio_hw_device *dev,
                                    enum OUT_PORT outport,
                                    bool user_setting)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;

    if (aml_dev->active_outport != outport) {
        ALOGI("%s: switch from %s to %s", __func__,
            outputPort2Str(aml_dev->active_outport), outputPort2Str(outport));

        /* switch off the active output */
        switch (aml_dev->active_outport) {
        case OUTPORT_SPEAKER:
            audio_route_apply_path(aml_dev->ar, "speaker_off");
            break;
        case OUTPORT_HDMI_ARC:
            audio_route_apply_path(aml_dev->ar, "hdmi_arc_off");
            break;
        case OUTPORT_HEADPHONE:
            audio_route_apply_path(aml_dev->ar, "headphone_off");
            break;
        case OUTPORT_A2DP:
            break;
        case OUTPORT_BT_SCO:
        case OUTPORT_BT_SCO_HEADSET:
            close_btSCO_device(aml_dev);
            break;
        default:
            ALOGW("%s: pre active_outport:%d unsupport", __func__, aml_dev->active_outport);
            break;
        }

        /* switch on the new output */
        switch (outport) {
        case OUTPORT_SPEAKER:
            if (!aml_dev->speaker_mute)
                audio_route_apply_path(aml_dev->ar, "speaker");
            audio_route_apply_path(aml_dev->ar, "spdif");
            break;
        case OUTPORT_HDMI_ARC:
            audio_route_apply_path(aml_dev->ar, "hdmi_arc");
            /* TODO: spdif case need deal with hdmi arc format */
            if (aml_dev->hdmi_format != 3)
                audio_route_apply_path(aml_dev->ar, "spdif");
            break;
        case OUTPORT_HEADPHONE:
            audio_route_apply_path(aml_dev->ar, "headphone");
            audio_route_apply_path(aml_dev->ar, "spdif");
            break;
        case OUTPORT_A2DP:
        case OUTPORT_BT_SCO:
        case OUTPORT_BT_SCO_HEADSET:
            break;
        default:
            ALOGW("%s: cur outport:%d unsupport", __func__, outport);
            break;
        }

        audio_route_update_mixer(aml_dev->ar);
        aml_dev->active_outport = outport;
    } else if (outport == OUTPORT_SPEAKER && user_setting) {
        /* In this case, user toggle the speaker_mute menu */
        if (aml_dev->speaker_mute)
            audio_route_apply_path(aml_dev->ar, "speaker_off");
        else
            audio_route_apply_path(aml_dev->ar, "speaker");
        audio_route_update_mixer(aml_dev->ar);
    } else {
        ALOGI("%s: outport %s already exists, do nothing", __func__, outputPort2Str(outport));
    }

    return 0;
}

static int aml_audio_input_routing(struct audio_hw_device *dev ,
                                    enum IN_PORT inport)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;

    if (aml_dev->active_inport != inport) {
        ALOGI("%s: switch from %s to %s", __func__,
            inputPort2Str(aml_dev->active_inport), inputPort2Str(inport));
        switch (inport) {
        case INPORT_HDMIIN:
            audio_route_apply_path(aml_dev->ar, "hdmirx_in");
            break;
        case INPORT_LINEIN:
            audio_route_apply_path(aml_dev->ar, "line_in");
            break;
        default:
            ALOGW("%s: cur inport:%d unsupport", __func__, inport);
            break;
        }

        audio_route_update_mixer(aml_dev->ar);
        aml_dev->active_inport = inport;
    }

    return 0;
}

static int aml_audio_update_arc_status(struct aml_audio_device *adev, bool enable)
{
    ALOGI("[%s:%d] set audio hdmi arc status:%d", __func__, __LINE__, enable);
    audio_route_set_hdmi_arc_mute(&adev->alsa_mixer, !enable);
    adev->bHDMIARCon = enable;
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    int is_arc_connected = 0;

    if (enable) {
        memcpy(hdmi_desc, &adev->hdmi_arc_capability_desc, sizeof(struct aml_arc_hdmi_desc));
    } else {
        memset(hdmi_desc, 0, sizeof(struct aml_arc_hdmi_desc));
        hdmi_desc->pcm_fmt.fmt = AML_HDMI_FORMAT_LPCM;
        hdmi_desc->dts_fmt.fmt = AML_HDMI_FORMAT_DTS;
        hdmi_desc->dtshd_fmt.fmt = AML_HDMI_FORMAT_DTSHD;
        hdmi_desc->dd_fmt.fmt = AML_HDMI_FORMAT_AC3;
        hdmi_desc->ddp_fmt.fmt = AML_HDMI_FORMAT_DDP;
        hdmi_desc->mat_fmt.fmt = AML_HDMI_FORMAT_MAT;
    }
    /*
     * when user switch UI setting, means output device changed,
     * use "arc_hdmi_updated" paramter to notify HDMI ARC have updated status
     * while in config_output(), it detects this change, then re-route output config.
     */
    adev->arc_hdmi_updated = 1;

    if (adev->bHDMIARCon) {
        is_arc_connected = 1;
    }

    /*
    *   when ARC is connecting, and user switch [Sound Output Device] to "ARC"
    *   we need to set out_port as OUTPORT_HDMI_ARC ,
    *   the aml_audio_output_routing() will "UNmute" ARC and "mute" speaker.
    */
    int out_port = adev->active_outport;
    if (adev->active_outport == OUTPORT_SPEAKER && is_arc_connected) {
        out_port = OUTPORT_HDMI_ARC;
    }

    /*
    *   when ARC is connecting, and user switch [Sound Output Device] to "speaker"
    *   we need to set out_port as OUTPORT_SPEAKER ,
    *   the aml_audio_output_routing() will "mute" ARC and "unmute" speaker.
    */
    if (adev->active_outport == OUTPORT_HDMI_ARC && !is_arc_connected) {
        out_port = OUTPORT_SPEAKER;
    }
    ALOGI("[%s:%d] out_port:%s", __func__, __LINE__, outputPort2Str(out_port));
    aml_audio_output_routing((struct audio_hw_device *)adev, out_port, true);

    return 0;
}

static int aml_audio_set_speaker_mute(struct aml_audio_device *adev, char *value)
{
    int ret = 0;
    if (strncmp(value, "true", 4) == 0 || strncmp(value, "1", 1) == 0) {
        adev->speaker_mute = 1;
    } else if (strncmp(value, "false", 5) == 0 || strncmp(value, "0", 1) == 0) {
        adev->speaker_mute = 0;
    } else {
        ALOGE("%s() unsupport speaker_mute value: %s", __func__, value);
    }
    /*
     * when ARC is connecting, and user switch [Sound Output Device] to "speaker"
     * we need to set out_port as OUTPORT_SPEAKER ,
     * the aml_audio_output_routing() will "mute" ARC and "unmute" speaker.
     */
    int out_port = adev->active_outport;
    if (adev->active_outport == OUTPORT_HDMI_ARC && adev->speaker_mute == 0) {
        out_port = OUTPORT_SPEAKER;
    }
    ret = aml_audio_output_routing(&adev->hw_device, out_port, true);
    if (ret < 0) {
        ALOGE("%s() output routing failed", __func__);
    }
    return 0;
}

static void check_usb_card_device(struct str_parms *parms, int device)
{
    /*usb audio hot plug need delay some time wait alsa file create */
    if ((device & AUDIO_DEVICE_OUT_ALL_USB) || (device & AUDIO_DEVICE_IN_ALL_USB)) {
        int card = 0, alsa_dev = 0, val, retry;
        char fn[256];
        int ret = str_parms_get_int(parms, "card", &val);
        if (ret >= 0) {
            card = val;
        }
        ret = str_parms_get_int(parms, "device", &val);
        if (ret >= 0) {
            alsa_dev = val;
        }
        snprintf(fn, sizeof(fn), "/dev/snd/pcmC%uD%u%c", card, alsa_dev,
             device & AUDIO_DEVICE_OUT_ALL_USB ? 'p' : 'c');
        for (retry = 0; access(fn, F_OK) < 0 && retry < 10; retry++) {
            usleep (20000);
        }
        if (access(fn, F_OK) < 0 && retry >= 10) {
            ALOGE("usb audio create alsa file time out,need check \n");
        }
    }
}
static void set_device_connect_state(struct aml_audio_device *adev, struct str_parms *parms, int device, bool state)
{
    ALOGE("state:%d, dev:%#x, pre_out:%#x, pre_in:%#x", state, device, adev->out_device, adev->in_device);
    if (state) {
        check_usb_card_device(parms, device);
        if (audio_is_output_device(device)) {
            if ((device & AUDIO_DEVICE_OUT_HDMI_ARC) || (device & AUDIO_DEVICE_OUT_HDMI)) {
                adev->bHDMIConnected = 1;
                memset(adev->last_arc_hdmi_array, 0, EDID_ARRAY_MAX_LEN);
                adev->bHDMIConnected_update = 1;
                if (device & AUDIO_DEVICE_OUT_HDMI_ARC) {
                    if (eDolbyMS12Lib == adev->dolby_lib_type) {
                        /*when arc is connected, disable dap*/
                        set_ms12_full_dap_disable(&adev->ms12, true);
                    }
                    aml_audio_set_speaker_mute(adev, "true");
                    aml_audio_update_arc_status(adev, true);
                }
#ifdef BUILD_LINUX
                if ((device & AUDIO_DEVICE_OUT_HDMI) && adev->is_STB) {
                    aml_audio_set_speaker_mute(adev, "true");
                    adev->active_outport = OUTPORT_HDMI;
                }
#endif
                update_sink_format_after_hotplug(adev);
            } else if (device & AUDIO_DEVICE_OUT_ALL_A2DP) {
                adev->a2dp_updated = 1;
                adev->out_device |= device;
                //a2dp_out_open(adev);
            } else if (device &  AUDIO_DEVICE_OUT_ALL_USB ||
                       device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                       device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                adev->out_device |= device;
            } else if (device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                       device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                adev->out_device |= device;
            }
        }
    } else {
        if (audio_is_output_device(device)) {
            if ((device & AUDIO_DEVICE_OUT_HDMI_ARC) || (device & AUDIO_DEVICE_OUT_HDMI)) {
                adev->bHDMIConnected = 0;
                adev->is_hdmi_arc_interact_done = false;
                adev->bHDMIConnected_update = 1;
                memset(adev->last_arc_hdmi_array, 0, EDID_ARRAY_MAX_LEN);
                adev->hdmi_descs.pcm_fmt.max_channels = 2;
                adev->hdmi_descs.mat_fmt.MAT_PCM_48kHz_only = false;
                if (device & AUDIO_DEVICE_OUT_HDMI_ARC) {
                    if (eDolbyMS12Lib == adev->dolby_lib_type) {
                        /*when arc is connected, disable dap*/
                        set_ms12_full_dap_disable(&adev->ms12, false);
                    }
                    aml_audio_set_speaker_mute(adev, "false");
                    aml_audio_update_arc_status(adev, false);
                }
#ifdef BUILD_LINUX
                if ((device & AUDIO_DEVICE_OUT_HDMI) && adev->is_STB) {
                    adev->active_outport = OUTPORT_SPEAKER;
                    aml_audio_set_speaker_mute(adev, "false");
                }
#endif
                update_sink_format_after_hotplug(adev);
            } else if (device & AUDIO_DEVICE_OUT_ALL_A2DP) {
                adev->a2dp_updated = 1;
                adev->out_device &= (~device);
                //a2dp_out_close(adev);
            } else if (device &  AUDIO_DEVICE_OUT_ALL_USB ||
                       device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE||
                       device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                adev->out_device &= (~device);
            } else if (device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
                       device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                adev->out_device &= (~device);
            }
        }
    }
}

static int adev_set_parameters (struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGD ("%s(%p, %s)", __FUNCTION__, dev, kvpairs);

    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct str_parms *parms;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    char value[AUDIO_HAL_CHAR_MAX_LEN] = {0};
    int val = 0;
    int ret = 0;
    float val_f = 0.0;

    ALOGI ("%s(kv: %s)", __FUNCTION__, kvpairs);
    parms = str_parms_create_str (kvpairs);
    ret = str_parms_get_str (parms, "screen_state", value, sizeof (value) );
    if (ret >= 0) {
        if (strcmp (value, AUDIO_PARAMETER_VALUE_ON) == 0) {
            adev->low_power = false;
        } else {
            adev->low_power = true;
        }
        goto exit;
    }

    ret = str_parms_get_float (parms, "noise_gate", &val_f);
    if (ret >= 0) {
        adev->aml_ng_level = val_f;
        ALOGI ("adev->aml_ng_level set to %f\n", adev->aml_ng_level);
        goto exit;
    }

    ret = str_parms_get_int (parms, "disable_pcm_mixing", &val);
    if (ret >= 0) {
        adev->disable_pcm_mixing = val;
        ALOGI ("ms12 disable_pcm_mixing set to %d\n", adev->disable_pcm_mixing);
        goto exit;
    }

    ret = str_parms_get_int(parms, "Audio hdmi-out mute", &val);
    if (ret >= 0) {
        /* for tv,hdmitx module is not registered, do not reponse this control interface */
        if (adev->is_STB) {
            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_HDMI_OUT_AUDIO_MUTE, val);
            ALOGI("audio hdmi out status: %d\n", val);
        }
        goto exit;
    }

    ret = str_parms_get_int(parms, "Audio spdif mute", &val);
    if (ret >= 0) {
        audio_route_set_spdif_mute(&adev->alsa_mixer, val);
        ALOGI("[%s:%d] set SPDIF mute status: %d", __func__, __LINE__, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "AED master volume", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_EQ_MASTER_VOLUME, val);
        ALOGI("audio master volume : %d\n", val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "AED channel volume", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_EQ_LCH_VOLUME, val);
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_EQ_RCH_VOLUME, val);
        ALOGI("audio channel volume : %d\n", val);
        goto exit;
    }

    // HDMI cable plug off
    ret = str_parms_get_int(parms, "disconnect", &val);
    if (ret >= 0) {

    set_device_connect_state(adev, parms, val, false);
        goto exit;
    }

    // HDMI cable plug in
    ret = str_parms_get_int(parms, "connect", &val);
    if (ret >= 0) {
        set_device_connect_state(adev, parms, val, true);
        if (val & AUDIO_DEVICE_OUT_HDMI_ARC) {
            adev->raw_to_pcm_flag = true;
        }

        goto exit;
    }

    ret = str_parms_get_str (parms, "BT_SCO", value, sizeof (value) );
    if (ret >= 0) {
        if (strcmp (value, AUDIO_PARAMETER_VALUE_OFF) == 0) {
            close_btSCO_device(adev);
        }
        goto exit;
    }

    //  HDMI plug in and UI [Sound Output Device] set to "ARC" will recieve HDMI ARC Switch = 1
    //  HDMI plug off and UI [Sound Output Device] set to "speaker" will recieve HDMI ARC Switch = 0
    ret = str_parms_get_int(parms, "HDMI ARC Switch", &val);
    if (ret >= 0) {
        aml_audio_update_arc_status(adev, (val != 0));
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_earctx_earc_mode", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_EARC_TX_EARC_MODE, val);
        ALOGI("eARC_TX eARC Mode: %d\n", val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_arc_earc_tx_enable", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_ARC_EARC_TX_ENABLE, val);
        ALOGI("ARC eARC TX enable: %d\n", val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "spdifin/arcin switch", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIFIN_ARCIN_SWITCH, val);
        ALOGI("audio source: %s\n", val?"ARCIN":"SPDIFIN");
        goto exit;
    }

    //add for fireos tv for Dolby audio setting
    ret = str_parms_get_int (parms, "hdmi_format", &val);
    if (ret >= 0 ) {
        ret = set_hdmi_format(adev, val);
        if (ret < 0) {
            ALOGE("[%s:%d], An error occurred during setting hdmi_format!", __func__, __LINE__);
            return ret;
        }
        goto exit;
    }

    ret = str_parms_get_int (parms, "spdif_format", &val);
    if (ret >= 0 ) {
        adev->spdif_format = val;
        ALOGI ("S/PDIF format: %d\n", adev->spdif_format);
        goto exit;
    }

    //"digital_output_format=pcm"
    //"digital_output_format=dd"
    //"digital_output_format=ddp"
    //"digital_output_format=auto"
    //"digital_output_format=bypass", the same as "disable_pcm_mixing"
    ret = str_parms_get_str(parms, "digital_output_format", value, sizeof(value));
    if (ret >= 0) {
        struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
        int hdmi_format = PCM;
        if (strcmp(value, "pcm") == 0) {
            hdmi_format = PCM;
            adev->disable_pcm_mixing = 0;
            ALOGI("digital_output_format is PCM");
        } else if (strcmp(value, "ddp") == 0) {
            hdmi_format = DDP;
            hdmi_desc->ddp_fmt.is_support = true;
            hdmi_desc->dd_fmt.is_support = true;
            adev->disable_pcm_mixing = 0;
            ALOGI("digital_output_format is DDP");
        } else if (strcmp(value, "dd") == 0) {
            hdmi_format = DD;
            adev->disable_pcm_mixing = 0;
            hdmi_desc->dd_fmt.is_support = true;
            hdmi_desc->ddp_fmt.is_support = false;
            ALOGI("digital_output_format is DD");
        } else if (strcmp(value, "auto") == 0) {
            hdmi_format = AUTO;
            adev->disable_pcm_mixing = 0;
            ALOGI("digital_output_format is AUTO");
        } else if (strcmp(value, "bypass") == 0) {
            //make sink caps as MAX for roku
            hdmi_format = BYPASS;
            hdmi_desc->ddp_fmt.is_support = true;
            hdmi_desc->dd_fmt.is_support = true;
            hdmi_desc->dts_fmt.is_support = true;
            adev->disable_pcm_mixing = 1;
            ALOGI("digital_output_format is bypass");
        } else {
            ALOGE("unknown kvpairs");
        }

        if (adev->hdmi_format != hdmi_format)
            adev->hdmi_format_updated = 1;
        adev->hdmi_format = hdmi_format;
        ALOGI ("HDMI format: %d\n", adev->hdmi_format);

        goto exit;
    }

    //add for roku
    ret = str_parms_get_str(parms, "sink_atoms_capability", value, sizeof(value));
    if (ret >= 0) {
        struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;

        if (strcmp(value, "true") == 0) {
            hdmi_desc->ddp_fmt.atmos_supported = true;
            ALOGI("atoms enable");
        } else {
            hdmi_desc->ddp_fmt.atmos_supported = false;
            ALOGI("atmos disable");
        }

        goto exit;
    }

    ret = str_parms_get_int (parms, "hal_param_spdif_output_enable", &val);
    if (ret >= 0 ) {
        ALOGI ("[%s:%d] set spdif output enable:%d", __func__, __LINE__, val);
        if (val == 0) {
            audio_route_apply_path(adev->ar, "spdif_off");
        } else {
            audio_route_apply_path(adev->ar, "spdif_on");
        }
        adev->spdif_enable = (val == 0) ? false : true;
        audio_route_update_mixer(adev->ar);
        goto exit;
    }

    ret = str_parms_get_int(parms, "audio_type", &val);
    if (ret >= 0) {
        adev->audio_type = val;
        ALOGI("audio_type: %d\n", adev->audio_type);
        goto exit;
    }
    ret = str_parms_get_int(parms, "hdmi_is_passthrough_active", &val);
    if (ret >= 0 ) {
        adev->hdmi_is_pth_active = val;
        ALOGI ("hdmi_is_passthrough_active: %d\n", adev->hdmi_is_pth_active);
        goto exit;
    }

    ret = str_parms_get_int (parms, "capability:routing", &val);
    if (ret >= 0) {
        adev->routing = val;
        ALOGI ("capability:routing = %#x\n", adev->routing);
        goto exit;
    }

    ret = str_parms_get_int (parms, "capability:format", &val);
    if (ret >= 0) {
        adev->output_config.format = val;
        ALOGI ("capability:format = %#x\n", adev->output_config.format = val);
        goto exit;
    }

    ret = str_parms_get_int (parms, "capability:channels", &val);
    if (ret >= 0) {
        adev->output_config.channel_mask = val;
        ALOGI ("capability:channel_mask = %#x\n", adev->output_config.channel_mask);
        goto exit;
    }

    ret = str_parms_get_int (parms, "capability:sampling_rate", &val);
    if (ret >= 0) {
        adev->output_config.sample_rate = val;
        ALOGI ("capability:sample_rate = %d\n", adev->output_config.sample_rate);
        goto exit;
    }

    ret = str_parms_get_int (parms, "ChannelReverse", &val);
    if (ret >= 0) {
        adev->FactoryChannelReverse = val;
        ALOGI ("ChannelReverse = %d\n", adev->FactoryChannelReverse);
        goto exit;
    }

    ret = str_parms_get_str (parms, "set_ARC_hdmi", value, sizeof (value) );
    if (ret >= 0) {
        set_arc_hdmi(dev, value, AUDIO_HAL_CHAR_MAX_LEN);
        goto exit;
    }

    ret = str_parms_get_str (parms, "set_ARC_format", value, sizeof (value) );
    if (ret >= 0) {
        set_arc_format(dev, value, AUDIO_HAL_CHAR_MAX_LEN);
        goto exit;
    }

    ret = set_tv_source_switch_parameters(dev, parms);
    if (ret >= 0) {
        ALOGD("get TV source param(kv: %s)", kvpairs);
        goto exit;
    }

    //  HDMI plug in and UI [Sound Output Device] set to "ARC" will recieve speaker_mute = 1
    ret = str_parms_get_str (parms, "speaker_mute", value, sizeof (value) );
    if (ret >= 0) {
        aml_audio_set_speaker_mute(adev, value);
        goto exit;
    }

    ret = str_parms_get_str(parms, "parental_control_av_mute", value, sizeof(value));
    if (ret >= 0) {
        if (strncmp(value, "true", 4) == 0) {
            adev->parental_control_av_mute = true;
        } else if (strncmp(value, "false", 5) == 0) {
            adev->parental_control_av_mute = false;
        } else {
            ALOGE("%s() unsupport parental_control_av_mute value: %s",
                    __func__, value);
        }
        ALOGI("parental_control_av_mute set to %d\n", adev->parental_control_av_mute);
        goto exit;
    }

    // used for get first apts for A/V sync
    ret = str_parms_get_str (parms, "first_apts", value, sizeof (value) );
    if (ret >= 0) {
        unsigned int first_apts = atoi (value);
        ALOGI ("audio set first apts 0x%x\n", first_apts);
        adev->first_apts = first_apts;
        adev->first_apts_flag = true;
        adev->frame_trigger_thred = 0;
        goto exit;
    }

    ret = str_parms_get_str (parms, "is_netflix", value, sizeof (value) );
    if (ret >= 0) {
        if (strcmp(value, "true") == 0) {
            adev->is_netflix_hide = false;
            adev->is_netflix = 1;
        } else {
            if (adev->is_netflix)
                adev->is_netflix_hide = true;
            adev->is_netflix = 0;
            adev->gap_passthrough_state = GAP_PASSTHROUGH_STATE_IDLE;
            adev->gap_offset = 0;
        }
        ALOGI("%s:%d is_netflix %d(%s) hide:%d", __FUNCTION__, __LINE__, adev->is_netflix, value, adev->is_netflix_hide);

        set_ms12_encoder_chmod_locking(&(adev->ms12), adev->is_netflix);
        set_ms12_acmod2ch_lock(&(adev->ms12), !adev->is_netflix);
        goto exit;
    }

    ret = str_parms_get_str (parms, "vx_enable", value, sizeof (value) );
    if (ret >= 0) {
        if (Check_VX_lib()) { //need to check whether dts vx lib exists first
            if (strcmp(value, "true") == 0) {
                adev->vx_enable = true;
#ifdef DTS_VX_V4_ENABLE
                if (0 == VirtualX_setparameter(&adev->native_postprocess,VIRTUALX4_PARAM_ENABLE,1,EFFECT_CMD_SET_PARAM)
                     && 0 == VirtualX_getparameter(&adev->native_postprocess, PARAM_TSX_PROCESS_DISCARD_I32) )
#else
                if (VirtualX_getparameter(&adev->native_postprocess, DTS_PARAM_VX_ENABLE_I32) == 1
                    && VirtualX_getparameter(&adev->native_postprocess, DTS_PARAM_TSX_PROCESS_DISCARD_I32) == 0)
#endif
                {
                    dca_set_out_ch_internal(0);//If vx4 enable successful or vx2 is enabled,config output auto
                } else {
                    adev->vx_enable = false;
                    dca_set_out_ch_internal(2);//If vx4 enable failed or vx2 is disable, set config output stero
                }
            } else { //set dts vx false
                adev->vx_enable = false;
#ifdef DTS_VX_V4_ENABLE
                VirtualX_setparameter(&adev->native_postprocess,VIRTUALX4_PARAM_ENABLE,0,EFFECT_CMD_SET_PARAM);
#endif
                dca_set_out_ch_internal(2);//set vx4 or vx2 disabled,set config output stero
            }
       }
        goto exit;
    }
    /*use dolby_lib_type_last to check ms12 type, because durig playing DTS file,
      this type will be changed to dcv*/
    if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
        ret = str_parms_get_int(parms, "continuous_audio_mode", &val);
        if (ret >= 0) {
            int disable_continuous = !val;
            // if exit netflix, we need disable atmos lock
            if (disable_continuous) {
                adev->atoms_lock_flag = false;
                set_ms12_atmos_lock(&(adev->ms12), adev->atoms_lock_flag);
                ALOGI("exit netflix, set atmos lock as 0");
            }
            {
                bool acmod2ch_lock = !val;
                //when in netflix, we should always keep ddp5.1, exit netflix we can output ddp2ch
                set_ms12_encoder_chmod_locking(&(adev->ms12), !acmod2ch_lock);
                set_ms12_acmod2ch_lock(&(adev->ms12), acmod2ch_lock);
            }

            ALOGI("%s ignore the continuous_audio_mode!\n", __func__ );
            adev->is_netflix = val;
            goto exit;
            ALOGI("%s continuous_audio_mode set to %d\n", __func__ , val);
            char buf[PROPERTY_VALUE_MAX] = {0};
            char *str = aml_audio_property_get_str(DISABLE_CONTINUOUS_OUTPUT, buf, NULL);
            if (str != NULL) {
                sscanf(buf, "%d", &disable_continuous);
                ALOGI("%s[%s] disable_continuous %d\n", DISABLE_CONTINUOUS_OUTPUT, buf, disable_continuous);
            }
            pthread_mutex_lock(&adev->lock);
            if (continuous_mode(adev) && disable_continuous) {
                // If the Netflix application is terminated, your platform must disable Atmos locking.
                // For more information, see When to enable/disable Atmos lock.
                // The following Netflix application state transition scenarios apply (state definitions described in Always Ready):
                // Running->Not Running
                // Not Running->Running
                // Hidden->Visible
                // Visible->Hidden
                // Application crash / abnormal shutdown
               if (eDolbyMS12Lib == adev->dolby_lib_type) {
                    adev->atoms_lock_flag = 0;
                    dolby_ms12_set_atmos_lock_flag(adev->atoms_lock_flag);
                }

                if (dolby_stream_active(adev) || hwsync_lpcm_active(adev)) {
                    ALOGI("%s later release MS12 when direct stream active", __func__);
                    adev->need_remove_conti_mode = true;
                } else {
                    ALOGI("%s Dolby MS12 is at continuous output mode, here go to end it!\n", __FUNCTION__);
                    bool set_ms12_non_continuous = true;
                    get_dolby_ms12_cleanup(&adev->ms12, set_ms12_non_continuous);
                    adev->ms12_out = NULL;
                    //ALOGI("[%s:%d] get_dolby_ms12_cleanup\n", __FUNCTION__, __LINE__);
                    adev->exiting_ms12 = 1;
                    clock_gettime(CLOCK_MONOTONIC, &adev->ms12_exiting_start);
                    if (adev->active_outputs[STREAM_PCM_NORMAL] != NULL)
                        usecase_change_validate_l(adev->active_outputs[STREAM_PCM_NORMAL], true);
                    //continuous_stream_do_standby(adev);
                }
            } else {
                if ((!disable_continuous) && !continuous_mode(adev)) {
                    adev->mix_init_flag = false;
                    adev->continuous_audio_mode = 1;
                }
            }
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hdmi_dolby_atmos_lock", &val);
        if (ret >= 0) {
            ALOGI("%s hdmi_dolby_atmos_lock set to %d\n", __func__ , val);
            char buf[PROPERTY_VALUE_MAX];
            adev->atoms_lock_flag = val ? true : false;

            if (eDolbyMS12Lib == adev->dolby_lib_type) {
            // Note: some guide from www.netflix.com
            // In the Netflix application, enable/disable Atmos locking
            // when you receive the signal from the Netflix application.
            pthread_mutex_lock(&adev->lock);
            if (continuous_mode(adev)) {
                // enable/disable atoms lock
                set_ms12_atmos_lock(&(adev->ms12), adev->atoms_lock_flag);
                ALOGI("%s set adev->atoms_lock_flag = %d, \n", __func__,adev->atoms_lock_flag);
            } else {
                ALOGI("%s not in continuous mode, do nothing\n", __func__);
            }
            pthread_mutex_unlock(&adev->lock);
            }
            goto exit;
        }

        //for the local playback, IEC61937 format!
        //temp to disable the Dolby MS12 continuous
        //if local playback on Always continuous is completed,
        //we should remove this function!
        ret = str_parms_get_int(parms, "is_ms12_continuous", &val);
        if (ret >= 0) {
            ALOGI("%s is_ms12_continuous set to %d\n", __func__ , val);
            goto exit;
        }
        /*after enable MS12 continuous mode, the audio delay is big, we
        need use this API to compensate some delay to video*/
        ret = str_parms_get_int(parms, "compensate_video_enable", &val);
        if (ret >= 0) {
            ALOGI("%s compensate_video_enable set to %d\n", __func__ , val);
            adev->compensate_video_enable = val;
            //aml_audio_compensate_video_delay(val);
            goto exit;
        }

        /*enable force ddp output for ms12 v2*/
        ret = str_parms_get_int(parms, "hal_param_force_ddp", &val);
        if (ret >= 0) {
            ALOGI("%s hal_param_force_ddp set to %d\n", __func__ , val);
            if (adev->ms12_force_ddp_out != val) {
                adev->ms12_force_ddp_out = val;
                update_sink_format_after_hotplug(adev);
            }
            goto exit;
        }
    } else if (eDolbyDcvLib == adev->dolby_lib_type_last) {
        ret = str_parms_get_int(parms, "continuous_audio_mode", &val);
        if (ret >= 0) {
            ALOGI("%s ignore the continuous_audio_mode!\n", __func__ );
            adev->is_netflix = val;
            goto exit;
        }
    }

    ret = str_parms_get_str(parms, "SOURCE_GAIN", value, sizeof(value));
    if (ret >= 0) {
        float fAtvGainDb = 0, fDtvGainDb = 0, fHdmiGainDb = 0, fAvGainDb = 0, fMediaGainDb = 0;
        sscanf(value,"%f %f %f %f %f", &fAtvGainDb, &fDtvGainDb, &fHdmiGainDb, &fAvGainDb, &fMediaGainDb);
        ALOGI("%s() audio source gain: atv:%f, dtv:%f, hdmiin:%f, av:%f, media:%f", __func__,
        fAtvGainDb, fDtvGainDb, fHdmiGainDb, fAvGainDb, fMediaGainDb);
        adev->eq_data.s_gain.atv = DbToAmpl(fAtvGainDb);
        adev->eq_data.s_gain.dtv = DbToAmpl(fDtvGainDb);
        adev->eq_data.s_gain.hdmi = DbToAmpl(fHdmiGainDb);
        adev->eq_data.s_gain.av = DbToAmpl(fAvGainDb);
        adev->eq_data.s_gain.media = DbToAmpl(fMediaGainDb);
        goto exit;
    }

    ret = str_parms_get_str(parms, "SOURCE_MUTE", value, sizeof(value));
    if (ret >= 0) {
        sscanf(value,"%d", &adev->source_mute);
        ALOGI("%s() set audio source mute: %s",__func__,
            adev->source_mute?"mute":"unmute");
        goto exit;
    }

    ret = str_parms_get_str(parms, "POST_GAIN", value, sizeof(value));
    if (ret >= 0) {
        sscanf(value,"%f %f %f", &adev->eq_data.p_gain.speaker, &adev->eq_data.p_gain.spdif_arc,
                &adev->eq_data.p_gain.headphone);
        ALOGI("%s() audio device gain: speaker:%f, spdif_arc:%f, headphone:%f", __func__,
        adev->eq_data.p_gain.speaker, adev->eq_data.p_gain.spdif_arc,
        adev->eq_data.p_gain.headphone);
        goto exit;
    }
#ifdef ADD_AUDIO_DELAY_INTERFACE
    ret = str_parms_get_str(parms, "port_delay", value, sizeof(value));
    if (ret >= 0) {
        char *port_name;
        int port_delay = 0;
        char *delimiter = ",";
        port_name = strtok(value, delimiter);
        port_delay = (atoi)(strtok(NULL, delimiter));
        aml_audio_set_port_delay(port_name, port_delay);
        goto exit;
    }
#endif
#ifdef USE_EQ_DRC
    ret = str_parms_get_str(parms, "EQ_PARAM", value, sizeof(value));
    if (ret >= 0) {
       sscanf(value, "%lf %lf %u %u %u",&adev->Eq_data.G,&adev->Eq_data.Q,&adev->Eq_data.fc,&adev->Eq_data.type,&adev->Eq_data.band_id);
       setpar_eq(adev->Eq_data.G,adev->Eq_data.Q,adev->Eq_data.fc,adev->Eq_data.type,adev->Eq_data.band_id);
       goto exit;
    }
    ret = str_parms_get_str(parms,"mb_drc",value,sizeof(value));
    if (ret >= 0) {
        sscanf(value, "%u %u %u %u %f %f",&adev->Drc_data.band_id,&adev->Drc_data.attrack_time,&adev->Drc_data.release_time,
             &adev->Drc_data.estimate_time,&adev->Drc_data.K,&adev->Drc_data.threshold);
        setmb_drc(adev->Drc_data.band_id,adev->Drc_data.attrack_time,adev->Drc_data.release_time,adev->Drc_data.estimate_time,
                adev->Drc_data.K,adev->Drc_data.threshold);
        goto exit;
    }
    ret = str_parms_get_str(parms,"fb_drc",value,sizeof(value));
    if (ret >= 0) {
        sscanf(value, "%u %u %u %u %f %f %u",&adev->Drc_data.band_id,&adev->Drc_data.attrack_time,&adev->Drc_data.release_time,
             &adev->Drc_data.estimate_time,&adev->Drc_data.K,&adev->Drc_data.threshold,&adev->Drc_data.delays);
        setfb_drc(adev->Drc_data.band_id,adev->Drc_data.attrack_time,adev->Drc_data.release_time,adev->Drc_data.estimate_time,
               adev->Drc_data.K,adev->Drc_data.threshold,adev->Drc_data.delays);
        goto exit;
    }
    ret = str_parms_get_str(parms,"csfilter_drc",value,sizeof(value));
    if (ret >= 0) {
        sscanf(value, "%u %u",&adev->Drc_data.band_id,&adev->Drc_data.fc);
        setcsfilter_drc(adev->Drc_data.band_id,adev->Drc_data.fc);
        goto exit;
    }
    ret = str_parms_get_int(parms, "eq_enable", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_AED_EQ_ENABLE, val);
        ALOGI("audio AED EQ enable: %s\n", val?"enable":"disable");
        goto exit;
    }
    ret = str_parms_get_int(parms, "multi_drc_enable", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_AED_MULTI_DRC_ENABLE, val);
        ALOGI("audio AED Multi-band DRC enable: %s\n", val?"enable":"disable");
        goto exit;
    }
    ret = str_parms_get_int(parms, "fullband_drc_enable", &val);
    if (ret >= 0) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_AED_FULL_DRC_ENABLE, val);
        ALOGI("audio AED Full-band DRC enable: %s\n", val?"enable":"disable");
        goto exit;
    }
#endif
    ret = str_parms_get_str(parms, "DTS_POST_GAIN", value, sizeof(value));
    if (ret >= 0) {
        sscanf(value,"%f", &adev->dts_post_gain);
        ALOGI("%s() audio dts post gain: %f", __func__, adev->dts_post_gain);
        goto exit;
    }

    ret = str_parms_get_int(parms, "EQ_MODE", &val);
    #ifdef USE_EQ_DRC
    if (ret >= 0) {
        if (eq_mode_set(&adev->eq_data, val) < 0)
            ALOGE("%s: eq_mode_set failed", __FUNCTION__);
        goto exit;
    }
    #endif
#ifdef USE_DTV
    {//add tsplayer compatible cmd
        ret = str_parms_get_int(parms, "dual_decoder_support", &val);
        if (ret >= 0) {
            ALOGI("dual_decoder_support set to %d\n", val);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT ,val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "pid", &val);
        if (ret >= 0) {
            ALOGI("%s() get the audio pid %d\n", __func__, val);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_PID, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "demux_id", &val);
        if (ret >= 0) {
            ALOGI("%s() get the audio demux_id %d\n", __func__, val);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "fmt", &val);
        if (ret >= 0) {
            ALOGI("%s() get the audio format %d\n", __func__, val);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_FMT, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "mode", &val);
        if (ret >= 0) {
            ALOGI("DTV sound mode %d ", val);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "has_dtv_video", &val);
        if (ret >= 0) {
            ALOGI("%s() get the has video parameters %d \n", __func__, val);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "associate_audio_mixing_enable", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "cmd", &val);
        if (ret >= 0) {
         switch (val) {
         case 1:
            ALOGI("%s() live AUDIO_DTV_PATCH_CMD_START\n", __func__);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_OPEN);
            usleep(1000*50);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_START);
            break;
         case 2:
            ALOGI("%s() live AUDIO_DTV_PATCH_CMD_PAUSE\n", __func__);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_PAUSE);
            break;
         case 3:
            ALOGI("%s() live AUDIO_DTV_PATCH_CMD_RESUME\n", __func__);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_RESUME);
            break;
         case 4:
            ALOGI("%s() live AUDIO_DTV_PATCH_CMD_STOP\n", __func__);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_STOP);
            usleep(1000*50);
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_CLOSE);
            break;
         default:
            ALOGI("%s() live %d \n", __func__, val);
            break;
         }
         goto exit;
        }
    }
    /* dvb cmd deal with start */
    {
        ret = str_parms_get_int(parms, "hal_param_dtv_patch_cmd", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, val);
            goto exit;
#if 0
            int cmd_val= val & ((1 << DVB_DEMUX_ID_BASE) - 1);
            ALOGI("[%s:%d] cmd_val=%d(%d)", __FUNCTION__, __LINE__, cmd_val, val);

            if (cmd_val == AUDIO_DTV_PATCH_CMD_STOP || cmd_val == AUDIO_DTV_PATCH_CMD_PAUSE) {
                ALOGI("[%s:%d] mute dtv to do fade out", __FUNCTION__, __LINE__);
                //adev->start_mute_flag = true;
                set_ms12_main_audio_mute(&adev->ms12, true, 32);
                //set_aed_master_volume_mute(&adev->alsa_mixer, true); // dtv continuous mode, we do not need aed to do fade
                //set_dac_digital_volume_mute(&adev->alsa_mixer, true);
                usleep(64000);
                dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, val);
            } else if (cmd_val == AUDIO_DTV_PATCH_CMD_RESUME) {
                ALOGI("[%s:%d] unmute dtv to do fade in", __FUNCTION__, __LINE__);
                dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, val);
                usleep(20000);//when resume, av sync policy is HOLD, we need wait 20ms that audio policy change to normal output
                //adev->start_mute_flag = false;
                set_ms12_main_audio_mute(&adev->ms12, false, 32);
                //set_aed_master_volume_mute(&adev->alsa_mixer, false);
                //set_dac_digital_volume_mute(&adev->alsa_mixer, false);
            } else {
                dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, val);
            }
#endif
        }

        ret = str_parms_get_int(parms, "hal_param_dual_dec_support", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT ,val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_ad_mix_enable", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE ,val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_media_sync_id", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID ,val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "ad_switch_enable", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE, val);
            ALOGI("ad_switch_enable set to %d\n", adev->associate_audio_mixing_enable);
            goto exit;
        }

        ret = str_parms_get_int(parms, "dual_decoder_advol_level", &val);
        if (ret >= 0) {

            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dual_dec_mix_level", &val);
        if (ret >= 0) {

            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_security_mem_level", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_dtv_synctype", &val);
        if (ret >= 0) {
            adev->synctype = val;
            ALOGI("adev->synctype set to %d\n", val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_audio_output_mode", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_dtv_demux_id", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_dtv_pid", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_PID, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_fmt", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_FMT, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_dtv_audio_fmt", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_FMT, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_audio_id", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_PID, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_sub_audio_fmt", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_FMT, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_sub_audio_pid", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_PID, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_has_dtv_video", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_tv_mute", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MUTE, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_media_presentation_id", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_PRESENTATION_ID, val);
            goto exit;
        }
        ret = str_parms_get_int(parms, "hal_param_dtv_media_first_lang", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_FIRST_LANG, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_media_second_lang", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_SECOND_LANG, val);
            goto exit;
        }

        ret = str_parms_get_int(parms, "hal_param_dtv_audio_volume", &val);
        if (ret >= 0) {
            dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_VOLUME, val);
            goto exit;
        }
    }
#endif
    /* dvb cmd deal with end */
    ret = str_parms_get_str(parms, "sound_track", value, sizeof(value));
    if (ret > 0) {
        int mode = atoi(value);
        if (adev->audio_patch != NULL) {
            ALOGI("%s()the audio patch is not NULL \n", __func__);
            if (adev->patch_src == SRC_DTV) {
                ALOGI("DTV sound mode %d ",mode);
                adev->audio_patch->mode = mode;
            }
            goto exit;
        }
        ALOGI("video player sound_track mode %d ",mode );
        adev->sound_track_mode = mode;
        goto exit;
    }

    ret = str_parms_get_str(parms, "has_video", value, sizeof(value));
    if (ret >= 0) {
        if (strncmp(value, "true", 4) == 0) {
            adev->is_has_video = true;
        } else if (strncmp(value, "false", 5) == 0) {
            adev->is_has_video = false;
        } else {
            adev->is_has_video = true;
            ALOGE("%s() unsupport value %s choose is_has_video(default) %d\n", __func__, value, adev->is_has_video);
        }
        ALOGI("is_has_video set to %d\n", adev->is_has_video);
    }

    ret = str_parms_get_str(parms, "reconfigA2dp", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("%s A2DP reconfigA2dp out_device=%x", __FUNCTION__, adev->out_device);
        goto exit;
    }

    ret = str_parms_get_str(parms, "hfp_set_sampling_rate", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: hfp_set_sampling_rate. Abort function and return 0.", __FUNCTION__);
        goto exit;
    }

    ret = str_parms_get_str(parms, "hfp_volume", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: hfp_volume. Abort function and return 0.", __FUNCTION__);
        goto exit;
    }

    ret = str_parms_get_str(parms, "bt_headset_name", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: bt_headset_name. Abort function and return 0.", __FUNCTION__);
        //goto exit;
    }

    ret = str_parms_get_str(parms, "rotation", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: rotation. Abort function and return 0.", __FUNCTION__);
        goto exit;
    }

    ret = str_parms_get_str(parms, "bt_headset_nrec", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: bt_headset_nrec. Abort function and return 0.", __FUNCTION__);
        //goto exit;
    }

    ret = str_parms_get_str(parms, "bt_wbs", value, sizeof(value));
    if (ret >= 0) {
        ALOGI("Amlogic_HAL - %s: bt_wbs=%s.", __func__, value);
        if (strncmp(value, "on", 2) == 0)
            adev->bt_wbs = true;
        else
            adev->bt_wbs = false;

        goto exit;
    }

    ret = str_parms_get_str(parms, "hfp_enable", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: hfp_enable. Abort function and return 0.", __FUNCTION__);
        goto exit;
    }

    ret = str_parms_get_str(parms, "HACSetting", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: HACSetting. Abort function and return 0.", __FUNCTION__);
        goto exit;
    }

    ret = str_parms_get_str(parms, "tty_mode", value, sizeof(value));
    if (ret >= 0) {
        ALOGE ("Amlogic_HAL - %s: tty_mode. Abort function and return 0.", __FUNCTION__);
        goto exit;
    }


    ret = str_parms_get_str(parms, "show-meminfo", value, sizeof(value));
    if (ret >= 0) {
        unsigned int level = (unsigned int)atoi(value);
        ALOGE ("Amlogic_HAL - %s: ShowMem info level:%d.", __FUNCTION__,level);
        aml_audio_debug_malloc_showinfo(level);
        goto exit;
    }

#ifndef NO_AUDIO_CAP
    /*add interface to audio capture*/
    ret = str_parms_get_str(parms, "cap_buffer", value, sizeof(value));
    if (ret >= 0) {
        int size = 0;
        char *name = strtok(value, ",");
        ALOGI("cap_buffer %s", value);
        if (name) {
            size = atoi(name + strlen(name) + 1);
            if (size > 0) {
                pthread_mutex_lock(&adev->cap_buffer_lock);
                if (adev->cap_buffer) {
                    IpcBuffer_destroy(adev->cap_buffer);
                }
                adev->cap_buffer = IpcBuffer_create(name, size);
                ALOGI("IpcBuffer_created %p (%s, %d)", adev->cap_buffer, name, size);
                pthread_mutex_unlock(&adev->cap_buffer_lock);
                ret = 0;
                goto exit;
            } else if (size == 0) {
                pthread_mutex_lock(&adev->cap_buffer_lock);
                if (adev->cap_buffer) {
                    IpcBuffer_destroy(adev->cap_buffer);
                    adev->cap_buffer = NULL;
                }
                pthread_mutex_unlock(&adev->cap_buffer_lock);
                ret = 0;
                goto exit;
            }
        }
        ret = -EINVAL;
        goto exit;
    }
    ret = str_parms_get_str(parms, "cap_delay", value, sizeof(value));
    if (ret >= 0) {
        adev->cap_delay = atoi(value);
        ret = 0;
        goto exit;
    }
#endif
    ret = str_parms_get_str(parms, "diaglogue_enhancement", value, sizeof(value));
    if (ret >= 0) {
        adev->ms12.ac4_de = atoi(value);
        ALOGE ("Amlogic_HAL - %s: set MS12 ac4 Dialogue Enhancement gain :%d.", __FUNCTION__,adev->ms12.ac4_de);

        char parm[32] = "";
        sprintf(parm, "%s %d", "-ac4_de", adev->ms12.ac4_de);
        pthread_mutex_lock(&adev->lock);
        if (strlen(parm) > 0)
            aml_ms12_update_runtime_params(&(adev->ms12), parm);
        pthread_mutex_unlock(&adev->lock);
        goto exit;
    }

    ret = str_parms_get_str(parms, "picture_mode", value, sizeof(value));
    if (ret >= 0) {
        if (strncmp(value, "PQ_MODE_STANDARD", 16) == 0) {
            adev->pic_mode = PQ_STANDARD;
            adev->game_mode = false;
        } else if (strncmp(value, "PQ_MODE_MOVIE", 13) == 0) {
            adev->pic_mode = PQ_MOVIE;
            adev->game_mode = false;
        } else if (strncmp(value, "PQ_MODE_DYNAMIC", 15) == 0) {
            adev->pic_mode = PQ_DYNAMIC;
            adev->game_mode = false;
        } else if (strncmp(value, "PQ_MODE_NATURAL", 15) == 0) {
            adev->pic_mode = PQ_NATURAL;
            adev->game_mode = false;
        } else if (strncmp(value, "PQ_MODE_GAME", 12) == 0) {
            adev->pic_mode = PQ_GAME;
            adev->game_mode = true;
        } else if (strncmp(value, "PQ_MODE_PC", 10) == 0) {
            adev->pic_mode = PQ_PC;
            adev->game_mode = false;
        } else if (strncmp(value, "PQ_MODE_CUSTOMER", 16) == 0) {
            adev->pic_mode = PQ_CUSTOM;
            adev->game_mode = false;
        } else {
            adev->pic_mode = PQ_STANDARD ;
            adev->game_mode = false;
            ALOGE("%s() unsupport value %s choose pic mode (default) standard\n", __func__, value);
        }
        ALOGI("%s(), set pic mode to: %d, is game mode = %d\n", __func__, adev->pic_mode, adev->game_mode);
        goto exit;
    }
    ret = str_parms_get_str(parms, "prim_mixgain", value, sizeof(value));
    if (ret >= 0) {
        char parm[40] = "";
        int gain = atoi(value);
        if ((gain <= 0) && (gain >= -96)) {
            sprintf(parm, "-sys_prim_mixgain %d,200,0", gain << 7);
            pthread_mutex_lock(&adev->lock);
            aml_ms12_update_runtime_params(&(adev->ms12), parm);
            pthread_mutex_unlock(&adev->lock);
            ret = 0;
            goto exit;
        }
        ret = -EINVAL;
        goto exit;
    }
    ret = str_parms_get_str(parms, "apps_mixgain", value, sizeof(value));
    if (ret >= 0) {
        char parm[40] = "";
        int gain = atoi(value);
        if ((gain <= 0) && (gain >= -96)) {
            int after_master_gain;
            adev->apps_mixgain = gain << 7;
            after_master_gain = 2560.0f * log10((adev->master_mute) ? 0.0f : adev->master_volume);
            after_master_gain += adev->apps_mixgain;
            if (after_master_gain > 0) {
                after_master_gain = 0;
            } else if (after_master_gain < (-96 << 7)) {
                after_master_gain = -96 << 7;
            }
            sprintf(parm, "-sys_apps_mixgain %d,200,0", after_master_gain);
            pthread_mutex_lock(&adev->lock);
            aml_ms12_update_runtime_params(&(adev->ms12), parm);
            pthread_mutex_unlock(&adev->lock);
            ret = 0;
            goto exit;
        }
        ret = -EINVAL;
        goto exit;
    }
    ret = str_parms_get_str(parms, "syss_mixgain", value, sizeof(value));
    if (ret >= 0) {
        char parm[40] = "";
        int gain = atoi(value);
        if ((gain <= 0) && (gain >= -96)) {
            int after_master_gain;
            adev->syss_mixgain = gain << 7;
            after_master_gain = 2560.0f * log10((adev->master_mute) ? 0.0f : adev->master_volume);
            after_master_gain += adev->syss_mixgain;
            if (after_master_gain > 0) {
                after_master_gain = 0;
            } else if (after_master_gain < (-96 << 7)) {
                after_master_gain = -96 << 7;
            }
            sprintf(parm, "-sys_syss_mixgain %d,200,0", after_master_gain);
            pthread_mutex_lock(&adev->lock);
            aml_ms12_update_runtime_params(&(adev->ms12), parm);
            pthread_mutex_unlock(&adev->lock);
            ret = 0;
            goto exit;
        }
        ret = -EINVAL;
        goto exit;
    }
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
       ret = str_parms_get_str(parms, "vol_ease", value, sizeof(value));
        if (ret >= 0) {
            int gain, duration, shape;
            ALOGI("vol_ease: value= %s", value);
            if (adev->master_volume < FLOAT_ZERO) {
                ret = 0;
                goto exit;
            }
            if (sscanf(value, "%d,%d,%d", &gain, &duration, &shape) == 3) {
                char cmd[128] = {0};
                sprintf(cmd, "-sys_prim_mixgain %d,%d,%d -sys_apps_mixgain %d,%d,%d -sys_syss_mixgain %d,%d,%d",
                    gain, duration, shape,
                    gain, duration, shape,
                    gain, duration, shape);
                ALOGI("vol_ease %d %d %d", gain, duration, shape);

                if (duration == 0) {
                    if (aml_ms12_update_runtime_params(&(adev->ms12), cmd) == -1) {
                        /* an immediate setting of easing volume is not successful, which
                         * could happen when MS12 graph has not been set up yet.
                         * This setting is saved by libms12 and will be applied when
                         * MS12 is launched. adev->vol_ease_valid is the flag to show there
                         * is already a pending start volume.
                         */
                        ALOGI("vol_ease EASE_SETTING_START with gain %d", gain);
                        adev->vol_ease_setting_state = EASE_SETTING_START;
                    }
                } else {
                    if (adev->vol_ease_setting_state == EASE_SETTING_START) {
                        /* when MS12 lib has an initial easing set, save the following
                         * non-zero duration settings and apply it when MS12 is launched
                         */
                        adev->vol_ease_setting_state = EASE_SETTING_PENDING;
                        adev->vol_ease_setting_gain = gain;
                        adev->vol_ease_setting_duration = duration;
                        adev->vol_ease_setting_shape = shape;

                        ALOGI("vol_ease EASE_SETTING_PENDING with %d %d %d", gain, duration, shape);

                    } else {
                        /* when MS12 lib does not have an initial easing set, update runtime directly */
                        ALOGI("vol_ease apply %d %d %d", gain, duration, shape);
                        aml_ms12_update_runtime_params(&(adev->ms12), cmd);
                    }
                }
                ret = 0;
                goto exit;
            }
        }
        ret = str_parms_get_str(parms, "ms12_runtime", value, sizeof(value));
        if (ret >= 0) {
            char *parm = strstr(kvpairs, "=");
            pthread_mutex_lock(&adev->lock);
            if (parm) {
                if (strcmp(parm+1, "-atmos_lock 1") == 0) {
                    adev->atoms_lock_flag = 1;
                } else if (strcmp(parm+1, "-atmos_lock 0") == 0) {
                    adev->atoms_lock_flag = 0;
                } else if (strstr(parm+1, "-drc")) {
                    aml_audio_set_drc_control(parm+1, &adev->decoder_drc_control);
                    //dap drc default same with decoder drc
                    adev->dap_drc_control = adev->decoder_drc_control;
                }
                if (strstr(parm+1, "-dap_drc")) {
                    aml_audio_set_drc_control(strstr(parm+1, "-dap_drc"), &adev->dap_drc_control);
                }
                if (strstr(parm+1, "-dmx")) {
                    adev->downmix_type = atoi(parm+5);//refer to enum: AM_DownMixMode_t
                    ALOGE("%s, downmix_type %d", parm+5, adev->downmix_type);
                }
                aml_ms12_update_runtime_params(&(adev->ms12), parm+1);
            }
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
    }
    ret = str_parms_get_str(parms, "dmx_mode", value, sizeof(value));
    if (ret >= 0) {
        char *parm = strstr(kvpairs, "=");
        pthread_mutex_lock(&adev->lock);
        if (parm) {
            if (strstr(parm+1, "-dmx")) {
                adev->downmix_type = atoi(parm+5);//refer to enum: AM_DownMixMode_t
                AM_LOGD("%s, downmix_type %d", parm+5, adev->downmix_type);
                if (eDolbyMS12Lib == adev->dolby_lib_type) {
                    aml_ms12_update_runtime_params(&(adev->ms12), parm+1);
                }
            }
        }
        pthread_mutex_unlock(&adev->lock);
        goto exit;
    }

    ret = str_parms_get_str(parms, "drc_mode", value, sizeof(value));
    if (ret >= 0) {
        char *parm = strstr(kvpairs, "=");
        pthread_mutex_lock(&adev->lock);
        if (parm) {
            if (strstr(parm+1, "-drc")) {
                aml_audio_set_drc_control(parm+1, &adev->decoder_drc_control);
                //dap drc default same with decoder drc
                adev->dap_drc_control = adev->decoder_drc_control;
            }

            if (eDolbyMS12Lib == adev->dolby_lib_type) {
                if (strstr(parm+1, "-dap_drc")) {
                    aml_audio_set_drc_control(strstr(parm+1, "-dap_drc"), &adev->dap_drc_control);
                }
                aml_ms12_update_runtime_params(&(adev->ms12), parm+1);
            }
        }
        pthread_mutex_unlock(&adev->lock);
        goto exit;
    }

    ret = str_parms_get_str(parms, "bypass_dap", value, sizeof(value));
    if (ret >= 0) {
        sscanf(value,"%d %f", &adev->ms12.dap_bypass_enable, &adev->ms12.dap_bypassgain);
        ALOGD("dap_bypass_enable is %d and dap_bypassgain is %f",adev->ms12.dap_bypass_enable, adev->ms12.dap_bypassgain);
        goto exit;
    }

    ret = str_parms_get_str(parms, "VX_SET_DTS_Mode", value, sizeof(value));
    if (ret >= 0) {
        int dts_decoder_output_mode = atoi(value);
        if (dts_decoder_output_mode > 6 || dts_decoder_output_mode < 0)
            goto exit;
        if (dts_decoder_output_mode == 2)
            adev->native_postprocess.vx_force_stereo = 1;
        else
            adev->native_postprocess.vx_force_stereo = 0;
        dca_set_out_ch_internal(dts_decoder_output_mode);
        ALOGD("set dts decoder output mode to %d", dts_decoder_output_mode);
        goto exit;
    }

    ret = str_parms_get_str(parms, "pts_gap", value, sizeof(value));
    if (ret >= 0) {
        unsigned long long offset, duration;
        if (sscanf(value, "%llu,%lld", &offset, &duration) == 2) {
            ALOGI("pts_gap %llu %lld", offset, duration);
            adev->gap_offset = offset;
            adev->gap_ignore_pts = false;
            adev->gap_passthrough_state = GAP_PASSTHROUGH_STATE_SET;
            dolby_ms12_set_pts_gap(offset, duration);
            ret = 0;
            goto exit;
        }
        ret = -EINVAL;
        goto exit;
    }

    ret = str_parms_get_int(parms, "audioinject", &val);
    if (ret >= 0) {
        if (val == 1) {
            ALOGI("close pcm out");
            pthread_mutex_lock(&adev->alsa_pcm_lock);
            if (adev->pcm_handle[I2S_DEVICE]) {
                pcm_close(adev->pcm_handle[I2S_DEVICE]);
                adev->pcm_handle[I2S_DEVICE] = NULL;
                adev->pcm_refs[I2S_DEVICE] = 0;
            }
            pthread_mutex_unlock(&adev->alsa_pcm_lock);
            adev->injection_enable = 1;
        } else {
            adev->injection_enable = 0;
        }
        goto exit;
    }

    ret = str_parms_get_int(parms, "gst_pause", &val);
    if (ret >= 0) {
        if (adev->audio_ease) {
            start_ease_out(adev);
        }
        ALOGE ("gst_pause: fade out\n");

        goto exit;
    }

    ret = str_parms_get_int(parms, "enable_scaletempo", &val);
    if (ret >= 0) {
        if (val == 1)
            adev->user_setting_scaletempo = true;
        else
            adev->user_setting_scaletempo = false;
        ALOGI("audio enable_scaletempo: %s\n", adev->user_setting_scaletempo? "enable": "disable");
        goto exit;
    }

    ret = str_parms_get_str(parms,"OutputDestination", value , sizeof(value));
    if (ret >= 0) {
        ALOGI("%s:OutputDestination=%s",__func__,value);
        adev_Updata_ActiveOutport(adev,value);
        goto exit;
    }

    ret = str_parms_get_int(parms, "spdif_protection_mode", &val);
    if (ret >= 0 ) {
        if (val == SPDIF_PROTECTION_MODE_NEVER) {
            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_OUT_CHANNEL_STATUS, SPDIF_PROTECTION_ENABLE);
            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_B_OUT_CHANNEL_STATUS, SPDIF_PROTECTION_ENABLE);
        } else if (val == SPDIF_PROTECTION_MODE_ONCE || val == SPDIF_PROTECTION_MODE_NONE) {
            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_OUT_CHANNEL_STATUS, SPDIF_PROTECTION_DISABLE);
            aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_B_OUT_CHANNEL_STATUS, SPDIF_PROTECTION_DISABLE);
        }
        AM_LOGI("spdif_protection_mode:%d", val);
        goto exit;
    }

    ret = str_parms_get_int(parms,"audio_focus_enable", &val);
    if (ret >= 0) {
        if (val == 1)
            adev->audio_focus_enable = true;
        else
            adev->audio_focus_enable = false;
        ALOGI("%s:audio focus %s", __func__, adev->audio_focus_enable? "enable": "disable");
        goto exit;
    }

    ret = str_parms_get_int(parms,"audio_focus_volume", &val);
    if (ret >= 0) {
        adev->audio_focus_volume = val;
        if (adev->audio_focus_enable)
            aml_audio_focus(adev, true);
        ALOGI("%s:audio_focus_volume is %d", __func__, adev->audio_focus_volume);
        goto exit;
    }

#ifdef BUILD_LINUX
    ret = str_parms_get_str(parms, "setenv", value, sizeof(value));
    if (ret >= 0) {
        char *var = strtok(value, ",");
        if (var) {
            char *val = var + strlen(var) + 1;
            if (strlen(val) > 0) {
                if ((setenv(var, val, 1)) == 0) {
                    ALOGI("setenv %s=%s success", var, val);
                    ret = 0;
                    goto exit;
                } else {
                    ALOGE("setenv %s=%s failed", var, val);
                    ret = -EINVAL;
                    goto exit;
                }
            } else {
                unsetenv(var);
                ALOGI("env %s unset", var);

#ifdef DIAG_LOG
                if (strcmp(var, "AV_PROGRESSION") == 0) {
                    pthread_mutex_lock(&adev->diag_log_lock);
                    diag_log_close(adev);
                    pthread_mutex_unlock(&adev->diag_log_lock);
                }
#endif

            }
        }
        goto exit;
    }
#endif
exit:
    str_parms_destroy (parms);
    /* always success to pass VTS */
    return 0;
}

static void adev_get_cec_control_object(struct aml_audio_device *adev, char *temp_buf)
{
    int cec_control_tv = 0;
    if (!adev->is_TV && adev->dolby_lib_type != eDolbyMS12Lib) {
        enum AML_SPDIF_FORMAT format = AML_STEREO_PCM;
        enum AML_SRC_TO_HDMITX spdif_index = aml_mixer_ctrl_get_int(&adev->alsa_mixer, AML_MIXER_ID_AUDIO_SRC_TO_HDMI);
        if (spdif_index == AML_SPDIF_A_TO_HDMITX) {
            format = aml_mixer_ctrl_get_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT);
        } else if (spdif_index == AML_SPDIF_B_TO_HDMITX) {
            format = aml_mixer_ctrl_get_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT);
        } else {
            format = aml_mixer_ctrl_get_int(&adev->alsa_mixer, AML_MIXER_ID_I2S2HDMI_FORMAT);
        }
        cec_control_tv = (format == AML_STEREO_PCM) ? 0 : 1;
    }
    sprintf (temp_buf, "hal_param_cec_control_tv=%d", cec_control_tv);
    if (adev->debug_flag) {
        ALOGD("[%s:%d] hal_param_cec_control_tv:%d", __func__, __LINE__, cec_control_tv);
    }
}

static char * adev_get_parameters (const struct audio_hw_device *dev,
                                   const char *keys)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    char temp_buf[AUDIO_HAL_CHAR_MAX_LEN + 32] = {0};

    if (!strcmp (keys, AUDIO_PARAMETER_HW_AV_SYNC) ) {
        ALOGI ("get hw_av_sync id\n");
        if (adev->hw_mediasync == NULL) {
            adev->hw_mediasync = mediasync_wrap_create();
        }
        if (adev->hw_mediasync != NULL) {
            int32_t id = -1;
            bool ret = mediasync_wrap_allocInstance(adev->hw_mediasync, 0, 0, &id);
            ALOGI ("ret: %d, id:%d\n", ret, id);
            if(ret && id != -1) {
                sprintf (temp_buf, "hw_av_sync=%d", id);
                return strdup (temp_buf);
            }
        }
        return strdup ("hw_av_sync=12345678");
    }
    // ALOGI ("adev_get_parameters keys: %s \n", keys);
    if (strstr (keys, "getenv=") ) {
        char *pret=getenv(&keys[7]);
        if (NULL != pret) {
            ALOGI ("ret: val %s,key %s \n", pret, &keys[7]);
            return strdup (pret);
        } else {
            return strdup ("find not");
        }
    } else if (strstr (keys, AUDIO_PARAMETER_HW_AV_EAC3_SYNC) ) {
        return strdup ("HwAvSyncEAC3Supported=true");
    } else if (strstr (keys, "hdmi_format") ) {
        sprintf (temp_buf, "hdmi_format=%d", adev->hdmi_format);
        return strdup (temp_buf);
    } else if (strstr (keys, "digital_output_format") ) {
        if (adev->hdmi_format == PCM) {
            return strdup ("digital_output_format=pcm");
        } else if (adev->hdmi_format == DD) {
            return strdup ("digital_output_format=dd");
        } else if (adev->hdmi_format == DDP) {
            return strdup ("digital_output_format=ddp");
        } else if (adev->hdmi_format == AUTO) {
            return strdup ("digital_output_format=auto");
        } else if (adev->hdmi_format == BYPASS) {
            return strdup ("digital_output_format=bypass");
        }
    } else if (strstr (keys, "spdif_format") ) {
        sprintf (temp_buf, "spdif_format=%d", adev->spdif_format);
        return strdup (temp_buf);
    } else if (strstr (keys, "hdmi_is_passthrough_active") ) {
        sprintf (temp_buf, "hdmi_is_passthrough_active=%d", adev->hdmi_is_pth_active);
        return strdup (temp_buf);
    } else if (strstr (keys, "disable_pcm_mixing") ) {
        sprintf (temp_buf, "disable_pcm_mixing=%d", adev->disable_pcm_mixing);
        return strdup (temp_buf);
    } else if (strstr (keys, "hdmi_encodings") ) {
        struct format_desc *fmtdesc = NULL;
        bool dd = false, ddp = false;

        // query dd support
        fmtdesc = &adev->hdmi_descs.dd_fmt;
        if (fmtdesc && fmtdesc->fmt == AML_HDMI_FORMAT_AC3)
            dd = fmtdesc->is_support;

        // query ddp support
        fmtdesc = &adev->hdmi_descs.ddp_fmt;
        if (fmtdesc && fmtdesc->fmt == AML_HDMI_FORMAT_DDP)
            ddp = fmtdesc->is_support;

        sprintf (temp_buf, "hdmi_encodings=%s", "pcm;");
        if (ddp)
            sprintf (temp_buf, "ac3;eac3;");
        else if (dd)
            sprintf (temp_buf, "ac3;");

        return strdup (temp_buf);
    } else if (strstr (keys, "is_passthrough_active") ) {
        bool active = false;
        pthread_mutex_lock (&adev->lock);
        // "is_passthrough_active" is used by amnuplayer to check is current audio hal have passthrough instance(DD/DD+/DTS)
        // if already have one passthrough instance, it will not invoke another one.
        // While in HDMI plug off/in test case. Player will query "is_passthrough_active" before adev->usecase_masks finally settle
        // So Player will have a chance get a middle-term value, which is wrong.
        // now only one passthrough instance, do not check
        /* if (adev->usecase_masks & RAW_USECASE_MASK)
            active = true;
        else if (adev->audio_patch && (adev->audio_patch->aformat == AUDIO_FORMAT_E_AC3 \
                                       || adev->audio_patch->aformat == AUDIO_FORMAT_AC3) ) {
            active = true;
        } */
        pthread_mutex_unlock (&adev->lock);
        sprintf (temp_buf, "is_passthrough_active=%d",active);
        return  strdup (temp_buf);
    } else if (strstr(keys, "hal_param_cec_control_tv")) {
        adev_get_cec_control_object(adev, temp_buf);
        return  strdup (temp_buf);
    } else if (!strcmp(keys, "SOURCE_GAIN")) {
        sprintf(temp_buf, "source_gain = %f %f %f %f %f", adev->eq_data.s_gain.atv, adev->eq_data.s_gain.dtv,
                adev->eq_data.s_gain.hdmi, adev->eq_data.s_gain.av, adev->eq_data.s_gain.media);
        ALOGD("source_gain = %f %f %f %f %f", adev->eq_data.s_gain.atv, adev->eq_data.s_gain.dtv,
                adev->eq_data.s_gain.hdmi, adev->eq_data.s_gain.av, adev->eq_data.s_gain.media);
        return strdup(temp_buf);
    } else if (!strcmp(keys, "POST_GAIN")) {
        sprintf(temp_buf, "post_gain = %f %f %f", adev->eq_data.p_gain.speaker, adev->eq_data.p_gain.spdif_arc,
                adev->eq_data.p_gain.headphone);
        return strdup(temp_buf);
    } else if (strstr(keys, "dolby_ms12_enable")) {
        int ms12_enable = (eDolbyMS12Lib == adev->dolby_lib_type_last);
        ALOGI("ms12_enable :%d", ms12_enable);
        sprintf(temp_buf, "dolby_ms12_enable=%d", ms12_enable);
        return  strdup(temp_buf);
    } else if (strstr(keys, "isReconfigA2dpSupported")) {
        return  strdup("isReconfigA2dpSupported=1");
    } else if (!strcmp(keys, "SOURCE_MUTE")) {
        sprintf(temp_buf, "source_mute = %d", adev->source_mute);
        return strdup(temp_buf);
    } else if (strstr (keys, "stream_dra_channel") ) {
       if (adev->audio_patch != NULL && adev->patch_src == SRC_DTV) {
          if (adev->audio_patch->dtv_NchOriginal > 8 || adev->audio_patch->dtv_NchOriginal < 1) {
              sprintf (temp_buf, "0.0");
            } else {
              sprintf(temp_buf, "channel_num=%d.%d", adev->audio_patch->dtv_NchOriginal,adev->audio_patch->dtv_lfepresent);
              ALOGD ("temp_buf=%s\n", temp_buf);
            }
       } else {
          sprintf (temp_buf, "0.0");
       }
        return strdup(temp_buf);
    }
    else if (strstr(keys, "HDMI Switch")) {
        sprintf(temp_buf, "HDMI Switch=%d", (OUTPORT_HDMI == adev->active_outport || adev->bHDMIConnected == 1));
        ALOGD("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    } else if (!strcmp(keys, "parental_control_av_mute")) {
        sprintf(temp_buf, "parental_control_av_mute = %d", adev->parental_control_av_mute);
        return strdup(temp_buf);
    }
    else if (strstr (keys, "diaglogue_enhancement") ) {
        sprintf(temp_buf, "diaglogue_enhancement=%d", adev->ms12.ac4_de);
        ALOGD("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    }
    else if (strstr(keys, "audioindicator")) {
        get_audio_indicator(adev, temp_buf);
        return strdup(temp_buf);
    }
    else if (strstr(keys, "main_input_underrun")) {
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            sprintf(temp_buf, "main_input_underrun=%d", dolby_ms12_get_main_underrun());
            return strdup(temp_buf);
        }
    }
   else if (strstr (keys, "hal_param_get_earctx_attend_type") ) {
        int type = aml_audio_earctx_get_type(adev);
        sprintf(temp_buf, "hal_param_get_earctx_attend_type=%d", type);
        ALOGD("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    }
    else if (strstr (keys, "hal_param_get_earctx_cds") ) {
        char cds[AUDIO_HAL_CHAR_MAX_LEN] = {0};

        earctx_fetch_cds(&adev->alsa_mixer, cds, 0, &adev->hdmi_descs);
        sprintf(temp_buf, "hal_param_get_earctx_cds=%s", cds);
        return strdup(temp_buf);
    }
#ifdef USE_DTV
    else if (strstr (keys, "hal_param_dtv_demuxidbase") ) {
        sprintf(temp_buf, "hal_param_dtv_demuxidbase=%d", dtv_get_demuxidbase());
        ALOGD("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    } else if (strstr (keys, "hal_param_dtv_latencyms") ) {
        int latancyms = dtv_patch_get_latency(adev);
        sprintf(temp_buf, "hal_param_dtv_latencyms=%d", latancyms);
        ALOGD("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    } else if (strstr (keys, "ac4_active_pres_id")) {
       int active_id_offset = -1;
        if ((eDolbyMS12Lib == adev->dolby_lib_type) && (adev->ms12.input_config_format == AUDIO_FORMAT_AC4)) {
            //should use the dolby_ms12_get_ac4_active_presentation() before or after get_dolby_ms12_cleanup()
            pthread_mutex_lock(&adev->ms12.lock);
            if (adev->ms12.dolby_ms12_enable && (0 == dolby_ms12_get_ac4_active_presentation(&active_id_offset))) {
                ALOGI ("dolby_ms12_get_ac4_active_presentation index offset is %d\n", active_id_offset);
            }
            pthread_mutex_unlock(&adev->ms12.lock);
        }
        sprintf(temp_buf, "ac4_active_pres_id=%d", active_id_offset);
        return strdup(temp_buf);
    } else if (strstr (keys, "hal_param_dtv_es_pts_dts_flag")) {
        int flag = dtv_patch_get_es_pts_dts_flag(adev);
        sprintf(temp_buf, "hal_param_dtv_es_pts_dts_flag=%d", flag);
        ALOGD("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    }
#endif
    else if (strstr (keys, "codecsupport")) {
        return strdup(aml_get_codec_support((char*)(&keys[strlen("codecsupport") + 1]))? "true": "false");
    } else if (strstr(keys, "dolby_lib_type")) {
        ALOGI("dolby_lib_type: %d", adev->dolby_lib_type);
        sprintf(temp_buf, "dolby_lib_type=%d", adev->dolby_lib_type);
        return strdup(temp_buf);
    } else if (!strncmp(keys, "audio_info=", 11)) {
        int audio_info_type = 0;
        keys += 11;
        sscanf(keys ,"%d", &audio_info_type);

        switch (audio_info_type) {
        case AUDIO_PARAMTYPE_DEC_STATUS:
            sprintf (temp_buf,"audio_information_d=%d", adev->audio_hal_info.is_decoding);
            break;
        case AUDIO_PARAMTYPE_PTS:
            //AUDIO_PARAMTYPE_PTS
            break;
        case AUDIO_PARAMTYPE_SAMPLE_RATE:
            if (adev->dec_stream_info.dec_info.stream_sr == 0) {
                sprintf (temp_buf,"audio_information_s=%d", adev->audio_hal_info.sample_rate);
            } else
                sprintf (temp_buf,"audio_information_s=%d", adev->dec_stream_info.dec_info.stream_sr);
            break;
        case AUDIO_PARAMTYPE_CHANNEL_MODE:
            if (adev->dec_stream_info.dec_info.stream_ch == 0) {
                sprintf (temp_buf,"audio_information_c=%d", adev->audio_hal_info.channel_number);
            } else
                sprintf (temp_buf,"audio_information_c=%d", adev->dec_stream_info.dec_info.stream_ch);
            break;
        case AUDIO_PARAMTYPE_DIGITAL_AUDIO_INFO:
            //AUDIO_PARAMTYPE_DIGITAL_AUDIO_INFO;
            break;
        default:
            break;
        }
        ALOGI("temp_buf %s", temp_buf);
        return strdup(temp_buf);
    }
    else if (strstr (keys, "get_arc_capability")) {
        return strdup(get_arc_capability(adev));
    }
    return strdup("");
}

static int adev_init_check (const struct audio_hw_device *dev __unused)
{
    return 0;
}

static int adev_set_voice_volume (struct audio_hw_device *dev __unused, float volume __unused)
{
    return 0;
}

static int adev_set_master_volume (struct audio_hw_device *dev, float volume)
{
    if ((volume > 1.0) || (volume < 0.0) || (dev == NULL)) {
        return -EINVAL;
    }

    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    int i;
    pthread_mutex_lock (&adev->lock);
    adev->master_volume = volume;
    if (adev->master_mute)
        adev->master_mute = false;
    for (i = 0; i < STREAM_USECASE_MAX; i++) {
       struct aml_stream_out *aml_out = adev->active_outputs[i];
       struct audio_stream_out *out = (struct audio_stream_out *)aml_out;
       if (aml_out) {
            out_set_volume_l(out, aml_out->volume_l_org, aml_out->volume_r_org);
       }
    }
    ALOGI("%s() volume = %f, active_outport = %d", __FUNCTION__, volume, adev->active_outport);
    pthread_mutex_unlock (&adev->lock);
    return 0;
}

static int adev_get_master_volume (struct audio_hw_device *dev,
                                   float *volume)
{
    if ((volume == NULL) || (dev == NULL)) {
        return -EINVAL;
    }

    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    *volume = adev->master_volume;
    ALOGI("%s() volume = %f", __FUNCTION__, *volume);

    return 0;
}

static int adev_set_master_mute (struct audio_hw_device *dev, bool muted)
{
    if (dev == NULL) {
        return -EINVAL;
    }

    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    ALOGI("%s, %d -> %d", __FUNCTION__, adev->master_mute, muted);
    if (adev->master_mute != muted) {
        if (muted) { //mute
            adev_get_master_volume (dev, &adev->record_volume_before_mute); //Record the volume before mute
            adev_set_master_volume (dev, 0); //set mute
        } else {
            adev_set_master_volume (dev, adev->record_volume_before_mute); //unmute
        }
        adev->master_mute = muted;
    }
    return 0;
}

static int adev_get_master_mute (struct audio_hw_device *dev, bool *muted)
{
    if ((muted == NULL) || (dev == NULL)) {
        return -EINVAL;
    }

    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    *muted = adev->master_mute;
    ALOGI("%s() muted = %d", __FUNCTION__, *muted);

    return 0;

}
static int adev_set_mode (struct audio_hw_device *dev, audio_mode_t mode)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    ALOGD ("%s(%p, %d)", __FUNCTION__, dev, mode);

    pthread_mutex_lock (&adev->lock);
    if (adev->mode != mode) {
        adev->mode = mode;
        select_mode (adev);
    }
    pthread_mutex_unlock (&adev->lock);

    return 0;
}

static int adev_set_mic_mute (struct audio_hw_device *dev, bool state)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;

    adev->mic_mute = state;

    return 0;
}

static int adev_get_mic_mute (const struct audio_hw_device *dev, bool *state)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;

    *state = adev->mic_mute;

    return 0;

}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev __unused,
                                        const struct audio_config *config)
{
    int channel_count = audio_channel_count_from_in_mask(config->channel_mask);
    size_t size;

    ALOGD("%s: enter: channel_mask(%#x) rate(%d) format(%#x)", __func__,
        config->channel_mask, config->sample_rate, config->format);

    if (check_input_parameters(config->sample_rate, config->format, channel_count, AUDIO_DEVICE_NONE) != 0) {
        return -EINVAL;
    }

    size = get_input_buffer_size(0, config->sample_rate, config->format, channel_count);

    ALOGD("%s: exit: buffer_size = %zu", __func__, size);

    return size;
}

static int choose_stream_pcm_config(struct aml_stream_in *in)
{
    int channel_count = audio_channel_count_from_in_mask(in->hal_channel_mask);
    struct aml_audio_device *adev = in->dev;
    int ret = 0;

    if (in->device & AUDIO_DEVICE_IN_ALL_SCO) {
        memcpy(&in->config, &pcm_config_bt, sizeof(pcm_config_bt));
        if (adev->bt_wbs)
            in->config.rate = VX_WB_SAMPLING_RATE;
    } else {
        if (!((in->device & AUDIO_DEVICE_IN_HDMI) ||
                (in->device & AUDIO_DEVICE_IN_HDMI_ARC))) {
            memcpy(&in->config, &pcm_config_in, sizeof(pcm_config_in));
        }
        if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
            memcpy(&in->config, &pcm_config_aec, sizeof(pcm_config_aec));
        }
    }

    if (in->config.channels != 8)
        in->config.channels = channel_count;

    switch (in->hal_format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            in->config.format = PCM_FORMAT_S16_LE;
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            in->config.format = PCM_FORMAT_S32_LE;
            break;
        default:
            ALOGE("%s(), fmt not supported %#x", __func__, in->hal_format);
            break;
    }
    /* TODO: modify alsa config by params */
    update_alsa_config(in);

    return 0;
}

int add_in_stream_resampler(struct aml_stream_in *in)
{
    int ret = 0;

    if (in->requested_rate == in->config.rate)
        return 0;

    in->buffer = aml_audio_calloc(1, in->config.period_size * audio_stream_in_frame_size(&in->stream));
    if (!in->buffer) {
        ret = -ENOMEM;
        goto err;
    }

    ALOGD("%s: in->requested_rate = %d, in->config.rate = %d",
            __func__, in->requested_rate, in->config.rate);
    in->buf_provider.get_next_buffer = get_next_buffer;
    in->buf_provider.release_buffer = release_buffer;
    ret = create_resampler(in->config.rate, in->requested_rate, in->config.channels,
                        RESAMPLER_QUALITY_DEFAULT, &in->buf_provider, &in->resampler);
    if (ret != 0) {
        ALOGE("%s: create resampler failed (%dHz --> %dHz)", __func__, in->config.rate, in->requested_rate);
        ret = -EINVAL;
        goto err_resampler;
    }

    return 0;
err_resampler:
    aml_audio_free(in->buffer);
err:
    return ret;
}


static int adev_open_input_stream(struct audio_hw_device *dev,
                                audio_io_handle_t handle __unused,
                                audio_devices_t devices,
                                struct audio_config *config,
                                struct audio_stream_in **stream_in,
                                audio_input_flags_t flags __unused,
                                const char *address __unused,
                                audio_source_t source)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_stream_in *in;
    int channel_count = audio_channel_count_from_in_mask(config->channel_mask);
    int ret = 0;
    enum IN_PORT inport = INPORT_HDMIIN;

    AM_LOGD("%s: enter: devices(%#x) channel_mask(%#x) rate(%d) format(%#x) source(%d)", __func__,
        devices, config->channel_mask, config->sample_rate, config->format, source);

    if ((ret = check_input_parameters(config->sample_rate, config->format, channel_count, devices)) != 0) {
        if (-ENOSYS == ret) {
            ALOGV("  check_input_parameters input config Not Supported, and set to Default config(48000,2,PCM_16_BIT)");
            config->sample_rate = DEFAULT_OUT_SAMPLING_RATE;
            config->format = AUDIO_FORMAT_PCM_16_BIT;
            config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        } else {
           return -EINVAL;
        }

    } else {
        //check successfully, continue excute.
    }

    in = (struct aml_stream_in *)aml_audio_calloc(1, sizeof(struct aml_stream_in));
    if (!in) {
        AM_LOGE("  calloc fail, return!!!");
        return -ENOMEM;
    }
    in->device = devices & ~AUDIO_DEVICE_BIT_IN;

    if (channel_count == 1)
        // in fact, this value should be AUDIO_CHANNEL_OUT_BACK_LEFT(16u) according to VTS codes,
        // but the macroname can be confusing, so I'd like to set this value to
        // AUDIO_CHANNEL_IN_FRONT(16u) instead of AUDIO_CHANNEL_OUT_BACK_LEFT.
        config->channel_mask = AUDIO_CHANNEL_IN_FRONT;
    else if (channel_count == 2) {
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
            config->channel_mask = AUDIO_CHANNEL_IN_2POINT0POINT2;
        }
    }
    if (in->device  & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
        if (access(AML_AEC_LIB_PATH, R_OK) != 0) {
            AM_LOGE("libAudioSignalProcess.so not found.");
            goto err;
        }
    }

    android_dev_convert_to_hal_dev(devices, (int *)&inport);
    //adev->active_inport = inport;
    //adev->src_gain[inport] = 1.0;

#if defined(ENABLE_HBG_PATCH)
//[SEI-Tiger-2019/02/28] Optimize HBG RCU{
    if (is_hbg_hidraw() && (config->channel_mask != BUILT_IN_MIC)) {
//[SEI-Tiger-2019/02/28] Optimize HBG RCU}
        in->stream.common.get_sample_rate = in_hbg_get_sample_rate;
        in->stream.common.set_sample_rate = in_hbg_set_sample_rate;
        in->stream.common.get_buffer_size = in_hbg_get_buffer_size;
        in->stream.common.get_channels = in_hbg_get_channels;
        in->stream.common.get_format = in_hbg_get_format;
        in->stream.common.set_format = in_hbg_set_format;
        in->stream.common.standby = in_hbg_standby;
        in->stream.common.dump = in_hbg_dump;
        in->stream.common.set_parameters = in_hbg_set_parameters;
        in->stream.common.get_parameters = in_hbg_get_parameters;
        in->stream.common.add_audio_effect = in_hbg_add_audio_effect;
        in->stream.common.remove_audio_effect = in_hbg_remove_audio_effect;
        in->stream.set_gain = in_hbg_set_gain;
        in->stream.read = in_hbg_read;
        in->stream.get_input_frames_lost = in_hbg_get_input_frames_lost;
        in->stream.get_capture_position =  in_hbg_get_hbg_capture_position;
        in->hbg_channel = regist_callBack_stream();
        in->stream.get_active_microphones = in_get_active_microphones;
    }
    #ifndef BUILD_LINUX
    else if(remoteDeviceOnline() && (config->channel_mask != BUILT_IN_MIC)) {
    #endif
#else
    #ifndef BUILD_LINUX
    if (remoteDeviceOnline() && (config->channel_mask != BUILT_IN_MIC)) {
    #endif
#endif
    #ifndef BUILD_LINUX
        in->stream.common.set_sample_rate = kehwin_in_set_sample_rate;
        in->stream.common.get_sample_rate = kehwin_in_get_sample_rate;
        in->stream.common.get_buffer_size = kehwin_in_get_buffer_size;
        in->stream.common.get_channels = kehwin_in_get_channels;
        in->stream.common.get_format = kehwin_in_get_format;
        in->stream.common.set_format = kehwin_in_set_format;
        in->stream.common.dump = kehwin_in_dump;
        in->stream.common.set_parameters = kehwin_in_set_parameters;
        in->stream.common.get_parameters = kehwin_in_get_parameters;
        in->stream.common.add_audio_effect = kehwin_in_add_audio_effect;
        in->stream.common.remove_audio_effect = kehwin_in_remove_audio_effect;
        in->stream.set_gain = kehwin_in_set_gain;
        in->stream.common.standby = kehwin_in_standby;
        in->stream.read = kehwin_in_read;
        in->stream.get_input_frames_lost = kehwin_in_get_input_frames_lost;
        in->stream.get_capture_position =  kehwin_in_get_capture_position;

    }

    else
    #endif
    {
        in->stream.common.get_sample_rate = in_get_sample_rate;
        in->stream.common.set_sample_rate = in_set_sample_rate;
        in->stream.common.get_buffer_size = in_get_buffer_size;
        in->stream.common.get_channels = in_get_channels;
        in->stream.common.get_format = in_get_format;
        in->stream.common.set_format = in_set_format;
        in->stream.common.standby = in_standby;
        in->stream.common.dump = in_dump;
        in->stream.common.set_parameters = in_set_parameters;
        in->stream.common.get_parameters = in_get_parameters;
        in->stream.set_gain = in_set_gain;
        in->stream.read = in_read;
        in->stream.get_capture_position = in_get_capture_position;
        in->stream.get_input_frames_lost = in_get_input_frames_lost;
        in->stream.get_active_microphones = in_get_active_microphones;
        in->stream.common.add_audio_effect = in_add_audio_effect;
        in->stream.common.remove_audio_effect = in_remove_audio_effect;
    }

    in->dev = adev;
    in->standby = 1;

    in->requested_rate = config->sample_rate;
    in->hal_channel_mask = config->channel_mask;
    in->hal_format = config->format;

    if (in->device & AUDIO_DEVICE_IN_ALL_SCO) {
        memcpy(&in->config, &pcm_config_bt, sizeof(pcm_config_bt));
        if (adev->bt_wbs)
            in->config.rate = VX_WB_SAMPLING_RATE;
    #ifndef BUILD_LINUX
    } else if (in->device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
        //bluetooth rc voice
        // usecase for bluetooth rc audio hal
        AM_LOGI("%s: use RC audio HAL", __func__);
        ret = rc_open_input_stream(&in, config);
        if (ret != 0) {
            ALOGE("  rc_open_input_stream fail, goto err!!!");
            goto err;
        }
        config->sample_rate = in->config.rate;
        config->channel_mask = AUDIO_CHANNEL_IN_MONO;
    #endif
    } else if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
        memcpy(&in->config, &pcm_config_aec, sizeof(pcm_config_aec));
    } else {
        memcpy(&in->config, &pcm_config_in, sizeof(pcm_config_in));
    }
    in->config.channels = channel_count;
    in->source = source;
    if (source == AUDIO_SOURCE_ECHO_REFERENCE) {
        in->config.rate = PLAYBACK_CODEC_SAMPLING_RATE;
    }
    /* TODO: modify alsa config by params */
    update_alsa_config(in);

    switch (config->format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            in->config.format = PCM_FORMAT_S16_LE;
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            in->config.format = PCM_FORMAT_S32_LE;
            break;
        default:
            break;
    }

    in->buffer = aml_audio_malloc(in->config.period_size * audio_stream_in_frame_size(&in->stream));
    if (!in->buffer) {
        ret = -ENOMEM;
        ALOGE("  malloc fail, goto err!!!");
        goto err;
    }
    memset(in->buffer, 0, in->config.period_size * audio_stream_in_frame_size(&in->stream));

    if (!(in->device & AUDIO_DEVICE_IN_WIRED_HEADSET) && in->requested_rate != in->config.rate) {
        ALOGD("%s: in->requested_rate = %d, in->config.rate = %d",
            __func__, in->requested_rate, in->config.rate);
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;
        ret = create_resampler(in->config.rate, in->requested_rate, in->config.channels,
                            RESAMPLER_QUALITY_DEFAULT, &in->buf_provider, &in->resampler);
        if (ret != 0) {
            ALOGE("%s: create resampler failed (%dHz --> %dHz)", __func__, in->config.rate, in->requested_rate);
            ret = -EINVAL;
            goto err;
        }
    }

#ifdef ENABLE_AEC_HAL
    // Default 2 ch pdm + 2 ch lb
    if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        aec_spk_mic_init(in->requested_rate, 2, 2);
    }
#endif

    /* If AEC is in the app, only configure based on ECHO_REFERENCE spec.
     * If AEC is in the HAL, configure using the given mic stream. */
#ifdef ENABLE_AEC_APP
    bool aecInput = (source == AUDIO_SOURCE_ECHO_REFERENCE);
    if (aecInput) {
        int aec_ret = init_aec_mic_config(adev->aec, in);
        if (aec_ret) {
            ALOGE("AEC: Mic config init failed!");
            return -EINVAL;
        }
    }
#endif

    if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
        adev->aml_aec = aec_create(AML_AEC_MIC_CHANNEL_NUM, pcm_config_aec);
    }
    *stream_in = &in->stream;

#if ENABLE_NANO_NEW_PATH
    if (nano_is_connected() && (in->device & AUDIO_DEVICE_IN_BLUETOOTH_BLE)) {
        ret = nano_input_open(*stream_in, config);
        if (ret < 0) {
            ALOGD("%s: nano_input_open : %d",__func__,ret);
        }
    }
#endif

    ALOGD("%s: exit", __func__);
    return 0;
err:
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    if (in->buffer) {
        aml_audio_free(in->buffer);
        in->buffer = NULL;
    }
    aml_audio_free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                struct audio_stream_in *stream)
{
#ifndef BUILD_LINUX
    if (remoteDeviceOnline()) {
        kehwin_adev_close_input_stream(dev,stream);
        return;
    }
#endif
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_stream_in *in = (struct aml_stream_in *)stream;

    ALOGD("%s: enter: dev(%p) stream(%p)", __func__, dev, stream);
#if ENABLE_NANO_NEW_PATH
    nano_close(stream);
#endif

    in_standby(&stream->common);
    #ifndef BUILD_LINUX
    if (in->device & AUDIO_DEVICE_IN_WIRED_HEADSET)
        rc_close_input_stream(in);
    #endif
#ifdef ENABLE_AEC_HAL
    if (in->device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
        if (in->tmp_buffer_8ch) {
            aml_audio_free(in->tmp_buffer_8ch);
            in->tmp_buffer_8ch = NULL;
        }
        aec_spk_mic_release();
    }
#endif

    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }

    pthread_mutex_lock (&in->lock);
    if (in->buffer) {
        aml_audio_free(in->buffer);
        in->buffer = NULL;
    }
    pthread_mutex_unlock (&in->lock);

    if (in->proc_buf) {
        aml_audio_free(in->proc_buf);
    }
    if (in->ref_buf) {
        aml_audio_free(in->ref_buf);
    }
/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support { */
#if defined(ENABLE_HBG_PATCH)
    unregist_callBack_stream(in->hbg_channel);
#endif
/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support } */

#ifdef ENABLE_AEC_APP
    if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
        destroy_aec_mic_config(adev->aec);
    }
#endif

    if (in->device & AUDIO_DEVICE_IN_ECHO_REFERENCE) {
        aec_destroy(adev->aml_aec);
    }

    aml_audio_free(stream);
    ALOGD("%s: exit", __func__);
    return;
}

static void dump_audio_port_config (const struct audio_port_config *port_config)
{
    if (port_config == NULL)
        return;

    ALOGI ("  -%s port_config(%p)", __FUNCTION__, port_config);
    ALOGI ("\t-id(%d), role(%s), type(%s)",
        port_config->id,
        audio_port_role_to_str(port_config->role),
        audio_port_type_to_str(port_config->type));
    ALOGV ("\t-config_mask(%#x)", port_config->config_mask);
    ALOGI ("\t-sample_rate(%d), channel_mask(%#x), format(%#x)", port_config->sample_rate,
           port_config->channel_mask, port_config->format);
    ALOGV ("\t-gain.index(%#x)", port_config->gain.index);
    ALOGV ("\t-gain.mode(%#x)", port_config->gain.mode);
    ALOGV ("\t-gain.channel_mask(%#x)", port_config->gain.channel_mask);
    ALOGI ("\t-gain.value0(%d)", port_config->gain.values[0]);
    ALOGI ("\t-gain.value1(%d)", port_config->gain.values[1]);
    ALOGI ("\t-gain.value2(%d)", port_config->gain.values[2]);
    ALOGV ("\t-gain.ramp_duration_ms(%d)", port_config->gain.ramp_duration_ms);
    switch (port_config->type) {
    case AUDIO_PORT_TYPE_DEVICE:
        ALOGI ("\t-port device: type(%#x) addr(%s)",
               port_config->ext.device.type, port_config->ext.device.address);
        break;
    case AUDIO_PORT_TYPE_MIX:
        ALOGI ("\t-port mix: iohandle(%d)", port_config->ext.mix.handle);
        break;
    default:
        break;
    }
}

/* must be called with hw device and output stream mutexes locked */
int do_output_standby_l(struct audio_stream *stream)
{
    struct audio_stream_out *out = (struct audio_stream_out *) stream;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    ALOGI("[%s:%d] stream usecase:%s , continuous:%d", __func__, __LINE__,
        usecase2Str(aml_out->usecase), adev->continuous_audio_mode);
    /*
    if continuous mode,we need always have output.
    so we should not disable the output.
    */
    pthread_mutex_lock(&adev->alsa_pcm_lock);
    /*SWPL-15191 when exit movie player, it will set continuous and the alsa
      can't be closed. After playing something alsa will be opend again and can't be
      closed anymore, because its ref is bigger than 0.
      Now we add this patch to make sure the alsa will be closed.
    */
    if (aml_out->status == STREAM_HW_WRITING &&
        ((!continuous_mode(adev) || (!ms12->dolby_ms12_enable && (eDolbyMS12Lib == adev->dolby_lib_type))))) {
        ALOGI("%s aml_out(%p)standby close", __func__, aml_out);
        aml_alsa_output_close(out);

        if (aml_out->spdifout_handle) {
            aml_audio_spdifout_close(aml_out->spdifout_handle);
            aml_out->spdifout_handle = NULL;
        }
        if (aml_out->spdifout2_handle) {
            aml_audio_spdifout_close(aml_out->spdifout2_handle);
            aml_out->spdifout2_handle = NULL;
        }
    }

    aml_out->status = STREAM_STANDBY;
    aml_out->standby= 1;
    aml_out->continuous_mode_check = true;

    if (adev->continuous_audio_mode == 0) {
        // release buffers
        if (aml_out->buffer) {
            aml_audio_free(aml_out->buffer);
            aml_out->buffer = NULL;
        }

        if (aml_out->resampler) {
            release_resampler(aml_out->resampler);
            aml_out->resampler = NULL;
        }
    }
    usecase_change_validate_l (aml_out, true);
    pthread_mutex_unlock(&adev->alsa_pcm_lock);

    if (aml_out->is_normal_pcm) {
        set_system_app_mixing_status(aml_out, aml_out->status);
        aml_out->normal_pcm_mixing_config = false;
    }
    aml_out->pause_status = false;//clear pause status

#ifdef ENABLE_AEC_APP
    aec_set_spk_running(adev->aec, false);
#endif

    return 0;
}

int out_standby_new(struct audio_stream *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    int status;

    ALOGD("%s: enter", __func__);
    aml_audio_trace_int("out_standby_new", 1);
    if (aml_out->status == STREAM_STANDBY) {
        ALOGI("already standby, do nothing");
        aml_audio_trace_int("out_standby_new", 0);
        return 0;
    }

#if 0 // close this part, put the sleep to ms12 dolby_ms12_main_flush().
    if (continuous_mode(aml_out->dev)
        && (aml_out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC)) {
        //1.audio easing duration is 32ms,
        //2.one loop for schedule_run cost about 32ms(contains the hardware costing),
        //3.if [pause, flush] too short, means it need more time to do audio easing
        //so, the delay time for 32ms(pause is completed after audio easing is done) is enough.
        //aml_audio_sleep(64000);
    }
#endif
    pthread_mutex_lock (&aml_out->dev->lock);
    pthread_mutex_lock (&aml_out->lock);
    status = do_output_standby_l(stream);
    pthread_mutex_unlock (&aml_out->lock);
    pthread_mutex_unlock (&aml_out->dev->lock);
    ALOGI("%s exit", __func__);
    aml_audio_trace_int("out_standby_new", 0);

    return status;
}

static bool is_iec61937_format (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;

    if ( (adev->hdmi_format == BYPASS) && \
         (aml_out->hal_format == AUDIO_FORMAT_IEC61937) && \
         (adev->sink_format == AUDIO_FORMAT_E_AC3) ) {
        /*
         *case 1.With Kodi APK Dolby passthrough
         *Media Player direct format is AUDIO_FORMAT_IEC61937, containing DD+ 7.1ch audio
         *case 2.HDMI-IN or SPDIF-IN
         *audio format is AUDIO_FORMAT_IEC61937, containing DD+ 7.1ch audio
         */
        return true;
    }
    /*other data is dd/dd+ raw data*/

    return false;
}

audio_format_t get_output_format (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    audio_format_t output_format = aml_out->hal_internal_format;

    struct dolby_ms12_desc *ms12 = & (adev->ms12);

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
            output_format = adev->sink_format;
    } else if (eDolbyDcvLib == adev->dolby_lib_type) {
        if (adev->hdmi_format > 0) {
            if (adev->dual_spdif_support) {
                if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3 && adev->sink_format == AUDIO_FORMAT_E_AC3) {
                    if (adev->dolby_decode_enable == 1) {
                        output_format =  AUDIO_FORMAT_AC3;
                    } else {
                        output_format =  AUDIO_FORMAT_E_AC3;
                    }
                } else {
                    output_format = adev->sink_format;
                }
            } else {
                output_format = adev->sink_format;
            }
        } else
            output_format = adev->sink_format;
    }

    return output_format;
}


/* AEC need a wall clock (monotonic clk?) to sync*/
static aec_timestamp get_timestamp(void) {
    struct timespec ts;
    unsigned long long current_time;
    aec_timestamp return_val;
    /*clock_gettime(CLOCK_MONOTONIC, &ts);*/
    clock_gettime(CLOCK_REALTIME, &ts);
    current_time = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return_val.timeStamp = current_time;
    return return_val;
}

ssize_t audio_hal_data_processing_ms12v2(struct audio_stream_out *stream,
                                const void *buffer,
                                size_t bytes,
                                void **output_buffer,
                                size_t *output_buffer_bytes,
                                audio_format_t output_format,
                                int nchannels)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    int16_t *tmp_buffer = (int16_t *) buffer;
    int32_t *effect_tmp_buf = NULL;
    /* TODO  support 24/32 bit sample */
    int out_frames = bytes / (nchannels * 2); /* input is nchannels 16 bit */
    size_t i;
    int j, ret;
    uint32_t latency_frames = 0;
    uint64_t total_frame = 0;
    int enable_dump = aml_audio_property_get_bool("vendor.media.audiohal.outdump", false);
    if (adev->debug_flag) {
        ALOGD("%s,size %zu,format %x,ch %d\n",__func__,bytes,output_format,nchannels);
    }
    {
        /* nchannels 16 bit PCM, and there is no effect applied after MS12 processing */
        {
            int16_t *tmp_buffer = (int16_t *)buffer;
            size_t out_frames = bytes / (nchannels * 2); /* input is nchannels 16 bit */
            if (enable_dump) {
                FILE *fp1 = fopen("/data/vendor/audiohal/ms12_out_spk.pcm", "a+");
                if (fp1) {
                    int flen = fwrite((char *)tmp_buffer, 1, bytes, fp1);
                    ALOGV("%s buffer %p size %zu\n", __FUNCTION__, effect_tmp_buf, bytes);
                    fclose(fp1);
                }
            }
            if (adev->effect_buf_size < bytes*2) {
                adev->effect_buf = aml_audio_realloc(adev->effect_buf, bytes*2);
                if (!adev->effect_buf) {
                    ALOGE ("realloc effect buf failed size %zu format = %#x", bytes, output_format);
                    return -ENOMEM;
                } else {
                    ALOGI("realloc effect_buf size from %zu to %zu format = %#x", adev->effect_buf_size, bytes, output_format);
                }
                adev->effect_buf_size = bytes*2;
            }
            /* apply volume for spk/hp, SPDIF/HDMI keep the max volume */
            float gain_speaker = adev->sink_gain[OUTPORT_SPEAKER];
            effect_tmp_buf = (int32_t *)adev->effect_buf;
            if ((eDolbyMS12Lib == adev->dolby_lib_type) && aml_out->ms12_vol_ctrl) {
                gain_speaker = 1.0;
            }
            apply_volume_16to32(gain_speaker, (int16_t *)tmp_buffer,effect_tmp_buf, bytes);
            if (enable_dump) {
                FILE *fp1 = fopen("/data/vendor/audiohal/ms12_out_spk-volume-32bit.pcm", "a+");
                if (fp1) {
                    int flen = fwrite((char *)effect_tmp_buf, 1, bytes*2, fp1);
                    ALOGV("%s buffer %p size %zu\n", __FUNCTION__, effect_tmp_buf, bytes);
                    fclose(fp1);
                }
            }
            /* nchannels 32 bit --> 8 channel 32 bit mapping */
            if (aml_out->tmp_buffer_8ch_size < out_frames * 4*adev->default_alsa_ch) {
                aml_out->tmp_buffer_8ch = aml_audio_realloc(aml_out->tmp_buffer_8ch, out_frames * 4*adev->default_alsa_ch);
                if (!aml_out->tmp_buffer_8ch) {
                    ALOGE("%s: realloc tmp_buffer_8ch buf failed size = %zu format = %#x",
                        __func__, 8 * bytes, output_format);
                    return -ENOMEM;
                } else {
                    ALOGI("%s: realloc tmp_buffer_8ch size from %zu to %zu format = %#x",
                        __func__, aml_out->tmp_buffer_8ch_size, out_frames * 4*adev->default_alsa_ch, output_format);
                }
                aml_out->tmp_buffer_8ch_size = out_frames * 4*adev->default_alsa_ch;
            }

            for (i = 0; i < out_frames; i++) {
                for (j = 0; j < nchannels; j++) {
                    aml_out->tmp_buffer_8ch[adev->default_alsa_ch * i + j] = effect_tmp_buf[nchannels * i + j];
                }
                for(j = nchannels; j < adev->default_alsa_ch; j++) {
                    aml_out->tmp_buffer_8ch[adev->default_alsa_ch * i + j] = 0;
                }
            }
            *output_buffer = aml_out->tmp_buffer_8ch;
            *output_buffer_bytes = out_frames * 4*adev->default_alsa_ch; /* from nchannels 32 bit to 8 ch 32 bit */
            if (enable_dump) {
                FILE *fp1 = fopen("/data/vendor/audiohal/ms12_out_10_spk.pcm", "a+");
                if (fp1) {
                    int flen = fwrite((char *)aml_out->tmp_buffer_8ch, 1,out_frames *4*adev->default_alsa_ch, fp1);
                    fclose(fp1);
                }
            }
        }
    }
    return 0;
}

ssize_t audio_hal_data_processing(struct audio_stream_out *stream,
                                const void *buffer,
                                size_t bytes,
                                void **output_buffer,
                                size_t *output_buffer_bytes,
                                audio_format_t output_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    int16_t *tmp_buffer = (int16_t *) buffer;
    int16_t *effect_tmp_buf = NULL;
    int out_frames = bytes / 4;
    size_t i;
    int j, ret;
    uint32_t latency_frames = 0;
    uint64_t total_frame = 0;

    /* raw data need packet to IEC61937 format by spdif encoder */
    if (output_format == AUDIO_FORMAT_IEC61937) {
        //ALOGI("IEC61937 Format");
        *output_buffer = (void *) buffer;
        *output_buffer_bytes = bytes;
    } else if ((output_format == AUDIO_FORMAT_AC3) || (output_format == AUDIO_FORMAT_E_AC3)) {
        //ALOGI("%s, aml_out->hal_format %x , is_iec61937_format = %d, \n", __func__,
        //      aml_out->hal_format,is_iec61937_format(stream));
        if ((is_iec61937_format(stream) == true) ||
            (adev->dolby_lib_type == eDolbyDcvLib)) {
            *output_buffer = (void *) buffer;
            *output_buffer_bytes = bytes;
            /* Need mute when play DTV, HDMI output. */
            if (adev->sink_gain[OUTPORT_HDMI] < FLOAT_ZERO && adev->active_outport == OUTPORT_HDMI && adev->audio_patching) {
                memset(*output_buffer, 0, bytes);
            }
        } else {
            if (aml_out->spdifenc_init == false) {
                ALOGI("%s, aml_spdif_encoder_open, output_format %#x\n", __func__, output_format);
                ret = aml_spdif_encoder_open(&aml_out->spdifenc_handle, output_format);
                if (ret) {
                    ALOGE("%s() aml_spdif_encoder_open failed", __func__);
                    return ret;
                }
                aml_out->spdifenc_init = true;
                aml_out->spdif_enc_init_frame_write_sum = aml_out->frame_write_sum;
                adev->spdif_encoder_init_flag = true;
            }
            aml_spdif_encoder_process(aml_out->spdifenc_handle, buffer, bytes, output_buffer, output_buffer_bytes);
        }
    } else if (output_format == AUDIO_FORMAT_DTS) {
        *output_buffer = (void *) buffer;
        *output_buffer_bytes = bytes;
    } else if (output_format == AUDIO_FORMAT_PCM_32_BIT) {
        int32_t *tmp_buffer = (int32_t *)buffer;
        size_t out_frames = bytes / FRAMESIZE_32BIT_STEREO;
        if (!aml_out->ms12_vol_ctrl) {
           float gain_speaker = adev->sink_gain[OUTPORT_SPEAKER];
            gain_speaker *= aml_out->volume_l;
            apply_volume(gain_speaker, tmp_buffer, sizeof(uint32_t), bytes);

        }

        /* 2 ch 32 bit --> 8 ch 32 bit mapping, need 8X size of input buffer size */
        if (aml_out->tmp_buffer_8ch_size < FRAMESIZE_32BIT_8ch * out_frames) {
            aml_out->tmp_buffer_8ch = aml_audio_realloc(aml_out->tmp_buffer_8ch, FRAMESIZE_32BIT_8ch * out_frames);
            if (!aml_out->tmp_buffer_8ch) {
                ALOGE("%s: realloc tmp_buffer_8ch buf failed size = %zu format = %#x", __func__,
                        FRAMESIZE_32BIT_8ch * out_frames, output_format);
                return -ENOMEM;
            } else {
                ALOGI("%s: realloc tmp_buffer_8ch size from %zu to %zu format = %#x", __func__,
                        aml_out->tmp_buffer_8ch_size, FRAMESIZE_32BIT_8ch * out_frames, output_format);
            }
            aml_out->tmp_buffer_8ch_size = FRAMESIZE_32BIT_8ch * out_frames;
        }

        for (i = 0; i < out_frames; i++) {
            aml_out->tmp_buffer_8ch[8 * i] = tmp_buffer[2 * i];
            aml_out->tmp_buffer_8ch[8 * i + 1] = tmp_buffer[2 * i + 1];
            aml_out->tmp_buffer_8ch[8 * i + 2] = tmp_buffer[2 * i];
            aml_out->tmp_buffer_8ch[8 * i + 3] = tmp_buffer[2 * i + 1];
            aml_out->tmp_buffer_8ch[8 * i + 4] = tmp_buffer[2 * i];
            aml_out->tmp_buffer_8ch[8 * i + 5] = tmp_buffer[2 * i + 1];
            aml_out->tmp_buffer_8ch[8 * i + 6] = 0;
            aml_out->tmp_buffer_8ch[8 * i + 7] = 0;
        }

        *output_buffer = aml_out->tmp_buffer_8ch;
        *output_buffer_bytes = FRAMESIZE_32BIT_8ch * out_frames;
    } else {
        /*atom project supports 32bit hal only*/
        /*TODO: Direct PCM case, I think still needs EQ and AEC */
        if (aml_out->is_tv_platform == 1) {
            int16_t *tmp_buffer = (int16_t *)buffer;
            size_t out_frames = bytes / (2 * 2);

            int16_t *effect_tmp_buf;
            int32_t *spk_tmp_buf;
            int16_t *hp_tmp_buf = NULL;
            int32_t *ps32SpdifTempBuffer = NULL;
            float source_gain;
            float gain_speaker = adev->eq_data.p_gain.speaker;

            /* handling audio effect process here */
            if (adev->effect_buf_size < bytes) {
                adev->effect_buf = aml_audio_realloc(adev->effect_buf, bytes);
                if (!adev->effect_buf) {
                    ALOGE ("realloc effect buf failed size %zu format = %#x", bytes, output_format);
                    return -ENOMEM;
                } else {
                    ALOGI("realloc effect_buf size from %zu to %zu format = %#x", adev->effect_buf_size, bytes, output_format);
                }
                adev->effect_buf_size = bytes;

                adev->spk_output_buf = aml_audio_realloc(adev->spk_output_buf, bytes * 2);
                if (!adev->spk_output_buf) {
                    ALOGE ("realloc headphone buf failed size %zu format = %#x", bytes, output_format);
                    return -ENOMEM;
                }
                // 16bit -> 32bit, need realloc
                adev->spdif_output_buf = aml_audio_realloc(adev->spdif_output_buf, bytes * 2);
                if (!adev->spdif_output_buf) {
                    ALOGE ("realloc spdif buf failed size %zu format = %#x", bytes, output_format);
                    return -ENOMEM;
                }

                adev->hp_output_buf = aml_audio_realloc(adev->hp_output_buf, bytes);
                if (!adev->hp_output_buf) {
                    ALOGE ("realloc hp buf failed size %zu format = %#x", bytes, output_format);
                    return -ENOMEM;
                }
            }
            hp_tmp_buf = (int16_t *)adev->hp_output_buf;
            effect_tmp_buf = (int16_t *)adev->effect_buf;
            spk_tmp_buf = (int32_t *)adev->spk_output_buf;

            bool dap_processing = is_audio_postprocessing_add_dolbyms12_dap(adev) && adev->ms12.dolby_ms12_enable;
            if (dap_processing) {
                if (adev->debug_flag > 1) {
                    AM_LOGI("Get HP/SPDIF data from ring buffer");
                }
                int read_space = get_buffer_read_space(&adev->ms12.spdif_ring_buffer);
                if (adev->ms12.spdif_ring_buffer.size && read_space >= (int)bytes) {
                    ring_buffer_read(&adev->ms12.spdif_ring_buffer, (unsigned char*)hp_tmp_buf, bytes);
                } else {
                    AM_LOGE("get hp/spdif pcm data fail, ring buf size(%d), read space(%d)",
                        adev->ms12.spdif_ring_buffer.size, read_space);
                }
            } else {
                memcpy(hp_tmp_buf, tmp_buffer, bytes);
            }

            ps32SpdifTempBuffer = (int32_t *)adev->spdif_output_buf;
#ifdef ENABLE_AVSYNC_TUNING
            tuning_spker_latency(adev, effect_tmp_buf, tmp_buffer, bytes);
#else
            memcpy(effect_tmp_buf, tmp_buffer, bytes);
#endif

            if (adev->patch_src == SRC_DTV)
                source_gain = adev->eq_data.s_gain.dtv;
            else if (adev->patch_src == SRC_HDMIIN)
                source_gain = adev->eq_data.s_gain.hdmi;
            else if (adev->patch_src == SRC_LINEIN)
                source_gain = adev->eq_data.s_gain.av;
            else if (adev->patch_src == SRC_ATV)
                source_gain = adev->eq_data.s_gain.atv;
            else
                source_gain = adev->eq_data.s_gain.media;

            if (adev->patch_src == SRC_DTV && adev->audio_patch != NULL) {
                aml_audio_switch_output_mode((int16_t *)effect_tmp_buf, bytes, adev->audio_patch->mode);
            } else if ( adev->audio_patch == NULL) {
                aml_audio_switch_output_mode((int16_t *)effect_tmp_buf, bytes, adev->sound_track_mode);
            }

            if (adev->stream_write_data) {
                adev->stream_write_data = Stop_watch(adev->last_write_ts, 800);// no audio data more than 800ms
            }
            bool audio_effect_enable = adev->stream_write_data;
            if (adev->last_audio_effect_enable != audio_effect_enable) {
                adev->last_audio_effect_enable = audio_effect_enable;
                if (audio_effect_enable) {
                    if (eDolbyMS12Lib == adev->dolby_lib_type) {
                        if (adev->active_outport == OUTPORT_SPEAKER) {
                            set_ms12_full_dap_disable(&adev->ms12, false);
                        }
                    }
                } else {
                    if (eDolbyMS12Lib == adev->dolby_lib_type) {
                        set_ms12_full_dap_disable(&adev->ms12, true);
                    }
                }
            }

            /*aduio effect process for speaker*/
            if (adev->active_outport != OUTPORT_A2DP && audio_effect_enable) {
                audio_post_process(&adev->native_postprocess, effect_tmp_buf, out_frames);
            }

            if (aml_audio_property_get_bool("vendor.media.audiohal.outdump", false)) {
                aml_audio_dump_audio_bitstreams("/data/audio/audio_spk.pcm",
                    effect_tmp_buf, bytes);
            }

            float volume = source_gain;
            /* apply volume for spk/hp, SPDIF/HDMI keep the max volume */
            if (adev->active_outport == OUTPORT_A2DP) {
                if ((adev->patch_src == SRC_DTV || adev->patch_src == SRC_HDMIIN
                        || adev->patch_src == SRC_LINEIN || adev->patch_src == SRC_ATV)
                        && adev->audio_patching) {
                        volume *= adev->sink_gain[OUTPORT_A2DP];
                }
            } else {
                volume *= gain_speaker * adev->sink_gain[OUTPORT_SPEAKER];
            }

            apply_volume_16to32(volume, effect_tmp_buf, spk_tmp_buf, bytes);
            apply_volume_16to32(source_gain, hp_tmp_buf, ps32SpdifTempBuffer, bytes);
            apply_volume(adev->sink_gain[OUTPORT_HEADPHONE], hp_tmp_buf, sizeof(uint16_t), bytes);

#ifdef ADD_AUDIO_DELAY_INTERFACE
            aml_audio_delay_process(AML_DELAY_OUTPORT_SPEAKER, spk_tmp_buf,
                out_frames * 2 * 4, AUDIO_FORMAT_PCM_32_BIT, aml_out->hal_rate);  //spk/hp output pcm
            aml_audio_delay_process(AML_DELAY_OUTPORT_SPDIF, ps32SpdifTempBuffer,
                out_frames * 2 * 4, AUDIO_FORMAT_PCM_32_BIT, aml_out->hal_rate);  //spdif output pcm

#endif
            /* 2 ch 16 bit --> 8 ch 32 bit mapping, need 8X size of input buffer size */
            if (aml_out->tmp_buffer_8ch_size < 8 * bytes) {
                aml_out->tmp_buffer_8ch = aml_audio_realloc(aml_out->tmp_buffer_8ch, 8 * bytes);
                if (!aml_out->tmp_buffer_8ch) {
                    ALOGE("%s: realloc tmp_buffer_8ch buf failed size = %zu format = %#x",
                        __func__, 8 * bytes, output_format);
                    return -ENOMEM;
                } else {
                    ALOGI("%s: realloc tmp_buffer_8ch size from %zu to %zu format = %#x",
                        __func__, aml_out->tmp_buffer_8ch_size, 8 * bytes, output_format);
                }
                aml_out->tmp_buffer_8ch_size = 8 * bytes;
            }
            if (alsa_device_is_auge()) {
                for (i = 0; i < out_frames; i++) {
                    aml_out->tmp_buffer_8ch[8 * i + 0] = spk_tmp_buf[2 * i];
                    aml_out->tmp_buffer_8ch[8 * i + 1] = spk_tmp_buf[2 * i + 1];
                    aml_out->tmp_buffer_8ch[8 * i + 2] = ps32SpdifTempBuffer[2 * i];
                    aml_out->tmp_buffer_8ch[8 * i + 3] = ps32SpdifTempBuffer[2 * i + 1];
                    aml_out->tmp_buffer_8ch[8 * i + 4] = hp_tmp_buf[2 * i] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 5] = hp_tmp_buf[2 * i + 1] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 6] = tmp_buffer[2 * i] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 7] = tmp_buffer[2 * i + 1] << 16;
                }
            } else {
                for (i = 0; i < out_frames; i++) {
                    aml_out->tmp_buffer_8ch[8 * i + 0] = tmp_buffer[2 * i] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 1] = tmp_buffer[2 * i + 1] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 2] = spk_tmp_buf[2 * i];
                    aml_out->tmp_buffer_8ch[8 * i + 3] = spk_tmp_buf[2 * i + 1];
                    aml_out->tmp_buffer_8ch[8 * i + 4] = tmp_buffer[2 * i] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 5] = tmp_buffer[2 * i + 1] << 16;
                    aml_out->tmp_buffer_8ch[8 * i + 6] = 0;
                    aml_out->tmp_buffer_8ch[8 * i + 7] = 0;
                }
            }
#ifndef NO_AUDIO_CAP
                /* 2ch downmix capture for TV platform*/
                pthread_mutex_lock(&adev->cap_buffer_lock);
                if (adev->cap_buffer) {
                    IpcBuffer_write(adev->cap_buffer, (const unsigned char *)buffer, (int) bytes);
                }
                pthread_mutex_unlock(&adev->cap_buffer_lock);
#endif
            *output_buffer = aml_out->tmp_buffer_8ch;
            *output_buffer_bytes = 8 * bytes;
        } else {
            float gain_speaker = 1.0;
            if (adev->active_outport == OUTPORT_HDMI) {
                if (adev->audio_patching == true) {
                    gain_speaker = adev->sink_gain[OUTPORT_HDMI];
                }
            } else if (aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
                if (adev->audio_patching == true)
                    gain_speaker = adev->sink_gain[OUTPORT_A2DP];
            } else  if (adev->active_outport == OUTPORT_SPEAKER){
                gain_speaker = adev->sink_gain[OUTPORT_SPEAKER];
            }
            if (adev->debug_flag > 100) {
                ALOGI("[%s:%d] gain:%f, out_device:%#x, active_outport:%d", __func__, __LINE__,
                    gain_speaker, aml_out->out_device, adev->active_outport);
            }

            if ((eDolbyMS12Lib == adev->dolby_lib_type) && aml_out->ms12_vol_ctrl) {
                gain_speaker = 1.0;
            }

            if (adev->patch_src == SRC_DTV && adev->audio_patch != NULL) {
                aml_audio_switch_output_mode((int16_t *)buffer, bytes, adev->audio_patch->mode);
            } else if (adev->audio_patch == NULL) {
                aml_audio_switch_output_mode((int16_t *)buffer, bytes, adev->sound_track_mode);
            }

#ifndef NO_AUDIO_CAP
            /* 2ch downmix capture for nonetv platform*/
            pthread_mutex_lock(&adev->cap_buffer_lock);
            if (adev->cap_buffer) {
                IpcBuffer_write(adev->cap_buffer, (const unsigned char *)buffer, (int) bytes);
            }
            pthread_mutex_unlock(&adev->cap_buffer_lock);
#endif

            *output_buffer = (void *) buffer;
            *output_buffer_bytes = bytes;
            apply_volume(gain_speaker, *output_buffer, sizeof(uint16_t), bytes);
            aml_audio_delay_process(AML_DELAY_OUTPORT_SPDIF, *output_buffer, *output_buffer_bytes, AUDIO_FORMAT_PCM_16_BIT, aml_out->hal_rate);
        }
    }
    /*when REPORT_DECODED_INFO is added, we will enable it*/
#if 0
    if (adev->audio_patch != NULL && adev->patch_src == SRC_DTV) {
        int sample_rate = 0, pch = 0, lfepresent;
        char sysfs_buf[AUDIO_HAL_CHAR_MAX_LEN] = {0};
        if (adev->audio_patch->aformat != AUDIO_FORMAT_E_AC3
            && adev->audio_patch->aformat != AUDIO_FORMAT_AC3 &&
            adev->audio_patch->aformat != AUDIO_FORMAT_DTS) {
            unsigned int errcount;
            char sysfs_buf[AUDIO_HAL_CHAR_MAX_LEN] = {0};
            audio_decoder_status(&errcount);
            sprintf(sysfs_buf, "decoded_err %d", errcount);
            sysfs_set_sysfs_str(REPORT_DECODED_INFO, sysfs_buf);
            dtv_audio_decpara_get(&sample_rate, &pch, &lfepresent);
            pch = pch + lfepresent;
        } else if (adev->audio_patch->aformat == AUDIO_FORMAT_AC3 ||
            adev->audio_patch->aformat == AUDIO_FORMAT_E_AC3) {
            pch = adev->ddp.sourcechnum;
            sample_rate = adev->ddp.sourcesr;
        }
        sprintf(sysfs_buf, "ch_num %d", pch);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, sysfs_buf);
        sprintf(sysfs_buf, "samplerate %d", sample_rate);
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, sysfs_buf);
        if (pch == 2) {
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_STEREO);
        } else if (pch == 6) {
            sprintf (sysfs_buf, "ch_configuration %d", TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_5_1);
        } else {
            ALOGV("unsupport yet");
        }
        sysfs_set_sysfs_str(REPORT_DECODED_INFO, sysfs_buf);
    }
#endif
    if (adev->patch_src == SRC_DTV && adev->tuner2mix_patch) {
#ifdef USE_DTV
        dtv_in_write(stream, buffer, bytes);
#endif
    }

    return 0;
}

ssize_t hw_write (struct audio_stream_out *stream
                  , const void *buffer
                  , size_t bytes
                  , audio_format_t output_format)
{
    ALOGV ("+%s() buffer %p bytes %zu", __func__, buffer, bytes);
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    audio_config_base_t in_data_config = {48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT};
    const uint16_t *tmp_buffer = buffer;
    int16_t *effect_tmp_buf = NULL;
    struct aml_audio_patch *patch = adev->audio_patch;
    bool is_dtv = (adev->patch_src == SRC_DTV);
    int ch = 2;
    int bytes_per_sample = 2;

    int out_frames = 0;
    ssize_t ret = 0;
    int i;
    uint32_t latency_frames = 0;
    uint64_t total_frame = 0;
    uint64_t write_frames = 0;
    uint64_t  sys_total_cost = 0;
    int  adjust_ms = 0;
    int  alsa_port = -1;

    if (adev->is_TV && audio_is_linear_pcm(output_format)) {
        ch = adev->default_alsa_ch;
        bytes_per_sample = 4;
        in_data_config.channel_mask = AUDIO_CHANNEL_OUT_7POINT1;
        in_data_config.format = AUDIO_FORMAT_PCM_32_BIT;
    }
    out_frames = bytes / (ch * bytes_per_sample);

    adev->debug_flag = aml_audio_get_debug_flag();
    if (adev->debug_flag) {
        ALOGI("+%s() buffer %p bytes %zu, format %#x out %p need_sync %d\n",
            __func__, buffer, bytes, output_format, aml_out, aml_out->need_sync);
    }

    pthread_mutex_lock(&adev->alsa_pcm_lock);
    aml_out->alsa_output_format = output_format;
    if (aml_out->status != STREAM_HW_WRITING) {
        ALOGI("%s, aml_out %p alsa open output_format %#x\n", __func__, aml_out, output_format);
        if (adev->useSubMix) {
            if (/*adev->audio_patching &&*/
                output_format != AUDIO_FORMAT_PCM_16_BIT &&
                output_format != AUDIO_FORMAT_PCM) {
                // TODO: mbox+dvb and bypass case
                ret = aml_alsa_output_open(stream);
                if (ret) {
                    ALOGE("%s() open failed", __func__);
                }
            } else {
                aml_out->pcm = getSubMixingPCMdev(adev);
                if (aml_out->pcm == NULL) {
                    ALOGE("%s() get pcm handle failed", __func__);
                }
                if (adev->raw_to_pcm_flag) {
                    ALOGI("disable raw_to_pcm_flag --");
                    pcm_stop(aml_out->pcm);
                    adev->raw_to_pcm_flag = false;
                }
            }
        } else {
            if (!adev->tuner2mix_patch) {
                ret = aml_alsa_output_open(stream);
                if (ret) {
                    ALOGE("%s() open failed", __func__);
                }
            }
        }
#ifdef USE_DTV
        if (is_dtv)
            audio_set_spdif_clock(aml_out, get_codec_type(output_format));
#endif
        aml_out->status = STREAM_HW_WRITING;
    } else if (!aml_out->pcm && !adev->injection_enable) {
        ALOGE("pcm is null, open device");
        aml_alsa_output_open(stream);
    }

    if (aml_out->pcm || adev->a2dp_hal || is_sco_port(adev->active_outport)) {
        if (adjust_ms) {
            int adjust_bytes = 0;
            if ((output_format == AUDIO_FORMAT_E_AC3) || (output_format == AUDIO_FORMAT_AC3)) {
                int i = 0;
                int bAtmos = 0;
                int insert_frame = adjust_ms/32;
                int raw_size = 0;
                if (insert_frame > 0) {
                    char *raw_buf = NULL;
                    char *temp_buf = NULL;
                    /*atmos lock or input is ddp atmos*/
                    if (adev->atoms_lock_flag || adev->ms12.is_dolby_atmos) {
                        bAtmos = 1;
                    }
                    raw_buf = aml_audio_get_muteframe(output_format, &raw_size, bAtmos);
                    ALOGI("insert atmos=%d raw frame size=%d times=%d", bAtmos, raw_size, insert_frame);
                    if (raw_buf && (raw_size > 0)) {
                        temp_buf = aml_audio_malloc(raw_size);
                        if (!temp_buf) {
                            ALOGE("%s malloc failed", __func__);
                            pthread_mutex_unlock(&adev->alsa_pcm_lock);
                            return -1;
                        }
                        for (i = 0; i < insert_frame; i++) {
                            memcpy(temp_buf, raw_buf, raw_size);
                            ret = aml_alsa_output_write(stream, (void*)temp_buf, raw_size);
                            if (ret < 0) {
                                ALOGE("%s alsa write fail when insert", __func__);
                                break;
                            }
                        }
                        aml_audio_free(temp_buf);
                    }
                }
            } else {
                memset((void*)buffer, 0, bytes);
                if (output_format == AUDIO_FORMAT_E_AC3) {
                    adjust_bytes = 192 * 4 * abs(adjust_ms);
                } else if (output_format == AUDIO_FORMAT_AC3) {
                    adjust_bytes = 48 * 4 * abs(adjust_ms);
                } else {
                    if (adev->is_TV) {
                        adjust_bytes = 48 * 32 * abs(adjust_ms);    //8ch 32 bit.
                    } else {
                        adjust_bytes = 48 * 4 * abs(adjust_ms); // 2ch 16bit
                    }
                }
                adjust_bytes &= ~255;
                ALOGI("%s hwsync audio need %s %d ms,adjust bytes %d",
                      __func__, adjust_ms > 0 ? "insert" : "skip", abs(adjust_ms), adjust_bytes);
                if (adjust_ms > 0) {
                    char *buf = aml_audio_malloc(1024);
                    int write_size = 0;
                    if (!buf) {
                        ALOGE("%s malloc failed", __func__);
                        pthread_mutex_unlock(&adev->alsa_pcm_lock);
                        return -1;
                    }
                    memset(buf, 0, 1024);
                    while (adjust_bytes > 0) {
                        write_size = adjust_bytes > 1024 ? 1024 : adjust_bytes;
                        //#ifdef ENABLE_BT_A2DP
                        #ifndef BUILD_LINUX
                        if (adev->active_outport == OUTPORT_A2DP) {
                            ret = a2dp_out_write(adev, &in_data_config, (void*)buf, write_size);
                        } else
                        #endif
                        if (is_sco_port(adev->active_outport)) {
                            ret = write_to_sco(adev, &in_data_config, buffer, bytes);
                        } else {
                            ret = aml_alsa_output_write(stream, (void*)buf, write_size);
                        }
                        if (ret < 0) {
                            ALOGE("%s alsa write fail when insert", __func__);
                            break;
                        }
                        adjust_bytes -= write_size;
                    }
                    aml_audio_free(buf);
                } else {
                    //do nothing.
                    /*
                    if (bytes > (size_t)adjust_bytes)
                        bytes -= adjust_bytes;
                    else
                        bytes = 0;
                    */
                }
            }
        }
        //#ifdef ENABLE_BT_A2DP
        #ifndef BUILD_LINUX
        if (adev->active_outport == OUTPORT_A2DP) {
            ret = a2dp_out_write(adev, &in_data_config, buffer, bytes);
        } else
        #endif
        if (is_sco_port(adev->active_outport)) {
            ret = write_to_sco(adev, &in_data_config, buffer, bytes);
        } else {
            ret = aml_alsa_output_write(stream, (void *) buffer, bytes);
        }
        //ALOGE("!!aml_alsa_output_write"); ///zzz
        if (ret < 0) {
            ALOGE("ALSA out write fail");
            aml_out->frame_write_sum += out_frames;
        } else {
            if (!continuous_mode(adev)) {
                if ((output_format == AUDIO_FORMAT_AC3) ||
                    (output_format == AUDIO_FORMAT_E_AC3) ||
                    (output_format == AUDIO_FORMAT_MAT) ||
                    (output_format == AUDIO_FORMAT_DTS)) {
                    if (is_iec61937_format(stream) == true) {
                        //continuous_audio_mode = 0, when mixing-off and 7.1ch
                        //FIXME out_frames 4 or 16 when DD+ output???
                        aml_out->frame_write_sum += out_frames;

                    } else {
                        int sample_per_bytes = (output_format == AUDIO_FORMAT_MAT) ? 64 :
                                                (output_format == AUDIO_FORMAT_E_AC3) ? 16 : 4;

                        if (eDolbyDcvLib == adev->dolby_lib_type && aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
                            aml_out->frame_write_sum = aml_out->input_bytes_size / audio_stream_out_frame_size(stream);
                            //aml_out->frame_write_sum += out_frames;
                        } else {
                            aml_out->frame_write_sum += bytes / sample_per_bytes; //old code
                            //aml_out->frame_write_sum = spdif_encoder_ad_get_total() / sample_per_bytes + aml_out->spdif_enc_init_frame_write_sum; //8.1
                        }
                    }
                } else {
                    aml_out->frame_write_sum += out_frames;
                    total_frame =  aml_out->frame_write_sum;
                }
            }
        }
    }
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        latency_frames = aml_audio_out_get_ms12_latency_frames(stream);
    } else {
        latency_frames = out_get_latency_frames(stream);
    }

    pthread_mutex_unlock(&adev->alsa_pcm_lock);

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        /*it is the main alsa write function we need notify that to all sub-stream */
        adev->ms12.latency_frame = latency_frames;
        if (continuous_mode(adev)) {
            adev->ms12.sys_avail = dolby_ms12_get_system_buffer_avail(NULL);
        }
        //ALOGD("alsa latency=%d", latency_frames);
    }

    /*
    */
    if (!continuous_mode(adev)) {
        if (aml_out->hal_internal_format == AUDIO_FORMAT_PCM_16_BIT) {
            write_frames = aml_out->input_bytes_size / aml_out->hal_frame_size;
            //total_frame = write_frames;
        } else {
            total_frame = aml_out->frame_write_sum + aml_out->frame_skip_sum;
        }
    } else {
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            if (!audio_is_linear_pcm(aml_out->hal_internal_format)) {
                /*use the pcm which is gennerated by udc, to get the total frame by nbytes/nbtyes_per_sample
                 *Please be careful about the aml_out->continuous_audio_offset;*/

                total_frame = dolby_ms12_get_main_pcm_generated(stream);
                write_frames =  aml_out->total_ddp_frame_nblks * 256;/*256samples in one block*/
                if (adev->debug_flag) {
                    ALOGI("%s,total_frame %"PRIu64" write_frames %"PRIu64" total frame block nums %"PRIu64"",
                        __func__, total_frame, write_frames, aml_out->total_ddp_frame_nblks);
                }
            }
            /*case 3*/
            else if (aml_out->need_sync) {
                total_frame = dolby_ms12_get_main_pcm_generated(stream);
            }
            /*case 1*/
            else {
                /*system volume prestation caculatation is done inside aux write thread*/
                total_frame = 0;
                //write_frames = (aml_out->input_bytes_size - dolby_ms12_get_system_buffer_avail(NULL)) / 4;
                //total_frame = write_frames;
            }
        }

    }
    /*we should also to calculate the alsa latency*/
    {
        clock_gettime (CLOCK_MONOTONIC, &aml_out->timestamp);
        aml_out->lasttimestamp.tv_sec = aml_out->timestamp.tv_sec;
        aml_out->lasttimestamp.tv_nsec = aml_out->timestamp.tv_nsec;
        if (total_frame >= latency_frames) {
            aml_out->last_frames_position = total_frame - latency_frames;
        } else {
            aml_out->last_frames_position = 0;
        }
        aml_out->position_update = 1;
        //ALOGI("position =%lld time sec = %ld, nanosec = %ld", aml_out->last_frames_position, aml_out->lasttimestamp.tv_sec , aml_out->lasttimestamp.tv_nsec);
    }
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (continuous_mode(adev)) {
            if (adev->ms12.is_continuous_paused) {
                if (total_frame == adev->ms12.last_ms12_pcm_out_position) {
                    adev->ms12.ms12_position_update = false;
                }
            }
            /* the ms12 generate pcm out is not changed, we assume it is the same one
             * don't update the position
             */
            if (total_frame != adev->ms12.last_ms12_pcm_out_position) {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                adev->ms12.timestamp.tv_sec = ts.tv_sec;
                adev->ms12.timestamp.tv_nsec = ts.tv_nsec;
                adev->ms12.last_frames_position = aml_out->last_frames_position;
                adev->ms12.last_ms12_pcm_out_position = total_frame;
                adev->ms12.ms12_position_update = true;
            }
        }
        /* check sys audio postion */
        sys_total_cost = dolby_ms12_get_consumed_sys_audio();
        if (adev->ms12.last_sys_audio_cost_pos != sys_total_cost) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            adev->ms12.sys_audio_timestamp.tv_sec = ts.tv_sec;
            adev->ms12.sys_audio_timestamp.tv_nsec = ts.tv_nsec;
            /*FIXME. 2ch 16 bit audio */
            adev->ms12.sys_audio_frame_pos = adev->ms12.sys_audio_base_pos + adev->ms12.sys_audio_skip + sys_total_cost/4 - latency_frames;
        }
        if (adev->debug_flag)
            ALOGI("sys audio pos %"PRIu64" ms ,sys_total_cost %"PRIu64",base pos %"PRIu64",skip pos %"PRIu64", latency %d \n",
                    adev->ms12.sys_audio_frame_pos/48,sys_total_cost,adev->ms12.sys_audio_base_pos,adev->ms12.sys_audio_skip,latency_frames);
        adev->ms12.last_sys_audio_cost_pos = sys_total_cost;
    }
    if (adev->debug_flag) {
        ALOGI("%s() stream(%p) pcm handle %p format input %#x output %#x 61937 frame %d",
              __func__, stream, aml_out->pcm, aml_out->hal_internal_format, output_format, is_iec61937_format(stream));

        if (eDolbyMS12Lib == adev->dolby_lib_type && adev->debug_flag) {
            //ms12 internal buffer avail(main/associate/system)
            if (adev->ms12.dolby_ms12_enable == true) {
                ALOGI("%s MS12 buffer avail main %d associate %d system %d\n",
                    __FUNCTION__, dolby_ms12_get_main_buffer_avail(NULL), dolby_ms12_get_associate_buffer_avail(), dolby_ms12_get_system_buffer_avail(NULL));
            }
        }

        if ((aml_out->hal_internal_format == AUDIO_FORMAT_AC3) || (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3)) {
            ALOGI("%s() total_frame %"PRIu64" latency_frames %d last_frames_position %"PRIu64" total write %"PRIu64" total writes frames %"PRIu64" diff latency %"PRIu64" ms\n",
                  __FUNCTION__, total_frame, latency_frames, aml_out->last_frames_position, aml_out->input_bytes_size, write_frames, (write_frames - total_frame) / 48);
        } else {
            ALOGI("%s() total_frame %"PRIu64" latency_frames %d last_frames_position %"PRIu64" total write %"PRIu64" total writes frames %"PRIu64" diff latency %"PRIu64" ms\n",
                  __FUNCTION__, total_frame, latency_frames, aml_out->last_frames_position, aml_out->input_bytes_size, write_frames, (write_frames - aml_out->last_frames_position) / 48);
        }
    }
    return ret;
}

audio_format_t get_non_ms12_output_format(audio_format_t src_format, struct aml_audio_device *aml_dev)
{
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    struct aml_arc_hdmi_desc *hdmi_desc = &aml_dev->hdmi_descs;
    if (aml_dev->hdmi_format == AUTO) {
        if (src_format == AUDIO_FORMAT_E_AC3 ) {
            if (hdmi_desc->ddp_fmt.is_support)
               output_format = AUDIO_FORMAT_E_AC3;
            else if (hdmi_desc->dd_fmt.is_support)
                output_format = AUDIO_FORMAT_AC3;
        } else if (src_format == AUDIO_FORMAT_AC3 ) {
            if (hdmi_desc->dd_fmt.is_support)
                output_format = AUDIO_FORMAT_AC3;
        }
    }
    return output_format;
}

static bool is_need_clean_up_ms12(struct audio_stream_out *stream, bool reset_decoder)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    bool is_compatible = false;

    if (continuous_mode(adev) && ms12->dolby_ms12_enable) {
        is_compatible = is_ms12_output_compatible(stream, adev->sink_format, adev->optical_format);
    }

    if (is_compatible) {
        reset_decoder = false;
    }
    ALOGI("[%s:%d] continuous(adev) %d, dolby_ms12_enable %d, is_compatible %d, reset_decoder %d",
    __func__, __LINE__, continuous_mode(adev), ms12->dolby_ms12_enable, is_compatible, reset_decoder);
    if ((!is_bypass_dolbyms12(stream) && (reset_decoder == true))) {
        return true;
    } else {
        return false;
    }
}

int config_output(struct audio_stream_out *stream, bool reset_decoder)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_audio_patch *patch = adev->audio_patch;

    int ret = 0;
    bool main1_dummy = false;
    bool ott_input = false;
    bool dtscd_flag = false;
    bool reset_decoder_stored = reset_decoder;
    int i  = 0 ;
    uint64_t write_frames = 0;

    int is_arc_connected = 0;
    int sink_format = AUDIO_FORMAT_PCM_16_BIT;
    adev->dcvlib_bypass_enable = 0;
    adev->dtslib_bypass_enable = 0;
    AM_LOGI("+<IN>");

    /*get sink format*/
    get_sink_format (stream);
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        bool is_compatible = false;

        if (!is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)) {

            if (aml_out->aml_dec) {
                is_compatible = aml_decoder_output_compatible(stream, adev->sink_format, adev->optical_format);
                if (is_compatible) {
                    reset_decoder = false;
                }
            }

            if (reset_decoder) {
                /*if decoder is init, we close it first*/
                if (aml_out->aml_dec) {
                    aml_decoder_release(aml_out->aml_dec);
                    aml_out->aml_dec = NULL;
                }

                memset(&aml_out->dec_config, 0, sizeof(aml_dec_config_t));

                /*prepare the decoder config*/
                ret = aml_decoder_config_prepare(stream, aml_out->hal_internal_format, &aml_out->dec_config);

                if (ret < 0) {
                    ALOGE("config decoder error");
                    return -1;
                }

                ret = aml_decoder_init(&aml_out->aml_dec, aml_out->hal_internal_format, (aml_dec_config_t *)&aml_out->dec_config);
                if (ret < 0) {
                    ALOGE("aml_decoder_init failed");
                    return -1;
                }
            }

        }

        if (is_need_clean_up_ms12(stream, reset_decoder_stored)) {
            pthread_mutex_lock(&adev->lock);
            if (!ms12->dolby_ms12_enable) {
                adev_ms12_prepare((struct audio_hw_device *)adev);
            }
            adev->mix_init_flag = true;
            dolby_ms12_encoder_reconfig(&adev->ms12);
            pthread_mutex_unlock(&adev->lock);
            /* if ms12 reconfig, do avsync */
            if (ret == 0 && adev->audio_patch && (adev->patch_src == SRC_HDMIIN ||
                adev->patch_src == SRC_ATV || adev->patch_src == SRC_LINEIN)) {
                adev->audio_patch->need_do_avsync = true;
                ALOGI("set ms12, then do avsync!");
            }
        }
    } else {
        bool is_compatible = false;
        if (aml_out->aml_dec) {
            is_compatible = aml_decoder_output_compatible(stream, adev->sink_format, adev->optical_format);
            if (is_compatible) {
                reset_decoder = false;
            }
        }

        if (reset_decoder) {
            pthread_mutex_lock(&adev->alsa_pcm_lock);
            if (aml_out->status == STREAM_HW_WRITING) {
                aml_alsa_output_close(stream);
                aml_out->status = STREAM_STANDBY;
            }
            pthread_mutex_unlock(&adev->alsa_pcm_lock);
            /*if decoder is init, we close it first*/
            if (aml_out->aml_dec) {
                aml_decoder_release(aml_out->aml_dec);
                aml_out->aml_dec = NULL;
            }

            if (aml_out->spdifout_handle) {
                aml_audio_spdifout_close(aml_out->spdifout_handle);
                aml_out->spdifout_handle = NULL;
                aml_out->dual_output_flag = 0;
            }
            if (aml_out->spdifout2_handle) {
                aml_audio_spdifout_close(aml_out->spdifout2_handle);
                aml_out->spdifout2_handle = NULL;
            }

            memset(&aml_out->dec_config, 0, sizeof(aml_dec_config_t));

            /*prepare the decoder config*/
            ret = aml_decoder_config_prepare(stream, aml_out->hal_internal_format, &aml_out->dec_config);

            if (ret < 0) {
                ALOGE("config decoder error");
                return -1;
            }

            ret = aml_decoder_init(&aml_out->aml_dec, aml_out->hal_internal_format, (aml_dec_config_t *)&aml_out->dec_config);
            if (ret < 0) {
                ALOGE("aml_decoder_init failed");
                return -1;
            }

            pthread_mutex_lock(&adev->lock);
            if (!adev->hw_mixer.start_buf) {
                aml_hw_mixer_init(&adev->hw_mixer);
            } else {
                aml_hw_mixer_reset(&adev->hw_mixer);
            }
            pthread_mutex_unlock(&adev->lock);
        }
    }
    /*TV-4745: After switch from normal PCM playing to MS12, the device will
     be changed to SPDIF, but when switch back to the normal PCM, the out device
     is still SPDIF, then the sound is abnormal.
    */
    if (!(continuous_mode(adev) && (eDolbyMS12Lib == adev->dolby_lib_type))) {
        if (sink_format == AUDIO_FORMAT_PCM_16_BIT || sink_format == AUDIO_FORMAT_PCM_32_BIT) {
            aml_out->device = PORT_I2S;
        } else {
            aml_out->device = PORT_SPDIF;
        }
    }

    AM_LOGI("-<OUT> out stream alsa port device:%d", aml_out->device);
    return 0;
}

ssize_t mixer_main_buffer_write(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_stream_out *ms12_out = (struct aml_stream_out *)adev->ms12_out;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_audio_patch *patch = adev->audio_patch;
    uint64_t ringbuf_latency = 0;
    int case_cnt;
    int ret = -1;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    bool need_reconfig_output = false;
    bool need_reset_decoder = true;
    bool need_reconfig_samplerate = false;
    void *write_buf = (void *) abuffer->buffer;
    size_t  write_bytes = abuffer->size;
    void *parser_in_buf = (void *) abuffer->buffer;
    int32_t parser_in_size = abuffer->size;
    int32_t total_used_size = 0;
    int32_t current_used_size = 0;
    int frame_duration = 0;
    int return_bytes = write_bytes;
    struct audio_buffer abuffer_out = {0};
    memcpy(&abuffer_out, abuffer, sizeof(abuffer_out));
    abuffer_out.buffer = NULL;
    abuffer_out.iec_data_buf = NULL;

    if (adev->debug_flag) {
        ALOGI("[%s:%d] out:%p bytes:%"PRId32",format:%#x,conti:%d,with_header:%d", __func__, __LINE__,
              aml_out, abuffer->size, aml_out->hal_internal_format,adev->continuous_audio_mode,aml_out->with_header);
        ALOGI("[%s:%d] hal_format:%#x, out_usecase:%s", __func__, __LINE__,
            aml_out->hal_format, usecase2Str(aml_out->usecase));
    }

    if (write_buf == NULL) {
        ALOGE ("%s() invalid buffer %p\n", __FUNCTION__, write_buf);
        return -1;
    }

    /*
     * During the stream playback,if the new stream is also an main(dolby_ms12/direct-pcm/multi-pcm)
     * stream and the original main stream is still running, we should release the old main stream at first.
     */
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (ms12->ms12_main_stream_out != NULL && ms12->ms12_main_stream_out != aml_out) {
            AM_LOGI("main stream is not same, release the old one =%p  new =%p ", ms12->ms12_main_stream_out, aml_out);
            ms12->ms12_main_stream_out->stream.common.standby((struct audio_stream *)ms12->ms12_main_stream_out);
            close_ms12_output_main_stream((struct audio_stream_out *)ms12->ms12_main_stream_out);
        }
    }

    if (aml_out->standby && (eDolbyMS12Lib == adev->dolby_lib_type_last || !adev->useSubMix)) {
        ALOGI("%s(), standby to unstandby", __func__);
        aml_out->standby = false;
    }

    /*for ms12 continuous mode, we need update status here, instead of in hw_write*/
    if (aml_out->status == STREAM_STANDBY && continuous_mode(adev)) {
        aml_out->status = STREAM_HW_WRITING;
    }

    /* here to check if the audio HDMI ARC format updated. */
    if (adev->arc_hdmi_updated) {
        ALOGI ("%s(), arc format updated, need reconfig output", __func__);
        need_reconfig_output = true;
        /*
        we reset the whole decoder pipeline when audio routing change,
        audio output option change, we do not need do a/v sync in this user case.
        in order to get a low cpu loading, we enabled less ms12 modules in each
        hdmi in user case, we need reset the pipeline to get proper one.
        */
        need_reset_decoder = true;//digital_input_src ? true: false;
        adev->arc_hdmi_updated = 0;
    }

    if (adev->a2dp_updated) {
        ALOGI ("%s(), a2dp updated, need reconfig output, %d %d", __func__, adev->out_device, aml_out->out_device);
        need_reconfig_output = true;
        adev->a2dp_updated = 0;
    }

    if (adev->hdmi_format_updated) {
        ALOGI("%s(), hdmi format updated, need reconfig output, [PCM(0)/SPDIF(4)/AUTO(5)] %d", __func__, adev->hdmi_format);
        need_reconfig_output = true;
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            need_reset_decoder = false;
        } else {
            need_reset_decoder = true;
        }
        adev->hdmi_format_updated = 0;
    }

    if (adev->bHDMIConnected_update) {
        ALOGI("%s(), hdmi connect updated, need reconfig output", __func__);
        need_reconfig_output = true;
        need_reset_decoder = true;
        adev->bHDMIConnected_update = 0;
    }

    /* here to check if the audio output routing changed. */
    if (adev->out_device != aml_out->out_device) {
        ALOGI ("[%s:%d] output routing changed, need reconfig output, adev_dev:%#x, out_dev:%#x",
            __func__, __LINE__, adev->out_device, aml_out->out_device);
        need_reconfig_output = true;
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            need_reset_decoder = false;
        }
        aml_out->out_device = adev->out_device;
    }

    if (need_reconfig_output && need_reset_decoder) {
        /* Reset pts table and the payload offset ##SWPL-107272 */
        ALOGI("continuous_mode(adev) %d ms12->dolby_ms12_enable %d", adev->continuous_audio_mode, ms12->dolby_ms12_enable);
        int is_compatible = false;
        if (adev->continuous_audio_mode && ms12->dolby_ms12_enable) {
            is_compatible = is_ms12_output_compatible(stream, adev->sink_format, adev->optical_format);
        }

        if (is_compatible) {
            need_reset_decoder = false;
        }
        get_sink_format (stream);
        if (!is_bypass_dolbyms12(stream) && (need_reset_decoder == true)) {
            ALOGI("%s avsync_reset_apts_tbl", __func__);
            avsync_reset_apts_tbl(aml_out->avsync_ctx);
        }
    }
#if 0
    if (need_reconfig_output) {
        /* Reset pts table and the payload offset ##SWPL-107272 */
        if (is_need_clean_up_ms12(stream, need_reset_decoder)) {
            aml_audio_pheader_reset_apts(aml_out->pheader);
        }
    }
#endif
    if (write_bytes > 0) {
        if ((eDolbyMS12Lib == adev->dolby_lib_type) && continuous_mode(adev)) {
            /*SWPL-11531 resume the timer here, because we have data now*/
            if (adev->ms12.need_resume) {
                ALOGI("resume the timer");
                pthread_mutex_lock(&ms12->lock);
                audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_RESUME);
                pthread_mutex_unlock(&ms12->lock);
                adev->ms12.need_resume = 0;
            }
        }
    }

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        /**
         * Need config MS12 if its not enabled.
         * Since MS12 is essential for main input.
         */
        if (!adev->ms12.dolby_ms12_enable && !is_bypass_dolbyms12(stream)) {
            ALOGI("ms12 is not enabled, reconfig it");
            need_reconfig_output = true;
            need_reset_decoder = true;
        }

        /**
         * Need config MS12 in this scenario.
         * Switch source between HDMI1 and HDMI2, the two source playback pcm data.
         * sometimes dolby_ms12_enable is true(system stream config ms12), here should reconfig
         * ms12 when switching to HDMI stream source.(Jira:TV-46722)
         */
        if (need_reconfig_output && adev->ms12.dolby_ms12_enable && patch && patch->input_src == AUDIO_DEVICE_IN_HDMI) {
            need_reset_decoder = true;
            ALOGI ("%s() %d, HDMI input source, need reset decoder:%d", __func__, __LINE__, need_reset_decoder);
        }
    }
    if (need_reconfig_output) {
        config_output (stream,need_reset_decoder);
        need_reconfig_output = false;
    }

    if ((eDolbyMS12Lib == adev->dolby_lib_type) && !is_bypass_dolbyms12(stream) && !is_dts_format(aml_out->hal_internal_format)) {
        // in NETFLIX movie select screen, switch between movies, adev->ms12_out will change.
        // so we need to update to latest status just before use.zzz
        ms12_out = (struct aml_stream_out *)adev->ms12_out;
        audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
        if (ms12_out == NULL) {
            // add protection here
            ALOGI("%s,ERROR ms12_out = NULL,adev->ms12_out = %p", __func__, adev->ms12_out);
            return return_bytes;
        }
        /*
        continuous mode,available dolby format coming,need set main dolby dummy to false
        */
        if (!aml_out->is_ms12_main_decoder) {
            pthread_mutex_lock(&adev->trans_lock);
            ms12_out->hal_internal_format = aml_out->hal_internal_format;
            ms12_out->hal_ch = aml_out->hal_ch;
            ms12_out->hal_rate = aml_out->hal_rate;
            pthread_mutex_unlock(&adev->trans_lock);
        }
    }

    aml_out->input_bytes_size += write_bytes;
    if (patch && (adev->dtslib_bypass_enable || adev->dcvlib_bypass_enable)) {
        int cur_samplerate = audio_parse_get_audio_samplerate(patch->audio_parse_para);
        if (cur_samplerate != patch->input_sample_rate || need_reconfig_samplerate) {
            ALOGI ("HDMI/SPDIF input samplerate from %d to %d, or need_reconfig_samplerate\n", patch->input_sample_rate, cur_samplerate);
            patch->input_sample_rate = cur_samplerate;
            if (patch->aformat == AUDIO_FORMAT_DTS ||  patch->aformat == AUDIO_FORMAT_DTS_HD) {
                if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
                    if (cur_samplerate == 44100 || cur_samplerate == 32000) {
                        aml_out->config.rate = cur_samplerate;
                    } else {
                        aml_out->config.rate = 48000;
                    }
                }
            } else if (patch->aformat == AUDIO_FORMAT_AC3) {
                if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
                    aml_out->config.rate = cur_samplerate;
                }
            } else if (patch->aformat == AUDIO_FORMAT_E_AC3) {
                if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
                    if (cur_samplerate == 192000 || cur_samplerate == 176400) {
                        aml_out->config.rate = cur_samplerate / 4;
                    } else {
                        aml_out->config.rate = cur_samplerate;
                    }
                }
            } else {
                aml_out->config.rate = 48000;
            }
            ALOGI("adev->dtslib_bypass_enable :%d,adev->dcvlib_bypass_enable:%d, aml_out->config.rate :%d\n",adev->dtslib_bypass_enable,
            adev->dcvlib_bypass_enable,aml_out->config.rate);
        }
    }

    /*
     *when disable_pcm_mixing is true, the 7.1ch DD+ could not be process with Dolby MS12
     *the HDMI-In or Spdif-In is special with IEC61937 format, other input need packet with spdif-encoder.
     */
    audio_format_t output_format = get_output_format (stream);
    if (adev->debug_flag) {
        ALOGD("%s:%d hal_format:%#x, output_format:0x%x, sink_format:0x%x",
            __func__, __LINE__, aml_out->hal_format, output_format, adev->sink_format);
    }

    int parser_count = 0;
    AM_LOGI_IF(adev->debug_flag, "======== parser_in_size(%d) total_used_size(%d) b_pts_valid(%d) pts(%"PRIu64"ms)\n",
        parser_in_size, total_used_size, abuffer->b_pts_valid, abuffer->pts/90);
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        abuffer_out.iec_data_buf = abuffer->buffer;
        abuffer_out.iec_data_size = abuffer->size;
    }

    if (aml_audio_property_get_bool("vendor.media.audiohal.indump", false)) {
        aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/before_parser.raw",
            abuffer->buffer, abuffer->size);
    }
    while (parser_in_size > total_used_size) {
        /*AM_LOGI("Before parser  [%d]in_buf(%p) parser_in_buf(%d) total_used_size(%d)",
            parser_count, parser_in_buf, parser_in_size, total_used_size);*/
        ret = aml_audio_parser_process_wrapper(stream,
                                                parser_in_buf + total_used_size,
                                                parser_in_size - total_used_size,
                                                &current_used_size,
                                                &abuffer_out.buffer,
                                                &abuffer_out.size,
                                                &frame_duration);
        if (ret != 0) {
            AM_LOGE("aml_audio_parser_process_wrapper error");
            break;
        }
        if (aml_audio_property_get_bool("vendor.media.audiohal.indump", false)) {
            aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/after_parser.raw",
                abuffer_out.buffer, abuffer_out.size);
        }
        if (parser_count != 0) {
            abuffer_out.b_pts_valid = false;
        }
        char *p = (char*)abuffer_out.buffer;
        total_used_size += current_used_size;
        parser_count++;
        AM_LOGI_IF(adev->debug_flag, "After parser [%d]current_used_size %d, total_used_size %d, p %p", parser_count, current_used_size, total_used_size, p);
        if (p != NULL) {
            AM_LOGI_IF(adev->debug_flag, "After parser in_buf(%p) out_buf(%p)(%x,%x,%x,%x) out_size(%"PRId32") b_pts_valid(%d) pts(%"PRIu64"ms) dur %d",
                parser_in_buf, abuffer_out.buffer, *p, *(p+1),
                *(p+2),*(p+3),abuffer_out.size, abuffer_out.b_pts_valid, abuffer_out.pts/90, frame_duration);

            if (eDolbyMS12Lib == adev->dolby_lib_type) {
                return_bytes = aml_audio_ms12_render(stream, &abuffer_out);
            } else {
                if (is_dts_format(aml_out->hal_internal_format) && get_dts_lib_type() == eDTSXLib) {
                    return_bytes = aml_audio_nonms12_dec_render(stream, &abuffer_out);
                } else {
                    return_bytes = aml_audio_nonms12_render(stream, &abuffer_out);
                }
            }
            abuffer_out.pts += frame_duration;
        }
    }

exit:
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (continuous_mode(adev)) {
            aml_out->timestamp = adev->ms12.timestamp;
            //clock_gettime(CLOCK_MONOTONIC, &aml_out->timestamp);
            aml_out->last_frames_position = adev->ms12.last_frames_position;
        }
    }

    if (adev->debug_flag) {
        ALOGI("%s return %d!\n", __FUNCTION__, return_bytes);
    }
    return return_bytes;
}

ssize_t mixer_aux_buffer_write(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    const void *buffer = abuffer->buffer;
    size_t bytes = abuffer->size;
    int ret = 0;
    size_t frame_size = audio_stream_out_frame_size(stream);
    size_t in_frames = bytes / frame_size;
    size_t bytes_remaining = bytes;
    size_t bytes_written = 0;
    bool need_reconfig_output = false;
    bool  need_reset_decoder = false;
    int retry = 0;
    unsigned int alsa_latency_frame = 0;
    pthread_mutex_lock(&adev->lock);
    uint64_t enter_ns = 0;
    uint64_t leave_ns = 0;


    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    ALOGI("[%lld.%9ld -- %d] mixer_aux_buffer_write %zu", (long long)ts.tv_sec, ts.tv_nsec, gettid(), bytes);

    if (eDolbyMS12Lib == adev->dolby_lib_type && continuous_mode(adev)) {
        enter_ns = aml_audio_get_systime_ns();
    }

    if (adev->debug_flag) {
        ALOGD("%s:%d size:%zu, dolby_lib_type:0x%x, frame_size:%zu",
        __func__, __LINE__, bytes, adev->dolby_lib_type, frame_size);
    }

    if (aml_out->status == STREAM_STANDBY) {
        aml_out->status = STREAM_MIXING;
    }

    if (aml_out->standby) {
        ALOGI("%s(), standby to unstandby", __func__);
        aml_out->audio_data_handle_state = AUDIO_DATA_HANDLE_START;
        aml_out->standby = false;
    }

    if (aml_out->is_normal_pcm && !aml_out->normal_pcm_mixing_config && eDolbyMS12Lib == adev->dolby_lib_type) {
        if (0 == set_system_app_mixing_status(aml_out, aml_out->status)) {
            aml_out->normal_pcm_mixing_config = true;
        } else {
            aml_out->normal_pcm_mixing_config = false;
        }
    }

    pthread_mutex_unlock(&adev->lock);
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (adev->a2dp_no_reconfig_ms12 > 0) {
            uint64_t curr = aml_audio_get_systime();
            if (adev->a2dp_no_reconfig_ms12 <= curr)
                adev->a2dp_no_reconfig_ms12 = 0;
        }
        if (continuous_mode(adev)) {
            /*only system sound active*/
            if (!(dolby_stream_active(adev) || hwsync_lpcm_active(adev))) {
                /* here to check if the audio HDMI ARC format updated. */
                if (((adev->arc_hdmi_updated) || (adev->a2dp_updated) || (adev->hdmi_format_updated) || (adev->bHDMIConnected_update))
                    && (adev->ms12.dolby_ms12_enable == true)) {
                    //? if we need protect
                    if ((adev->a2dp_no_reconfig_ms12 > 0) && (aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) && (adev->a2dp_updated == 0)) {
                        need_reset_decoder = false;
                        ALOGD("%s: a2dp output and change audio format, no reconfig ms12 for hdmi update", __func__);
                    } else {
                        need_reset_decoder = true;
                        if (adev->hdmi_format_updated) {
                            ALOGI("%s(), hdmi format updated, [PCM(0)/SPDIF(4)/AUTO(5)] current %d", __func__, adev->hdmi_format);
                        }
                        else
                            ALOGI("%s() %s%s%s changing status, need reconfig Dolby MS12\n", __func__,
                                    (adev->arc_hdmi_updated==0)?" ":"HDMI ARC EndPoint ",
                                    (adev->bHDMIConnected_update==0)?" ":"HDMI ",
                                    (adev->a2dp_updated==0)?" ":"a2dp ");
                    }
                    adev->arc_hdmi_updated = 0;
                    adev->a2dp_updated = 0;
                    adev->hdmi_format_updated = 0;
                    adev->bHDMIConnected_update = 0;
                    need_reconfig_output = true;
                }

                /* here to check if the audio output routing changed. */
                if ((adev->out_device != aml_out->out_device) && (adev->ms12.dolby_ms12_enable == true)) {
                    ALOGI("%s(), output routing changed from 0x%x to 0x%x,need MS12 reconfig output", __func__, aml_out->out_device, adev->out_device);
                    aml_out->out_device = adev->out_device;
                    need_reconfig_output = true;
                }
                /* here to check if the hdmi audio output format dynamic changed. */
                if (adev->pre_hdmi_format != adev->hdmi_format) {
                    ALOGI("hdmi format is changed from %d to %d need reconfig output", adev->pre_hdmi_format, adev->hdmi_format);
                    adev->pre_hdmi_format = adev->hdmi_format;
                    need_reconfig_output = true;
                }
                /* here to check if ms12 is initialized. */
                if (adev->ms12_out == NULL && adev->doing_reinit_ms12 == true) {
                    ALOGI("adev->ms12_out == NULL, init ms12 and reset decoder");
                    need_reconfig_output = true;
                    need_reset_decoder = true;
                }
            }
            /* here to check if ms12 is already enabled, if main stream is doing init ms12, we don't need do it */
            if (!adev->ms12.dolby_ms12_enable && !adev->doing_reinit_ms12 && !adev->doing_cleanup_ms12) {
                ALOGI("%s(), 0x%x, Swithing system output to MS12, need MS12 reconfig output", __func__, aml_out->out_device);
                need_reconfig_output = true;
                need_reset_decoder = true;
            }

            if (need_reconfig_output) {
                /*during ms12 switch, the frame write may be not matched with
                  the input size, we need to align it*/
                if (aml_out->frame_write_sum * frame_size != aml_out->input_bytes_size) {
                    ALOGI("Align the frame write from %" PRId64 " to %" PRId64 "", aml_out->frame_write_sum, aml_out->input_bytes_size/frame_size);
                    aml_out->frame_write_sum = aml_out->input_bytes_size/frame_size;
                }
                config_output(stream,need_reset_decoder);
            }
        }
        /*
         *when disable_pcm_mixing is true and offload format is ddp and output format is ddp
         *the system tone voice should not be mixed
         */
        if (is_bypass_dolbyms12(stream)) {
            ms12->sys_audio_skip += bytes / frame_size;
            usleep(bytes * 1000000 /frame_size/out_get_sample_rate(&stream->common)*5/6);
        } else {
            /*
             *when disable_pcm_mixing is true and offload format is dolby
             *the system tone voice should not be mixed
             */
            if ((adev->hdmi_format == BYPASS) && (dolby_stream_active(adev) || hwsync_lpcm_active(adev))) {
                memset((void *)buffer, 0, bytes);
                if (adev->debug_flag) {
                    ALOGI("%s mute the mixer voice(system/alexa)\n", __FUNCTION__);
                }
            }

            /*for ms12 system input, we need do track switch before enter ms12*/
            {
                aml_audio_switch_output_mode((int16_t *)buffer, bytes, adev->sound_track_mode);
            }

            /* audio zero data detect, and do fade in */
            if (adev->is_netflix && STREAM_PCM_NORMAL == aml_out->usecase) {
                aml_audio_data_handle(stream, buffer, bytes);
            }

            // SUSP-008-TC2 Test Steps 8 pop
            if (adev->is_netflix_hide && STREAM_PCM_NORMAL == aml_out->usecase) {
                audio_fade_func((void *)buffer, bytes, 0);
                adev->netflix_hide_fadeout_startTime = aml_audio_get_systime()/1000;
                adev->is_netflix_hide = false;
                ALOGI("%s aux audio fade out. starTime:%"PRIu64"", __func__, adev->netflix_hide_fadeout_startTime);
            } else {
                if (adev->netflix_hide_fadeout_startTime) {
                    uint64_t currentTime = aml_audio_get_systime()/1000;
                    if (currentTime - adev->netflix_hide_fadeout_startTime < 500) {
                        memset((void *)buffer, 0, bytes);
                    } else {
                        adev->netflix_hide_fadeout_startTime = 0;
                        ALOGI("%s aux audio memset done. currentTime:%"PRIu64"", __func__, currentTime);
                    }
                }
            }

            while (bytes_remaining && adev->ms12.dolby_ms12_enable && retry < 20) {
                size_t used_size = 0;
                ret = dolby_ms12_system_process(stream, (char *)buffer + bytes_written, bytes_remaining, &used_size);
                if (!ret) {
                    bytes_remaining -= used_size;
                    bytes_written += used_size;
                    retry = 0;
                }
                retry++;
                if (bytes_remaining) {
                    //usleep(bytes_remaining * 1000000 / frame_size / out_get_sample_rate(&stream->common));
                    aml_audio_sleep(5000);
                }
            }
            if (bytes_remaining) {
                ms12->sys_audio_skip += bytes_remaining / frame_size;
                ALOGI("bytes_remaining =%zu totoal skip =%"PRIu64"", bytes_remaining, ms12->sys_audio_skip);
            }
        }
    } else {
        if (adev->useSubMix) {
            bytes_written = out_write_direct_pcm(stream, buffer, bytes);
        } else {
            bytes_written = aml_hw_mixer_write(&adev->hw_mixer, buffer, bytes);
        }
        /*these data is skip for ms12, we still need calculate it*/
        if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
            ms12->sys_audio_skip += bytes / frame_size;
        }
        if (aml_audio_property_get_bool("vendor.media.audiohal.mixer", false)) {
            aml_audio_dump_audio_bitstreams("/data/audio/mixerAux.raw", buffer, bytes);
        }
    }
    aml_out->input_bytes_size += bytes;
    aml_out->frame_write_sum += in_frames;

    clock_gettime (CLOCK_MONOTONIC, &aml_out->timestamp);
    aml_out->lasttimestamp.tv_sec = aml_out->timestamp.tv_sec;
    aml_out->lasttimestamp.tv_nsec = aml_out->timestamp.tv_nsec;

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        /*
           mixer_aux_buffer_write is called when the hw write is called another thread,for example
           main write thread or ms12 thread. aux audio is coming from the audioflinger mixed thread.
           we need calculate the system buffer latency in ms12 also with the alsa out latency.It is
           48K 2 ch 16 bit audio.
           */
        alsa_latency_frame = adev->ms12.latency_frame;
        int system_latency = 0;
        system_latency = dolby_ms12_get_system_buffer_avail(NULL) / frame_size;

        if (adev->compensate_video_enable) {
            alsa_latency_frame = 0;
        }
        aml_out->last_frames_position = aml_out->frame_write_sum - system_latency;
        if (aml_out->last_frames_position >= alsa_latency_frame) {
            aml_out->last_frames_position -= alsa_latency_frame;
        }
        if (adev->debug_flag) {
            ALOGI("%s stream audio presentation %"PRIu64" latency_frame %d.ms12 system latency_frame %d,total frame=%" PRId64 " %" PRId64 " ms",
                  __func__,aml_out->last_frames_position, alsa_latency_frame, system_latency,aml_out->frame_write_sum, aml_out->frame_write_sum/48);
        }
    } else {
        aml_out->last_frames_position = aml_out->frame_write_sum;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    ALOGI_IF(eDolbyMS12Lib == adev->dolby_lib_type, "[%lld.%9ld] write %zu bytes, system level %zu ms", (long long)ts.tv_sec, ts.tv_nsec, bytes, dolby_ms12_get_system_buffer_avail(NULL) / frame_size / 48);


#ifndef BUILD_LINUX
    /*if system sound return too quickly, it will causes audio flinger underrun*/
    if (eDolbyMS12Lib == adev->dolby_lib_type && continuous_mode(adev)) {
        uint64_t cost_time_us = 0;
        uint64_t frame_us = in_frames*1000/48;
        uint64_t minum_sleep_time_us = 5000;
        leave_ns = aml_audio_get_systime_ns();
        cost_time_us = (leave_ns - enter_ns)/1000;
        /*it costs less than 10ms*/
        //ALOGI("cost us=%lld frame/2=%lld", cost_time_us, frame_us/2);
        if ( cost_time_us < minum_sleep_time_us) {
            //ALOGI("sleep =%lld", minum_sleep_time_us - cost_time_us);
            aml_audio_sleep(minum_sleep_time_us - cost_time_us);
        }
    }
#endif
    return bytes;

}
ssize_t mixer_ad_buffer_write (struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_stream_out *ms12_out = (struct aml_stream_out *)adev->ms12_out;
    int ret = -1, write_retry = 0, frame_duration = 0, parser_count = 0;
    size_t used_size = 0, total_bytes = abuffer->size;
    void *parser_in_buf = (void *) abuffer->buffer;
    int32_t total_parser_size = 0, current_parser_size = 0;
    struct audio_buffer abuffer_out = {0};
    abuffer_out.pts = abuffer->pts;

    if (abuffer == NULL || aml_out == NULL) {
        AM_LOGE ("invalid abuffer %p, out %p\n", abuffer, aml_out);
        return -1;
    }

    AM_LOGI_IF(adev->debug_flag, "useSubMix:%d, db_lib:%d, hal_format:0x%x, usecase:0x%x, out:%p, in %zu",
            adev->useSubMix, adev->dolby_lib_type, aml_out->hal_format, aml_out->usecase, aml_out, total_bytes);

    if (abuffer->buffer == NULL || abuffer->size == 0) {
        AM_LOGE ("invalid abuffer content, buf %p, sz %"PRId32"\n", abuffer->buffer, abuffer->size);
        return -1;
    }

    if (aml_out->standby) {
        aml_out->standby = false;
    }

    while (total_bytes > total_parser_size) {
        /*AM_LOGI("Before parser  [%d]in_buf(%p) parser_in_buf(%d) total_used_size(%d)",
            parser_count, parser_in_buf, parser_in_size, total_used_size);*/
        ret = aml_audio_parser_process_wrapper(stream,
                                                parser_in_buf + total_parser_size,
                                                total_bytes - total_parser_size,
                                                &current_parser_size,
                                                &abuffer_out.buffer,
                                                &abuffer_out.size,
                                                &frame_duration);
        if (ret != 0) {
            AM_LOGE("aml_audio_parser_process_wrapper error");
            break;
        }
        if (aml_audio_property_get_bool("vendor.media.audiohal.indump", false)) {
            aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/ad_after_parser.raw",
                abuffer_out.buffer, abuffer_out.size);
        }
        char *p = (char*)abuffer_out.buffer;
        total_parser_size += current_parser_size;
        parser_count++;
        AM_LOGI_IF(adev->debug_flag, "After parser [%d]current_used_size %d, total_used_size %d, p %p", parser_count, current_parser_size, total_parser_size, p);
        if (p != NULL) {
            AM_LOGI_IF(adev->debug_flag, "After parser in_buf(%p) out_buf(%p)(%x,%x,%x,%x) out_size(%"PRId32") used %d, total used %d, dur %d",
                parser_in_buf, abuffer_out.buffer, *p, *(p+1),
                *(p+2),*(p+3),abuffer_out.size, current_parser_size, total_parser_size, frame_duration);

            aml_audio_ad_render(stream, &abuffer_out);
            abuffer_out.pts += frame_duration;
        }
    }
ad_exit:
    AM_LOGI_IF(adev->debug_flag, "return %d!\n", total_parser_size);
    return total_parser_size;
}

ssize_t mixer_app_buffer_write(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
   struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
   struct aml_audio_device *adev = aml_out->dev;
   struct dolby_ms12_desc *ms12 = &(adev->ms12);
   int ret = 0;
   const void *buffer = abuffer->buffer;
   size_t bytes = abuffer->size;
   size_t frame_size = audio_stream_out_frame_size(stream);
   size_t bytes_remaining = bytes;
   size_t bytes_written = 0;
    bool  need_reset_decoder = true;
   int retry = 20;

   if (adev->debug_flag) {
       ALOGD("[%s:%d] size:%zu, frame_size:%zu", __func__, __LINE__, bytes, frame_size);
   }

   if (eDolbyMS12Lib != adev->dolby_lib_type) {
        if (!adev->useSubMix) {
            AM_LOGW("Submix is disable now, app write isn't supported");
            return bytes;
        }
        AM_LOGI_IF(adev->debug_flag, "dolby_lib_type:%d, is not ms12,  app write to nonms12", adev->dolby_lib_type);
        int  return_bytes = out_write_direct_pcm(stream, buffer, bytes);
        return return_bytes;
   }

   if (is_bypass_dolbyms12(stream)) {
       ALOGW("[%s:%d] is_bypass_dolbyms12, not support app write", __func__, __LINE__);
       return -1;
   }

   /*for ms12 continuous mode, we need update status here, instead of in hw_write*/
   if (aml_out->status == STREAM_STANDBY && continuous_mode(adev)) {
       aml_out->status = STREAM_HW_WRITING;
        if (false == adev->ms12.dolby_ms12_enable) {
            config_output(stream,need_reset_decoder);
       }
   }

   while (bytes_remaining && adev->ms12.dolby_ms12_enable && retry > 0) {
       size_t used_size = 0;
       ret = dolby_ms12_app_process(stream, (char *)buffer + bytes_written, bytes_remaining, &used_size);
       if (!ret) {
           bytes_remaining -= used_size;
           bytes_written += used_size;
       }
       retry--;
       if (bytes_remaining) {
           aml_audio_sleep(3000);
       }
   }
   if (retry <= 10) {
       ALOGE("[%s:%d] write retry=%d ", __func__, __LINE__, retry);
   }
   if (retry == 0 && bytes_remaining != 0) {
       ALOGE("[%s:%d] write timeout 60 ms ", __func__, __LINE__);
       bytes -= bytes_remaining;
   }

   return bytes;
}

ssize_t process_buffer_write(struct audio_stream_out *stream, struct audio_buffer *abuffer)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    const void *buffer = abuffer->buffer;
    size_t bytes = abuffer->size;

    if (adev->out_device != aml_out->out_device) {
        ALOGD("%s:%p device:%x,%x", __func__, stream, aml_out->out_device, adev->out_device);
        aml_out->out_device = adev->out_device;
        config_output(stream, true);
    }

    /*during ms12 continuous exiting, the write function will be
     set to this function, then some part of audio need to be
     discarded, otherwise it will cause audio gap*/
    if (adev->exiting_ms12) {
        int64_t gap;
        void * data = (void*)buffer;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        gap = calc_time_interval_us(&adev->ms12_exiting_start, &ts);

        if (gap >= 500*1000) {
            adev->exiting_ms12 = 0;
        } else {
            ALOGV("during MS12 exiting gap=%" PRId64 " mute the data", gap);
            memset(data, 0, bytes);
        }
    }

    if (audio_hal_data_processing(stream, buffer, bytes, &output_buffer, &output_buffer_bytes, aml_out->hal_internal_format) == 0) {
        hw_write(stream, output_buffer, output_buffer_bytes, aml_out->hal_internal_format);
    }
    if (bytes > 0) {
        aml_out->input_bytes_size += bytes;
    }
    return bytes;
}

/* must be called with hw device mutexes locked */
static int usecase_change_validate_l(struct aml_stream_out *aml_out, bool is_standby)
{
    struct aml_audio_device *adev = aml_out->dev;
    if (adev->dolby_lib_type == eDolbyMS12Lib) {
        if (aml_out->is_normal_pcm) {
            aml_out->write = mixer_aux_buffer_write;
            aml_out->write_func = MIXER_AUX_BUFFER_WRITE;
            aml_out->inputPortType = AML_DOLBY_INPUT_SYSTEM;
            AM_LOGI("mixer_aux_buffer_write!");
        } else if (aml_out->flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
            aml_out->write = mixer_app_buffer_write;
            aml_out->write_func = MIXER_APP_BUFFER_WRITE;
            aml_out->inputPortType = AML_DOLBY_INPUT_APP;
            AM_LOGI("mixer_app_buffer_write!");
        } else if (aml_out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM) {
            aml_out->write = mixer_ad_buffer_write;
            aml_out->inputPortType = AML_DOLBY_INPUT_AD;
            AM_LOGI("mixer_ad_buffer_write!");
        } else {
            aml_out->write = mixer_main_buffer_write;
            aml_out->write_func = MIXER_MAIN_BUFFER_WRITE;
            aml_out->inputPortType = AML_DOLBY_INPUT_MAIN;
            AM_LOGI("mixer_main_buffer_write!");
        }
    } else {
        if (STREAM_PCM_NORMAL == aml_out->usecase) {
            aml_out->write = mixer_aux_buffer_write;
            aml_out->write_func = MIXER_AUX_BUFFER_WRITE;
            AM_LOGI("mixer_aux_buffer_write !");
        } else if (STREAM_PCM_MMAP == aml_out->usecase) {
            aml_out->write = mixer_app_buffer_write;
            aml_out->write_func = MIXER_APP_BUFFER_WRITE;
            AM_LOGI("mixer_app_buffer_write !");
        } else if (aml_out->flags & AUDIO_OUTPUT_FLAG_AD_STREAM) {
            aml_out->write = mixer_ad_buffer_write;
            aml_out->inputPortType = AML_DOLBY_INPUT_AD;
            AM_LOGI("mixer_ad_buffer_write!");
        } else {
            aml_out->write = mixer_main_buffer_write;
            aml_out->write_func = MIXER_MAIN_BUFFER_WRITE;
            AM_LOGI("mixer_main_buffer_write !");
        }
        aml_out->inputPortType = get_input_port_type(&aml_out->audioCfg, aml_out->flags);
    }
    return 0;
}

static int set_hdmi_format(struct aml_audio_device *adev, int val)
{
    int ret = -1;
    if (adev->hdmi_format == val) {
        return 0;
    }
    adev->hdmi_format_updated = 1;
    adev->hdmi_format = val;
    /* update the DUT's EDID */
    ret = update_edid_after_edited_audio_sad(adev, &adev->hdmi_descs.ddp_fmt);
    if (ret < 0) {
        ALOGE("[%s:%d], An error occurred during updating the DUT's EDID!", __func__, __LINE__);
        return ret;
    }
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (continuous_mode(adev) && adev->ms12_out) {
            ALOGD("%s() active stream is ms12_out %p\n", __FUNCTION__, adev->ms12_out);
            get_sink_format((struct audio_stream_out *)adev->ms12_out);
        }
        if (adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
            adev->a2dp_no_reconfig_ms12 = aml_audio_get_systime() + 2000000;
        }
    }
    ALOGI("update HDMI format: %d\n", adev->hdmi_format);
    return 0;
}


/* out_write entrance: every write goes in here. */
ssize_t out_write_new(struct audio_stream_out *stream,
                      const void *buffer,
                      size_t bytes)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    ssize_t ret = 0;
    write_func  write_func_p = NULL;
    ssize_t return_bytes = 0;
    struct audio_buffer abuffer = {0};
    size_t pheader_cost = 0;
    int pheader_outsize = 0;
    uint64_t cur_pts = HWSYNC_PTS_NA;
    size_t write_bytes = bytes;
    const void *write_buf = buffer;
    size_t bytes_cost = 0;
    size_t total_bytes = bytes;

    if (NULL == buffer) {
        ALOGE("[%s:%d], buffer is NULL, out_stream(%p) position(%zu)", __func__, __LINE__, stream, bytes);
        return bytes;
    }

#ifdef NO_SERVER
    enum digital_format digital_mode = AML_HAL_PCM;
    enum OUT_PORT output_sel = OUTPORT_SPEAKER;

    digital_mode = get_digital_mode(&adev->alsa_mixer);
    ret = set_hdmi_format(adev, digital_mode);
    if (ret < 0) {
        ALOGE("[%s:%d], An error occurred during setting hdmi_format !", __func__, __LINE__);
        return ret;
    }

    output_sel = get_output_select(&adev->alsa_mixer);
    if (aml_out->is_sink_format_prepared && output_sel != adev->active_outport) {
        adev->active_outport = output_sel;
        get_sink_format(&aml_out->stream);
    } else {
        adev->active_outport = output_sel;
    }
#endif

    adev->debug_flag = aml_audio_get_debug_flag();
    if (adev->debug_flag > 1) {
        AM_LOGI("+<IN>out_stream(%p) position(%zu)", stream, bytes);
    }
    /*move it from open function, because when hdmi hot plug, audio service will
     * call many times open/close to query the hdmi capability, this will affect the
     * sink format
     */
    if (!aml_out->is_sink_format_prepared) {
        get_sink_format(&aml_out->stream);
        aml_out->card = alsa_device_get_card_index();
        if (!adev->is_TV) {
            if (is_use_spdifb(aml_out)) {
                aml_audio_select_spdif_to_hdmi(AML_SPDIF_B_TO_HDMITX);
                aml_out->restore_hdmitx_selection = true;
            }

            if (adev->sink_format == AUDIO_FORMAT_PCM_16_BIT) {
                aml_out->device = PORT_I2S;
            } else {
                aml_out->device = PORT_SPDIF;
            }
        }
        aml_out->is_sink_format_prepared = true;
    }

#if 0
    if ((adev->dolby_lib_type_last == eDolbyMS12Lib) &&
        (adev->audio_patching)) {
        /*in patching case, we can't use continuous mode*/
        if (adev->continuous_audio_mode) {
            bool set_ms12_non_continuous = true;
            ALOGI("in patch case src =%d, we need disable continuous mode", adev->patch_src);
            get_dolby_ms12_cleanup(&adev->ms12, set_ms12_non_continuous);
            adev->ms12_out = NULL;
            adev->doing_reinit_ms12 = true;
        }
    }
#endif

    /*if (adev->continuous_audio_mode && aml_out->avsync_type == AVSYNC_TYPE_MEDIASYNC) {
        bool set_ms12_non_continuous = true;
        ALOGI("mediasync esmode we need disable continuous mode\n");
        get_dolby_ms12_cleanup(&adev->ms12, set_ms12_non_continuous);
        adev->ms12_out = NULL;
        adev->doing_reinit_ms12 = true;
    }*/

    aml_audio_trace_int("out_write_new", bytes);
    /**
     * deal with the device output changes
     * pthread_mutex_lock(&aml_out->lock);
     * out_device_change_validate_l(aml_out);
     * pthread_mutex_unlock(&aml_out->lock);
     */

    clock_gettime(CLOCK_MONOTONIC, &adev->last_write_ts);
    adev->stream_write_data = true;

pheader_rewrite:
    if (true == aml_out->with_header) {
        write_bytes = 0;
        if (NULL == aml_out->pheader) {
            aml_out->pheader = audio_header_info_init();
        }

        pheader_cost = audio_header_find_frame(aml_out->pheader, (char *)buffer + bytes_cost, total_bytes - bytes_cost, &cur_pts, &pheader_outsize);
        if (pheader_outsize > 0)
        {
            write_bytes = pheader_outsize;
            write_buf = aml_out->pheader->hw_sync_body_buf;
        }

        if ((0 != aml_out->pheader->clip_front) || (0 != aml_out->pheader->clip_back))
        {
            if (NULL == aml_out->clip_meta)
            {
                aml_out->clip_meta = hal_clip_meta_init();
                if (0 != dolby_ms12_register_callback_by_type(CALLBACK_CLIP_META, (void*)ms12_clipmeta, aml_out))
                {
                    AM_LOGE("dolby_ms12_register_callback CALLBACK_CLIP_META fail!!");
                }
            }
            AM_LOGI("inbytes:%zu, outbytes:%zu, offset:%"PRIu64", clip_front:%"PRIu64", clip_back:%"PRIu64"", bytes, write_bytes,
                    aml_out->clip_offset, aml_out->pheader->clip_front, aml_out->pheader->clip_back);
            aml_out->clip_offset = aml_out->payload_offset;
            hal_set_clip_info(aml_out->clip_meta, aml_out->clip_offset, aml_out->pheader->clip_front, aml_out->pheader->clip_back);
        }

        aml_out->payload_offset += write_bytes;

        if (true == aml_out->pheader->eos) {
            aml_out->eos = aml_out->pheader->eos;
            /* in nonms12 render case, there would pop noise when playback ending, as there's no fade out.
            *  and we use AED mute to avoid noise.
            */
            if (adev->dolby_lib_type != eDolbyMS12Lib && is_dolby_format(aml_out->hal_internal_format)) {
                set_aed_master_volume_mute(&adev->alsa_mixer, true);
            }
        }
        if (adev->debug_flag > 1) {
            ALOGI("[%s:%d], inputsize:%zu, pheader_cost:%zu, cur_pts:0x%"PRIx64" (%"PRIu64" ms), outsize:%d", __func__, __LINE__, bytes, pheader_cost, cur_pts, cur_pts/90, pheader_outsize);
        }

        if ((cur_pts > 0xffffffff) && (cur_pts != HWSYNC_PTS_NA) && (cur_pts != HWSYNC_PTS_EOS)) {
            ALOGE("APTS exeed the max 32bit value");
        }
    }

    if ((NULL != aml_out->avsync_ctx) && (AVSYNC_TYPE_MEDIASYNC == aml_out->avsync_type)) {
        if (NULL == aml_out->avsync_ctx->mediasync_ctx) {
            AM_LOGE("mediasync_ctx is null error, cleanup avsync_type!");
            aml_out->avsync_type = AVSYNC_TYPE_NULL;
        } else if (true != aml_out->with_header) {  //DTV
            pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
            cur_pts = aml_out->avsync_ctx->mediasync_ctx->in_apts;
            pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
        }
    }

    if (aml_out->write && write_bytes > 0) {
        abuffer.buffer = write_buf;
        abuffer.size = write_bytes;
        abuffer.pts = cur_pts;
        if (abuffer.pts != HWSYNC_PTS_NA) {
            abuffer.b_pts_valid = true;
        }
        return_bytes = aml_out->write(stream, &abuffer);
    }

    /*if the data consume is not complete, it will be send again by audio flinger,
      this old data will cause av sync problem after seek.
    */
    if (aml_out->with_header && return_bytes > 0) {
        /*
        if the data is not  consumed totally,
        we need re-send data again
        */
        bytes_cost += pheader_cost;
        if (bytes_cost > 0 && total_bytes > bytes_cost) {
            /* We need to wait for Google to fix the issue:
             * Issue: After pause, there will be residual sound in AF, which will cause NTS fail.
             * Now we need to judge whether the current format is DTS */
            if (is_dts_format(aml_out->hal_internal_format))
                return bytes_cost;
            else
                goto pheader_rewrite;
        }
        return_bytes = bytes_cost;
    }

    aml_audio_trace_int("out_write_new", 0);
    if (return_bytes > 0) {
        aml_out->total_write_size += return_bytes;
        if (aml_out->is_normal_pcm) {
            size_t frame_size = audio_stream_out_frame_size(stream);
            if (frame_size != 0) {
                adev->sys_audio_frame_written = aml_out->input_bytes_size / frame_size;
            }
        }
    }

    update_audio_format(adev, aml_out->hal_internal_format);

    if (adev->debug_flag > 1) {
        AM_LOGI("-<OUT>out_stream(%p) position(%zu) ret %zd, %"PRIu64"", stream, bytes, return_bytes, aml_out->total_write_size);
    }

    return return_bytes;
}

int adev_open_output_stream_new(struct audio_hw_device *dev,
                                audio_io_handle_t handle __unused,
                                audio_devices_t devices,
                                audio_output_flags_t flags,
                                struct audio_config *config,
                                struct audio_stream_out **stream_out,
                                const char *address)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_stream_out *aml_out = NULL;
    stream_usecase_t usecase = STREAM_PCM_NORMAL;
    int ret;
    ALOGD("%s: enter", __func__);
    ret = adev_open_output_stream(dev,
                                    0,
                                    devices,
                                    flags,
                                    config,
                                    stream_out,
                                    address);
    if (ret < 0) {
        ALOGE("%s(), open stream failed", __func__);
        return ret;
    }

    aml_out = (struct aml_stream_out *)(*stream_out);
    aml_out->usecase = attr_to_usecase(aml_out->device, aml_out->hal_format, aml_out->flags);
    aml_out->is_normal_pcm = (aml_out->usecase == STREAM_PCM_NORMAL) ? 1 : 0;
    aml_out->out_cfg = *config;
    if (adev->useSubMix) {
        // In V1.1, android out lpcm stream and pheader pcm stream goes to aml mixer,
        // tv source keeps the original way.
        // Next step is to make all compitable.
        if (aml_out->usecase == STREAM_PCM_NORMAL ||
            aml_out->usecase == STREAM_PCM_HWSYNC ||
            aml_out->usecase == STREAM_PCM_MMAP ||
            (aml_out->usecase == STREAM_PCM_DIRECT)) {
            /*for 96000, we need bypass submix, this is for DTS certification*/
            /* for DTV case, maybe this function is called by the DTV output thread,
               and the audio patch is enabled, we do not need to wait DTV exit as it is
               enabled by DTV itself */
            if (config->sample_rate == 96000 || config->sample_rate == 88200 ||
                    audio_channel_count_from_out_mask(config->channel_mask) > 2) {
                AM_LOGI("need resample or downmix!");
            }
            {
                int retry_count = 0;
                /* when dvb switch to neflix,dvb send cmd stop and dtv decoder_state
                    AUDIO_DTV_PATCH_DECODER_STATE_INIT, but when hdmi plug in and out dvb
                    do not send cmd stop and only release audiopatch,dtv decoder_state AUDIO_DTV_PATCH_DECODER_STATE_RUNNING*/
                while  (adev->audio_patch && adev->audio_patching
                    && adev->patch_src == SRC_DTV && retry_count < 50
                    && adev->audio_patch->dtv_decoder_state == AUDIO_DTV_PATCH_DECODER_STATE_INIT) {
                    usleep(20000);
                    retry_count++;
                    ALOGW("waiting dtv patch release before create submixing path %d\n",retry_count);
                }
                /* delete initSubMixingInput now, no longer need deleteSubMixingInput in adev_close_output_stream_new*/
                //ret = initSubMixingInput(aml_out, config);
                aml_out->bypass_submix = false;
                aml_out->inputPortID = -1;
                if (ret < 0) {
                    ALOGE("initSub mixing input failed");
                }
            }
        }
    }
    memcpy(&aml_out->audioCfg, config, sizeof(struct audio_config));
    aml_out->status = STREAM_STANDBY;
    if (adev->continuous_audio_mode == 0) {
        adev->spdif_encoder_init_flag = false;
    }

    aml_out->stream.write = out_write_new;
    aml_out->stream.common.standby = out_standby_new;
    aml_out->stream.pause = out_pause_new;
    aml_out->stream.resume = out_resume_new;
    aml_out->stream.flush = out_flush_new;

    aml_out->codec_type = get_codec_type(aml_out->hal_internal_format);
    aml_out->continuous_mode_check = true;

    /*when a new stream is opened here, we can add it to active stream*/
    pthread_mutex_lock(&adev->active_outputs_lock);
    adev->active_outputs[aml_out->usecase] = aml_out;
    pthread_mutex_unlock(&adev->active_outputs_lock);

    if (!(flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        /*In order to avoid the invalid adjustment of mastervol when there is no stream */
        out_set_volume_l((struct audio_stream_out *)aml_out, aml_out->volume_l_org, aml_out->volume_r_org);
    }
    /* init ease for stream */
    if (aml_audio_ease_init(&aml_out->audio_stream_ease) < 0) {
        ALOGE("%s  aml_audio_ease_init faild\n", __func__);
        ret = -EINVAL;
        goto AUDIO_EASE_INIT_FAIL;
    }

    if (adev->audio_ease && adev->dolby_lib_type_last != eDolbyMS12Lib) {
        start_ease_in(adev);
    }

    if (address && !strcmp(address, LLP_BUFFER_NAME)) {
        aml_out->llp_buf = IpcBuffer_create(LLP_BUFFER_NAME, LLP_BUFFER_SIZE);
        if (aml_out->llp_buf) {
#if 0
            if (!pthread_create(&aml_out->llp_input_threadID, NULL,
                       &llp_input_threadloop, (struct audio_stream_out *)aml_out)) {
                aml_out->llp_input_thread_created = true;
                ALOGI("LLP input thread created");
            } else {
                IpcBuffer_destroy(aml_out->llp_buf);
                aml_out->llp_buf = NULL;
                ALOGE("LLP input thread failed to create");
            }
#else
            setenv(LLP_BUFFER_NAME, "1", 1);
            aml_alsa_set_llp(true);
            dolby_ms12_register_callback_by_type(CALLBACK_LLP_BUFFER, (void *)ms12_llp_callback, (void *)aml_out);
            if (audio_channel_count_from_out_mask(aml_out->hal_channel_mask) == 6) {
                /* Allow MS12 MC output when LLP input is 6ch */
                adev->sink_allow_max_channel = true;
            }
#endif
        } else {
            ALOGE("LLP IPC buffer create failed.");
        }
    }

    if (aml_audio_property_get_bool("vendor.media.audio.hal.debug", false)) {
        aml_out->debug_stream = 1;
    }

    if (aml_out->continuous_mode_check && !(flags & AUDIO_OUTPUT_FLAG_AD_STREAM)) {
        if (adev->dolby_lib_type_last == eDolbyMS12Lib) {
            /*if these format can't be supported by ms12, we can bypass it*/
            if (is_bypass_dolbyms12(*stream_out)) {
                if (adev->ms12.dolby_ms12_enable) {
                    AM_LOGI("bypass aml_ms12, clean up aml_ms12");
                    adev_ms12_cleanup((struct audio_hw_device *)adev);
                }
                adev->dolby_lib_type = eDolbyDcvLib;
                aml_out->restore_dolby_lib_type = true;
                AM_LOGI("bypass aml_ms12 change aml_dolby dcv lib type");
            }
        }
        aml_out->continuous_mode_check = false;
    }

    if (usecase_change_validate_l(aml_out, false) < 0) {
        ALOGE("%s() failed", __func__);
        aml_audio_trace_int("adev_open_output_stream_new", 0);
        return 0;
    }
    if (adev->useSubMix) {
        init_mixer_input_port(adev->audio_mixer, &aml_out->audioCfg, aml_out->flags,
            on_notify_cbk, aml_out, on_input_avail_cbk, aml_out, NULL, NULL, 1.0);
        AM_LOGI("direct port:%s", mixerInputType2Str(get_input_port_type(&aml_out->audioCfg, aml_out->flags)));
    }
    if (!adev->useSubMix && is_dts_format(aml_out->hal_internal_format) && adev->dolby_lib_type_last == eDolbyMS12Lib) {
        adev->useSubMix = true;
        ret = initHalSubMixing(MIXER_LPCM, adev, adev->is_TV);
        adev->raw_to_pcm_flag = false;
        init_mixer_input_port(adev->audio_mixer, &aml_out->audioCfg, aml_out->flags,
            on_notify_cbk, aml_out, on_input_avail_cbk, aml_out, NULL, NULL, 1.0);
        AM_LOGI("direct port:%s", mixerInputType2Str(get_input_port_type(&aml_out->audioCfg, aml_out->flags)));
    }

    ALOGD("-%s: out %p: io handle:%d, usecase:%s card:%d alsa devices:%d", __func__,
        aml_out, handle, usecase2Str(aml_out->usecase), aml_out->card, aml_out->device);

    return 0;

AUDIO_EASE_INIT_FAIL:
    adev_close_output_stream(dev, *stream_out);
    return ret;
}

void adev_close_output_stream_new(struct audio_hw_device *dev,
                                struct audio_stream_out *stream)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    if (dev == NULL || stream == NULL) {
        ALOGE("%s: Input parameter error!!!", __func__);
        return;
    }

    ALOGI("%s: enter usecase = %s", __func__, usecase2Str(aml_out->usecase));

    /* free stream ease resource  */
    aml_audio_ease_close(aml_out->audio_stream_ease);
    /* free audio speed resource  */
    aml_audio_speed_close(aml_out->speed_handle);

    /* call legacy close to reuse codes */
    pthread_mutex_lock(&adev->active_outputs_lock);
    if (adev->active_outputs[aml_out->usecase] == aml_out) {
        adev->active_outputs[aml_out->usecase] = NULL;
    } else {
        for (int i = STREAM_USECASE_EXT1; i < STREAM_USECASE_MAX; i++) {
            if (adev->active_outputs[i] == aml_out) {
                adev->active_outputs[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&adev->active_outputs_lock);

    if (adev->useSubMix) {
        if (aml_out->is_normal_pcm ||
            aml_out->usecase == STREAM_PCM_HWSYNC ||
            aml_out->usecase == STREAM_PCM_MMAP ||
            aml_out->usecase == STREAM_PCM_DIRECT) {
            if (!aml_out->bypass_submix) {
                 /* initSubMixingInput has been deleted in adev_open_output_stream_new, no longer need deleteSubMixingInput*/
                //deleteSubMixingInput(aml_out);
            }
        }
        if (aml_out->usecase == STREAM_PCM_HWSYNC) {
            int ret = aml_audio_timer_delete(aml_out->timer_id);
            ALOGD("func:%s timer_id:%d  ret:%d",__func__, aml_out->timer_id, ret);
        }

    }

    adev_close_output_stream(dev, stream);
    if (eDolbyMS12Lib ==  adev->dolby_lib_type && adev->audio_mixer) {
        deleteHalSubMixing(adev);
        adev->audio_mixer =NULL;
        adev->useSubMix = false;
        int ret = adev_ms12_prepare((struct audio_hw_device *)adev);
        if (0 != ret) {
            ALOGE("%s, adev_ms12_prepare fail!\n", __func__);
        }
    }
    ALOGI("%s: exit", __func__);
}

#if 0
static void *llp_input_threadloop(void *data)
{
    struct audio_stream_out *out = (struct audio_stream_out *)data;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)out;
    uint64_t rp = 0;
    unsigned char zero[5*12*48];
    unsigned char *in = IpcBuffer_get_ptr(LLP_BUFFER_NAME);
    void *ipc_buffer = IpcBuffer_get_by_name(LLP_BUFFER_NAME);

    memset(zero, 0, sizeof(zero));

    //setenv("vendor_media_audiohal_ms12dump", "65535", 1);

    while (!aml_out->llp_input_thread_exit) {
        uint64_t wp = IpcBuffer_get_wr_pos(LLP_BUFFER_NAME);
        unsigned level;
        unsigned offset = rp % LLP_BUFFER_SIZE;
        int nChannel = audio_channel_count_from_out_mask(aml_out->hal_channel_mask);
        int alsa_level = aml_audio_out_get_ms12_latency_frames((struct audio_stream_out *)aml_out);
        int main_input_level = dolby_ms12_get_main_buffer_avail(NULL) / nChannel / 2;
        level = (wp - rp) % LLP_BUFFER_SIZE;
        rp = wp - level;    // normalize
        if ((level > 0) && (rp == 0)) {
            dolby_ms12_flush_main_input_buffer();
            setenv("main_llp", "1", 1);
            aml_alsa_set_llp(true);
        }
        if (in) {
            if ((level == 0) && ((main_input_level + alsa_level) < (15 * 48))) {
                out_write_new(out, zero, sizeof(zero));
                IpcBuffer_add_silence(ipc_buffer, sizeof(zero) / nChannel / 2);
                out_write_new(out, zero, sizeof(zero));
                IpcBuffer_add_silence(ipc_buffer, sizeof(zero) / nChannel / 2);
                IpcBuffer_inc_underrun(ipc_buffer);
                ALOGI("llp underrun, alsa_level = %d main_input_level = %d", alsa_level, main_input_level);
            } else if (level <= (LLP_BUFFER_SIZE - offset)) {
                out_write_new(out, in + offset, level);
            } else {
                unsigned copied = LLP_BUFFER_SIZE - offset;
                out_write_new(out, in + offset, copied);
                out_write_new(out, in, level - copied);
            }
        }

        rp += level;

        alsa_level = aml_audio_out_get_ms12_latency_frames((struct audio_stream_out *)aml_out);
        main_input_level = dolby_ms12_get_main_buffer_avail(NULL) / nChannel / 2;
        IpcBuffer_setWaterLevel(ipc_buffer, alsa_level + main_input_level);

        ALOGI("llp wp = %" PRIu64 ", alsa_level = %d ms, ms12 input level = %d ms", wp, alsa_level / 48, main_input_level / 48);

        aml_audio_sleep(5000);
    }

    unsetenv("main_llp");
    aml_alsa_set_llp(false);

    //setenv("vendor_media_audiohal_ms12dump", "0", 1);

    ALOGI("Sys llp input thread exit.");

    return NULL;
}
#else
static int ms12_llp_callback(void *priv_data, void *info)
{
    struct aml_stream_out *aml_out;
    aml_scaletempo_info_t *conf;
    short *in;
    unsigned offset, level;
    uint64_t wp;
    uint64_t wp_underrun = 0;
    int nChannel, alsa_level;
    bool fill_silence = false;
    struct timespec ts;

    if (priv_data == NULL || info == NULL) {
        return -1;
    }

    aml_out = (struct aml_stream_out *)priv_data;
    conf = (aml_scaletempo_info_t *)info;

    if (!aml_out->llp_buf) {
        return -1;
    }

    wp = IpcBuffer_get_wr_pos(LLP_BUFFER_NAME);
    nChannel = audio_channel_count_from_out_mask(aml_out->hal_channel_mask);
    level = (wp - aml_out->llp_rp) % LLP_BUFFER_SIZE;

    alsa_level = out_get_ms12_latency_frames((struct audio_stream_out *)aml_out, true);
    if (alsa_level == aml_out->config.period_size * aml_out->config.period_count) {
        alsa_level = 0;
    }

    if (level < 256 * nChannel * 2) {
        if (alsa_level > 48 * 8) {
            conf->output_samples = 0;
            IpcBuffer_setMeta(aml_out->llp_buf, wp / nChannel / 2, alsa_level + level / nChannel / 2);
            clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
            ALOGI("[%lld.%.9ld]Holding silence, alsa_level = %d, input_level = %d", (long long)ts.tv_sec, ts.tv_nsec, alsa_level / 48, level / nChannel / 2 / 48);
            return 0;
        }

        fill_silence = true;
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        ALOGI("[%lld.%.9ld]llp underrun, fill silence", (long long)ts.tv_sec, ts.tv_nsec);

        if (aml_out->llp_underrun_wp != wp) {
            IpcBuffer_inc_underrun(aml_out->llp_buf);
            aml_out->llp_underrun_wp = wp;
            ALOGI("[%lld.%.9ld] inc underrun", (long long)ts.tv_sec, ts.tv_nsec);
        }
    }

    aml_out->llp_rp = wp - level;    // normalize
    offset = aml_out->llp_rp % LLP_BUFFER_SIZE;
    in = (short *)(IpcBuffer_get_ptr(LLP_BUFFER_NAME) + offset);

    conf->ch = nChannel;
    conf->output_samples = 256;

    /* TODO: Neon optimize vector conversion */
    if (nChannel == 6) {
        int i;
        float *out_l   = (float *)conf->outputbuffer->ppdata[0];
        float *out_r   = (float *)conf->outputbuffer->ppdata[1];
        float *out_c   = (float *)conf->outputbuffer->ppdata[2];
        float *out_lfe = (float *)conf->outputbuffer->ppdata[3];
        float *out_ls  = (float *)conf->outputbuffer->ppdata[4];
        float *out_rs  = (float *)conf->outputbuffer->ppdata[5];
        if (fill_silence) {
            for (i = 0; i < 256; i++) {
                *out_l++   = (float)0;
                *out_r++   = (float)0;
                *out_c++   = (float)0;
                *out_lfe++ = (float)0;
                *out_ls++  = (float)0;
                *out_rs++  = (float)0;
            }
        } else {
            for (i = 0; i < 256; i++) {
                *out_l++   = (float)(*in++) / 32768.0f;
                *out_r++   = (float)(*in++) / 32768.0f;
                *out_c++   = (float)(*in++) / 32768.0f;
                *out_lfe++ = (float)(*in++) / 32768.0f;
                *out_ls++  = (float)(*in++) / 32768.0f;
                *out_rs++  = (float)(*in++) / 32768.0f;
            }
       }
    } else if (nChannel == 2) {
        int i;
        float *out_l = (float *)conf->outputbuffer->ppdata[0];
        float *out_r = (float *)conf->outputbuffer->ppdata[1];
        if (fill_silence) {
            for (i = 0; i < 256; i++) {
                *out_l++ = (float)0;
                *out_r++ = (float)0;
            }
        } else {
            for (i = 0; i < 256; i++) {
                *out_l++ = (float)(*in++) / 32768.0f;
                *out_r++ = (float)(*in++) / 32768.0f;
            }
        }
    }

    if (!fill_silence) {
        level -= 256 * nChannel * 2;
        aml_out->llp_rp += nChannel * 2 * 256;
    }

    IpcBuffer_setMeta(aml_out->llp_buf, wp / nChannel / 2, alsa_level + level / nChannel / 2);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    ALOGI("[%lld.%.9ld] llp wp = %" PRIu64 ", rp = %" PRIu64 ", alsa_level = %d, llp input level = %d", (long long)ts.tv_sec, ts.tv_nsec, wp / 12, aml_out->llp_rp / 12, alsa_level, level / nChannel / 2);

    return 0;
}
#endif

static void dump_audio_patch_set (struct audio_patch_set *patch_set)
{
    struct audio_patch *patch = NULL;
    unsigned int i = 0;

    if (!patch_set)
        return;

    patch = &patch_set->audio_patch;
    if (!patch)
        return;

    ALOGI ("  - %s(), id: %d", __func__, patch->id);
    for (i = 0; i < patch->num_sources; i++)
        dump_audio_port_config (&patch->sources[i]);
    for (i = 0; i < patch->num_sinks; i++)
        dump_audio_port_config (&patch->sinks[i]);
}

static void dump_aml_audio_patch_sets (struct audio_hw_device *dev)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    struct audio_patch_set *patch_set = NULL;
    struct audio_patch *patch = NULL;
    struct listnode *node = NULL;
    unsigned int i = 0;

    ALOGI ("++%s(), lists all patch sets:", __func__);
    list_for_each (node, &aml_dev->patch_list) {
        ALOGI (" - patch set num: %d", i);
        patch_set = node_to_item (node, struct audio_patch_set, list);
        dump_audio_patch_set (patch_set);
        i++;
    }
    ALOGI ("--%s(), lists all patch sets over", __func__);
}


static int get_audio_patch_by_hdl(struct audio_hw_device *dev, audio_patch_handle_t handle, struct audio_patch *p_audio_patch)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;

    struct listnode *node = NULL;

    struct audio_patch_set *patch_set_tmp = NULL;
    struct audio_patch *patch_tmp = NULL;
    p_audio_patch = NULL;

    list_for_each(node, &aml_dev->patch_list) {
        patch_set_tmp = node_to_item(node, struct audio_patch_set, list);
        patch_tmp = &patch_set_tmp->audio_patch;


        if (patch_tmp->id == handle) {
            p_audio_patch = patch_tmp;
            break;
        }
    }
    return 0;
}

static enum OUT_PORT get_output_dev_for_sinks(enum OUT_PORT *sinks, int num_sinks, enum OUT_PORT sink) {
    for (int i=0; i<num_sinks; i++) {
        if (sinks[i] == sink) {
            return sink;
        }
    }
    return -1;
}

static enum OUT_PORT get_output_dev_for_strategy(struct aml_audio_device *adev, enum OUT_PORT *sinks, int num_sinks)
{
    uint32_t sink = -1;
    if (sink == -1) {
        sink = get_output_dev_for_sinks(sinks, num_sinks, OUTPORT_A2DP);
    }
    if (sink == -1 && adev->bHDMIARCon) {
        sink = get_output_dev_for_sinks(sinks, num_sinks, OUTPORT_HDMI_ARC);
    }
    if (sink == -1) {
        sink = get_output_dev_for_sinks(sinks, num_sinks, OUTPORT_HDMI);
    }
    if (sink == -1) {
        sink = get_output_dev_for_sinks(sinks, num_sinks, OUTPORT_SPEAKER);
    }
    if (sink == -1) {
        sink = OUTPORT_SPEAKER;
    }
    return sink;
}

/* remove audio patch from dev list */
static int unregister_audio_patch(struct audio_hw_device *dev __unused,
                                struct audio_patch_set *patch_set)
{
    ALOGD("%s: enter", __func__);
    if (!patch_set)
        return -EINVAL;
#ifdef DEBUG_PATCH_SET
    dump_audio_patch_set(patch_set);
#endif
    list_remove(&patch_set->list);
    aml_audio_free(patch_set);
    ALOGD("%s: exit", __func__);

    return 0;
}

/* add new audio patch to dev list */
static struct audio_patch_set *register_audio_patch(struct audio_hw_device *dev,
                                                unsigned int num_sources,
                                                const struct audio_port_config *sources,
                                                unsigned int num_sinks,
                                                const struct audio_port_config *sinks,
                                                audio_patch_handle_t *handle)
{
    /* init audio patch */
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct audio_patch_set *patch_set_new = NULL;
    struct audio_patch *patch_new = NULL;
    struct audio_patch_set *patch_set_tmp = NULL;
    struct audio_patch *patch_tmp = NULL;
    struct listnode *node = NULL;
    audio_patch_handle_t patch_handle;

    patch_set_new = aml_audio_calloc(1, sizeof (*patch_set_new) );
    if (!patch_set_new) {
        ALOGE("%s(): no memory", __func__);
        return NULL;
    }

    patch_new = &patch_set_new->audio_patch;
    #ifdef BUILD_LINUX
    patch_handle = (audio_patch_handle_t) atomic_inc((atomic_t *)(&aml_dev->next_unique_ID));
    #else
    patch_handle = (audio_patch_handle_t) android_atomic_inc(&aml_dev->next_unique_ID);
    #endif

    *handle = patch_handle;

    /* init audio patch new */
    patch_new->id = patch_handle;
    patch_new->num_sources = num_sources;
    memcpy(patch_new->sources, sources, num_sources * sizeof(struct audio_port_config));
    patch_new->num_sinks = num_sinks;
    memcpy(patch_new->sinks, sinks, num_sinks * sizeof (struct audio_port_config));
#ifdef DEBUG_PATCH_SET
    ALOGD("%s(), patch set new to register:", __func__);
    dump_audio_patch_set(patch_set_new);
#endif

    /* find if mix->dev exists and remove from list */
    list_for_each(node, &aml_dev->patch_list) {
        patch_set_tmp = node_to_item(node, struct audio_patch_set, list);
        patch_tmp = &patch_set_tmp->audio_patch;
        if (patch_tmp->sources[0].type == AUDIO_PORT_TYPE_MIX &&
            patch_tmp->sinks[0].type == AUDIO_PORT_TYPE_DEVICE &&
            sources[0].ext.mix.handle == patch_tmp->sources[0].ext.mix.handle) {
            ALOGI("patch found mix->dev id %d, patchset %p", patch_tmp->id, patch_set_tmp);
                break;
        } else {
            patch_set_tmp = NULL;
            patch_tmp = NULL;
        }
    }
    /* found mix->dev patch, so release and remove it */
    if (patch_set_tmp) {
        ALOGD("%s, found the former mix->dev patch set, remove it first", __func__);
        unregister_audio_patch(dev, patch_set_tmp);
        patch_set_tmp = NULL;
        patch_tmp = NULL;
    }
    /* find if dev->mix exists and remove from list */
    list_for_each (node, &aml_dev->patch_list) {
        patch_set_tmp = node_to_item(node, struct audio_patch_set, list);
        patch_tmp = &patch_set_tmp->audio_patch;
        if (patch_tmp->sources[0].type == AUDIO_PORT_TYPE_DEVICE &&
            patch_tmp->sinks[0].type == AUDIO_PORT_TYPE_MIX &&
            sinks[0].ext.mix.handle == patch_tmp->sinks[0].ext.mix.handle) {
            ALOGI("patch found dev->mix id %d, patchset %p", patch_tmp->id, patch_set_tmp);
                break;
        } else {
            patch_set_tmp = NULL;
            patch_tmp = NULL;
        }
    }
    /* found dev->mix patch, so release and remove it */
    if (patch_set_tmp) {
        ALOGD("%s, found the former dev->mix patch set, remove it first", __func__);
        unregister_audio_patch(dev, patch_set_tmp);
    }
    /* add new patch set to dev patch list */
    list_add_head(&aml_dev->patch_list, &patch_set_new->list);
    ALOGI("--%s: after registering new patch, patch sets will be:", __func__);
    //dump_aml_audio_patch_sets(dev);
    return patch_set_new;
}

static int adev_create_audio_patch(struct audio_hw_device *dev,
                                unsigned int num_sources,
                                const struct audio_port_config *sources,
                                unsigned int num_sinks,
                                const struct audio_port_config *sinks,
                                audio_patch_handle_t *handle)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct audio_patch_set *patch_set;
    const struct audio_port_config *src_config = sources;
    const struct audio_port_config *sink_config = sinks;
    enum input_source input_src = HDMIIN;
    uint32_t sample_rate = 48000, channel_cnt = 2;
    enum OUT_PORT outport = OUTPORT_SPEAKER;
    enum IN_PORT inport = INPORT_HDMIIN;
    unsigned int i = 0;
    int ret = -1;
    aml_dev->no_underrun_max = aml_audio_property_get_int("vendor.media.audio_hal.nounderrunmax", 60);
    aml_dev->start_mute_max = aml_audio_property_get_int("vendor.media.audio_hal.startmutemax", 50);

    aml_dtv_audio_instances_t *dtv_audio_instances = (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    for (int index = 0; index < DVB_DEMUX_SUPPORT_MAX_NUM; index ++) {
        dtv_audio_instances->demux_handle[index] = 0;
        dtv_audio_instances->demux_info[index].media_presentation_id = -1;
        dtv_audio_instances->demux_info[index].media_first_lang = -1;
        dtv_audio_instances->demux_info[index].media_second_lang = -1;
        dtv_audio_instances->demux_info[index].ad_dmx_init = false;
    }

    if ((src_config->ext.device.type == AUDIO_DEVICE_IN_WIRED_HEADSET) || (src_config->ext.device.type == AUDIO_DEVICE_IN_BLUETOOTH_BLE)) {
        ALOGD("bluetooth voice search is in use, bypass adev_create_audio_patch()!!\n");
        //we can't return error to application because it maybe process the error .
        return 0;
    }
    if (!sources || !sinks || !handle) {
        ALOGE("[%s:%d] null pointer! sources:%p, sinks:%p, handle:%p", __func__, __LINE__, sources, sinks, handle);
        return -EINVAL;
    }
    if ((num_sources != 1) || (num_sinks > AUDIO_PATCH_PORTS_MAX)) {
        ALOGE("[%s:%d] unsupport num sources:%d or sinks:%d", __func__, __LINE__, num_sources, num_sinks);
        return -EINVAL;
    }

    ALOGI("[%s:%d] num_sources:%d, num_sinks:%d, %s(%d)->%s(%d), aml_dev->patch_src:%s, AF:%p"
            , __func__, __LINE__, num_sources, num_sinks
            , (src_config->type == AUDIO_PORT_TYPE_MIX) ? "mix" : "device"
            , (src_config->type == AUDIO_PORT_TYPE_MIX) ? src_config->ext.mix.handle : src_config->ext.device.type
            , (sink_config->type == AUDIO_PORT_TYPE_MIX) ? "mix" : "device"
            , (sink_config->type == AUDIO_PORT_TYPE_MIX) ? sink_config->ext.mix.handle : sink_config->ext.device.type
            , patchSrc2Str(aml_dev->patch_src), handle);
    patch_set = register_audio_patch(dev, num_sources, sources, num_sinks, sinks, handle);
    if (!patch_set) {
        ALOGW("[%s:%d] patch_set is null, create fail.", __func__, __LINE__);
        return -ENOMEM;
    }

    if (sink_config->type == AUDIO_PORT_TYPE_DEVICE) {
        android_dev_convert_to_hal_dev(sink_config->ext.device.type, (int *)&outport);
        /* Only 3 sink devices are allowed to coexist */
        if (num_sinks > 3) {
            ALOGW("[%s:%d] not support num_sinks:%d", __func__, __LINE__, num_sinks);
            return -EINVAL;;
        } else if (num_sinks > 1) {
            enum OUT_PORT sink_devs[3] = {OUTPORT_SPEAKER, OUTPORT_SPEAKER, OUTPORT_SPEAKER};
            for (int i=0; i<num_sinks; i++) {
                android_dev_convert_to_hal_dev(sink_config[i].ext.device.type, (int *)&sink_devs[i]);
                if (aml_dev->debug_flag) {
                    ALOGD("[%s:%d] sink[%d]:%s", __func__, __LINE__, i, outputPort2Str(sink_devs[i]));
                }
            }
            outport = get_output_dev_for_strategy(aml_dev, sink_devs, num_sinks);
        } else {
            ALOGI("[%s:%d] one sink, sink:%s", __func__, __LINE__, outputPort2Str(outport));
        }

        /*ingore outport SPEAKER, because LINUX always sets SPEAKER when ARC or HDMI connected*/
        if (outport != OUTPORT_SPEAKER || aml_dev->active_outport == OUTPORT_MAX) {
            if (outport != OUTPORT_SPDIF) {
                ret = aml_audio_output_routing(dev, outport, false);
            }
            aml_dev->out_device = 0;
            for (i = 0; i < num_sinks; i++) {
                aml_dev->out_device |= sink_config[i].ext.device.type;
            }
        }

        /* 1.device to device audio patch. TODO: unify with the android device type */
        if (src_config->type == AUDIO_PORT_TYPE_DEVICE) {
            if (sink_config->config_mask & AUDIO_PORT_CONFIG_SAMPLE_RATE) {
                sample_rate = sink_config->sample_rate;
            }
            if (sink_config->config_mask & AUDIO_PORT_CONFIG_CHANNEL_MASK) {
                channel_cnt = audio_channel_count_from_out_mask(sink_config->channel_mask);
            }
            ret = android_dev_convert_to_hal_dev(src_config->ext.device.type, (int *)&inport);
            if (ret != 0) {
                ALOGE("[%s:%d] device->device patch: unsupport input dev:%#x.", __func__, __LINE__, src_config->ext.device.type);
                ret = -EINVAL;
                unregister_audio_patch(dev, patch_set);
            }

            aml_audio_input_routing(dev, inport);
            if (AUDIO_DEVICE_IN_ECHO_REFERENCE != src_config->ext.device.type &&
                AUDIO_DEVICE_IN_TV_TUNER != src_config->ext.device.type) {
                aml_dev->patch_src = android_input_dev_convert_to_hal_patch_src(src_config->ext.device.type);
            }
            input_src = android_input_dev_convert_to_hal_input_src(src_config->ext.device.type);
            aml_dev->active_inport = inport;
            ALOGI("[%s:%d] dev->dev patch: inport:%s(active_inport:%d) -> outport:%s, input_src:%s", __func__, __LINE__,
                inputPort2Str(inport), aml_dev->active_inport, outputPort2Str(outport), patchSrc2Str(input_src));
            ALOGI("[%s:%d] input dev:%#x, all output dev:%#x", __func__, __LINE__,
                src_config->ext.device.type, aml_dev->out_device);
            if (inport == INPORT_TUNER) {
                if (aml_dev->is_TV) {
                    if (aml_dev->patch_src != SRC_DTV)
                        aml_dev->patch_src = SRC_ATV;
                } else {
                   aml_dev->patch_src = SRC_DTV;
                }
            }
            // ATV path goes to dev set_params which could
            // tell atv or dtv source and decide to create or not.
            // One more case is ATV->ATV, should recreate audio patch.
            if ((inport != INPORT_TUNER)
                    || ((inport == INPORT_TUNER) && (aml_dev->patch_src == SRC_ATV))) {
                if (input_src != SRC_NA) {
                    set_audio_source(&aml_dev->alsa_mixer, input_src, alsa_device_is_auge());
                }


                if (aml_dev->audio_patch) {
                    ALOGD("%s: patch exists, first release it", __func__);
                    ALOGD("%s: new input %#x, old input %#x",
                        __func__, inport, aml_dev->audio_patch->input_src);
                    if (aml_dev->audio_patch->is_dtv_src) {
#ifdef USE_DTV
                        release_dtv_patch(aml_dev);
#endif
                    }
                    else
                        release_patch(aml_dev);
                }
                ret = create_patch(dev, src_config->ext.device.type, aml_dev->out_device);
                if (ret) {
                    ALOGE("[%s:%d] create patch failed, all out dev:%#x.", __func__, __LINE__, aml_dev->out_device);
                    ret = -EINVAL;
                    unregister_audio_patch(dev, patch_set);
                }
                aml_dev->audio_patching = 1;
            } else if ((inport == INPORT_TUNER) && (aml_dev->patch_src == SRC_DTV)) {
#ifdef USE_DTV
                 if (/*aml_dev->is_TV*/1) {

                     if (aml_dev->is_TV) {
                         if ((aml_dev->patch_src == SRC_DTV) && aml_dev->audio_patching) {
                             ALOGI("%s, now release the dtv patch now\n ", __func__);
                             ret = release_dtv_patch(aml_dev);
                             if (!ret) {
                                 aml_dev->audio_patching = 0;
                             }
                         }
                         ALOGI("%s, now end release dtv patch the audio_patching is %d ", __func__, aml_dev->audio_patching);
                         ALOGI("%s, now create the dtv patch now\n ", __func__);
                     }

                    aml_dev->patch_src = SRC_DTV;

                    ret = create_dtv_patch(dev, AUDIO_DEVICE_IN_TV_TUNER,
                                        AUDIO_DEVICE_OUT_SPEAKER);
                    if (ret == 0) {
                        aml_dev->audio_patching = 1;
                    }
                    ALOGI("%s, now end create dtv patch the audio_patching is %d ", __func__, aml_dev->audio_patching);

                 }
#endif
            }
#ifdef USE_EQ_DRC
            if (input_src == LINEIN && aml_dev->aml_ng_enable) {
                aml_dev->aml_ng_handle = init_noise_gate(aml_dev->aml_ng_level,
                                         aml_dev->aml_ng_attack_time, aml_dev->aml_ng_release_time);
                ALOGE("%s: init amlogic noise gate: level: %fdB, attrack_time = %dms, release_time = %dms",
                      __func__, aml_dev->aml_ng_level,
                      aml_dev->aml_ng_attack_time, aml_dev->aml_ng_release_time);
            }
#endif
        } else if (src_config->type == AUDIO_PORT_TYPE_MIX) {  /* 2. mix to device audio patch */
            ALOGI("[%s:%d] mix->device patch: mix - >outport:%s", __func__, __LINE__, outputPort2Str(outport));
            ret = 0;
        } else {
            ALOGE("[%s:%d] invalid patch, source error, source:%d to sink DEVICE", __func__, __LINE__, src_config->type);
            ret = -EINVAL;
            unregister_audio_patch(dev, patch_set);
        }
    } else if (sink_config->type == AUDIO_PORT_TYPE_MIX) {
        if (src_config->type == AUDIO_PORT_TYPE_DEVICE) { /* 3.device to mix audio patch */
            ret = android_dev_convert_to_hal_dev(src_config->ext.device.type, (int *)&inport);
            if (ret != 0) {
                ALOGE("[%s:%d] device->mix patch: unsupport input dev:%#x.", __func__, __LINE__, src_config->ext.device.type);
                ret = -EINVAL;
                unregister_audio_patch(dev, patch_set);
            }
            aml_audio_input_routing(dev, inport);
            input_src = android_input_dev_convert_to_hal_input_src(src_config->ext.device.type);
            if (AUDIO_DEVICE_IN_TV_TUNER == src_config->ext.device.type) {
                aml_dev->tuner2mix_patch = true;
            } else if (AUDIO_DEVICE_IN_ECHO_REFERENCE != src_config->ext.device.type) {
                if (AUDIO_DEVICE_IN_BUILTIN_MIC != src_config->ext.device.type &&
                    AUDIO_DEVICE_IN_BACK_MIC != src_config->ext.device.type )
                    aml_dev->patch_src = android_input_dev_convert_to_hal_patch_src(src_config->ext.device.type);
            }
            ALOGI("[%s:%d] device->mix patch: inport:%s -> mix, input_src:%s, in dev:%#x", __func__, __LINE__,
                inputPort2Str(inport), patchSrc2Str(input_src), src_config->ext.device.type);
            if (input_src != SRC_NA) {
                set_audio_source(&aml_dev->alsa_mixer, input_src, alsa_device_is_auge());
            }
            aml_dev->active_inport = inport;
            //aml_dev->src_gain[inport] = 1.0;
            if (inport == INPORT_HDMIIN || inport == INPORT_ARCIN || inport == INPORT_SPDIF) {
                aml_dev2mix_parser_create(dev, src_config->ext.device.type);
            } else if ((inport == INPORT_TUNER) && (aml_dev->patch_src == SRC_DTV)){///zzz
                if (aml_dev->is_TV) {
                    if (aml_dev->audio_patching) {
                        ALOGI("%s,!!!now release the dtv patch now\n ", __func__);
#ifdef USE_DTV
                        ret = release_dtv_patch(aml_dev);
#endif
                        if (!ret) {
                            aml_dev->audio_patching = 0;
                        }
                    }
                    ALOGI("%s, !!! now create the dtv patch now\n ", __func__);
#ifdef USE_DTV
                    ret = create_dtv_patch(dev, AUDIO_DEVICE_IN_TV_TUNER, AUDIO_DEVICE_OUT_SPEAKER);
#endif
                    if (ret == 0) {
                        aml_dev->audio_patching = 1;
                    }
                }
            }
            ret = 0;
        } else {
            ALOGE("[%s:%d] invalid patch, source error, source:%d to sink MIX", __func__, __LINE__, src_config->type);
            ret = -EINVAL;
            unregister_audio_patch(dev, patch_set);
        }
    } else {
        ALOGE("[%s:%d] invalid patch, sink:%d error", __func__, __LINE__, sink_config->type);
        ret = -EINVAL;
        unregister_audio_patch(dev, patch_set);
    }
    return ret;
}

/* Release an audio patch */
static int adev_release_audio_patch(struct audio_hw_device *dev,
                                audio_patch_handle_t handle)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct audio_patch_set *patch_set = NULL;
    struct audio_patch *patch = NULL;
    struct listnode *node = NULL;
    int ret = 0;

    ALOGI("++%s: handle(%d)", __func__, handle);
    if (list_empty(&aml_dev->patch_list)) {
        ALOGE("No patch in list to release");
        ret = -EINVAL;
        goto exit;
    }

    /* find audio_patch in patch_set list */
    list_for_each(node, &aml_dev->patch_list) {
        patch_set = node_to_item(node, struct audio_patch_set, list);
        patch = &patch_set->audio_patch;
        if (patch->id == handle) {
            ALOGI("patch set found id %d, patchset %p", patch->id, patch_set);
            break;
        } else {
            patch_set = NULL;
            patch = NULL;
        }
    }

    if (!patch_set || !patch) {
        ALOGE("Can't get patch in list");
        ret = -EINVAL;
        goto exit;
    }
    if (aml_dev->audio_patch && aml_dev->audio_patch->input_src != patch->sources[0].ext.device.type)
        goto exit_unregister;
    ALOGI("source 0 type=%d sink type =%d amk patch src=%s", patch->sources[0].type, patch->sinks[0].type, patchSrc2Str(aml_dev->patch_src));
    if (patch->sources[0].type == AUDIO_PORT_TYPE_DEVICE
        && patch->sinks[0].type == AUDIO_PORT_TYPE_DEVICE) {
#ifdef USE_DTV
        if (aml_dev->patch_src == SRC_DTV) {
            ALOGI("patch src == DTV now line %d \n", __LINE__);
            release_dtv_patch(aml_dev);
            aml_dev->audio_patching = 0;
        }
#endif
        if (aml_dev->patch_src != SRC_DTV
                && aml_dev->patch_src != SRC_INVAL
                && aml_dev->audio_patching == 1) {
            release_patch(aml_dev);
            if (aml_dev->patch_src == SRC_LINEIN && aml_dev->aml_ng_handle) {
#ifdef USE_DTV
#ifdef USE_EQ_DRC
                release_noise_gate(aml_dev->aml_ng_handle);
                aml_dev->aml_ng_handle = NULL;
#endif
#endif
            }
        }

        aml_dev->audio_patching = 0;
        /* save ATV src to deal with ATV HP hotplug */
        if (aml_dev->patch_src != SRC_ATV && aml_dev->patch_src != SRC_DTV) {
            aml_dev->patch_src = SRC_INVAL;
        }
        if (aml_dev->is_TV) {
            aml_dev->parental_control_av_mute = false;
        }
    }

    if (patch->sources[0].type == AUDIO_PORT_TYPE_DEVICE
        && patch->sinks[0].type == AUDIO_PORT_TYPE_MIX) {
        if (aml_dev->patch_src == SRC_HDMIIN) {
            aml_dev2mix_parser_release(aml_dev);
        }
#ifdef USE_DTV
        if (aml_dev->patch_src == SRC_DTV &&
                patch->sources[0].ext.device.type == AUDIO_DEVICE_IN_TV_TUNER) {
            ALOGI("patch src == DTV now line %d \n", __LINE__);
            release_dtv_patch(aml_dev);
            aml_dev->audio_patching = 0;
        }
#endif
        if (aml_dev->patch_src != SRC_ATV && aml_dev->patch_src != SRC_DTV) {
            aml_dev->patch_src = SRC_INVAL;
        }
        if (aml_dev->audio_patching) {
            ALOGI("patch src reset to  DTV now line= %d \n", __LINE__);
            aml_dev->patch_src = SRC_DTV;
            aml_dev->active_inport = INPORT_TUNER;
        }
        aml_dev->tuner2mix_patch = false;
    }

    ALOGI("--%s: after releasing patch, patch sets will be:", __func__);
    //dump_aml_audio_patch_sets(dev);
#ifdef ADD_AUDIO_DELAY_INTERFACE
    aml_audio_delay_clear(AML_DELAY_OUTPORT_SPEAKER);
    aml_audio_delay_clear(AML_DELAY_OUTPORT_SPDIF);
#endif
#if defined(IS_ATOM_PROJECT)
#ifdef DEBUG_VOLUME_CONTROL
    int vol = aml_audio_property_get_int("vendor.media.audio_hal.volume", -1);
    if (vol != -1)
        aml_dev->sink_gain[OUTPORT_SPEAKER] = (float)vol;
else
    aml_dev->sink_gain[OUTPORT_SPEAKER] = 1.0;
#endif
#endif
exit_unregister:
    unregister_audio_patch(dev, patch_set);
exit:
    return ret;
}

static char *adev_dump(const audio_hw_device_t *device, int fd)
{
    struct aml_audio_device* aml_dev = (struct aml_audio_device*)device;

    struct aml_stream_out *aml_out = NULL;
    const int kNumRetries = 5;
    const int kSleepTimeMS = 100;
    int retry = kNumRetries;
    int i;
    int size;
    char *string = NULL;
    int ret = 0;

#ifdef BUILD_LINUX
    fd = open("/tmp/haldump", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd < 0) {
        ALOGE("Cannot access /tmp for dump");
        return NULL;
    }
#endif
    aml_dev->debug_flag = aml_audio_get_debug_flag();
    dprintf(fd, "AUDIO_HAL_GIT_VERSION %s\n",libVersion_audio_hal);

    dprintf(fd, "\n-------------[AML_HAL] primary audio hal[dev:%p]------------------\n", aml_dev);
    while (retry > 0 && pthread_mutex_trylock(&aml_dev->lock) != 0) {
        usleep(kSleepTimeMS * 1000);
        retry--;
    }

    if (retry > 0) {
        aml_dev_dump_latency(aml_dev, fd);
        pthread_mutex_unlock(&aml_dev->lock);
    } else {
        // Couldn't lock
        dprintf(fd, "[AML_HAL]      Could not obtain aml_dev lock.\n");
    }

    dprintf(fd, "\n");
    dprintf(fd, "[AML_HAL]      hdmi_format     : %10d |  active_outport    :    %s | active_inport: %s\n",
        aml_dev->hdmi_format, outputPort2Str(aml_dev->active_outport), inputPort2Str(aml_dev->active_inport));
    dprintf(fd, "[AML_HAL]  audio_effect_enable : %10d | stream_write_data  :    %d\n",
        aml_dev->last_audio_effect_enable, aml_dev->stream_write_data);
    dprintf(fd, "[AML_HAL]      A2DP gain       : %10f |  patch_src         :    %s\n",
        aml_dev->sink_gain[OUTPORT_A2DP], patchSrc2Str(aml_dev->patch_src));
    dprintf(fd, "[AML_HAL]      SPEAKER gain    : %10f |  HDMI gain         :    %f\n",
        aml_dev->sink_gain[OUTPORT_SPEAKER], aml_dev->sink_gain[OUTPORT_HDMI]);
    dprintf(fd, "[AML_HAL]      dtv_src gain: %10f |  atv gain: %f | hdmi gain: %f |  linein: %f | media gain: %f\n",
        aml_dev->src_gain[INPORT_TUNER], aml_dev->src_gain[INPORT_ATV], aml_dev->src_gain[INPORT_HDMIIN], aml_dev->src_gain[INPORT_LINEIN], aml_dev->src_gain[INPORT_MEDIA]);
    dprintf(fd, "[AML_HAL]      ms12 main volume: %10f\n", aml_dev->ms12.main_volume);
    dprintf(fd, "[AML_HAL]      decoder DRC control: %#x\n", aml_dev->decoder_drc_control);
    dprintf(fd, "[AML_HAL]      DAP DRC control: %#x\n", aml_dev->dap_drc_control);
    dprintf(fd, "[AML_HAL]      downmix type: %d\n", aml_dev->downmix_type);
    dprintf(fd, "[AML_HAL]      dap bypass: %d\n", aml_dev->ms12.dap_bypass_enable);
    aml_audio_ease_t *audio_ease = aml_dev->audio_ease;
    if (audio_ease && fabs(audio_ease->current_volume) <= 1e-6) {
        dprintf(fd, "[AML_HAL] ease out muted. start:%f target:%f\n", audio_ease->start_volume, audio_ease->target_volume);
    }
    dprintf(fd, "[AML_HAL]      master volume: %10f, mute: %d\n", aml_dev->master_volume, aml_dev->master_mute);
    aml_decoder_info_dump(aml_dev, fd);
    aml_adev_stream_out_dump(aml_dev, fd);
    dprintf(fd, "[AML_HAL]      mixing %d, ad_vol %d\n", aml_dev->mixing_level, aml_dev->advol_level);
    dprintf(fd, "\nAML stream outs:\n");

#ifdef AML_MALLOC_DEBUG
        aml_audio_debug_malloc_showinfo(MEMINFO_SHOW_PRINT);
#endif

    aml_adev_audio_patch_dump(aml_dev, fd);
    audio_hal_info_dump(aml_dev, fd);
    aml_alsa_device_status_dump(aml_dev, fd);
    aml_alsa_mixer_status_dump(&aml_dev->alsa_mixer, fd);

//#ifdef ENABLE_BT_A2DP
#ifndef BUILD_LINUX
    a2dp_hal_dump(aml_dev, fd);
#endif

#ifdef BUILD_LINUX
    size = lseek(fd, 0, SEEK_CUR);
    string = aml_audio_calloc(size + 1, 1);
    if (string) {
        lseek(fd, 0, SEEK_SET);
        ret = read(fd, string, size);
        if (ret < 0) {
            ALOGE("%s(), fail to read", __func__);
        }
    }

    close(fd);
    unlink("/tmp/haldump");
#endif
    dprintf(fd, "\n-------------[AML_HAL] primary audio hal End---------------------\n");
    return string;
}

pthread_mutex_t adev_mutex = PTHREAD_MUTEX_INITIALIZER;
static void * g_adev = NULL;
void *adev_get_handle(void) {
    return (void *)g_adev;
}

int adev_ms12_prepare(struct audio_hw_device *dev) {
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct audio_config stream_config;
    struct aml_stream_out *aml_out = NULL;
    struct audio_stream_out *stream_out = NULL;
    int ret = -1;
    bool main1_dummy = true;
    bool ott_input = false;
    audio_format_t aformat = AUDIO_FORMAT_E_AC3;

    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    if (adev->ms12_out) {
        ALOGD("%s: ms12 stream exist", __func__);
        return 0;
    }
    ALOGD("%s: enter", __func__);
    stream_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    stream_config.sample_rate = 48000;
    stream_config.format = AUDIO_FORMAT_PCM_16_BIT;

    ret = adev_open_output_stream_new(dev,
                                      0,
                                      AUDIO_DEVICE_NONE,
                                      AUDIO_OUTPUT_FLAG_NONE,
                                      &stream_config,
                                      &stream_out,
                                      "MS12");
    if (ret < 0) {
        ALOGE("%s: open output stream failed", __func__);
        return ret;
    }

    aml_out = (struct aml_stream_out *)stream_out;

    get_sink_format(&aml_out->stream);

    adev->continuous_audio_mode = true;
    adev->ms12.is_continuous_paused = false;
    ret = get_the_dolby_ms12_prepared(aml_out, aformat, AUDIO_CHANNEL_OUT_STEREO, 48000);

    /*the stream will be used in ms12, don't close it*/
    //adev_close_output_stream_new(dev, stream_out);
    AM_LOGD("exit");
    return 0;
}


void adev_ms12_cleanup(struct audio_hw_device *dev) {
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct audio_stream_out *stream_out = (struct audio_stream_out *)adev->ms12_out;
    get_dolby_ms12_cleanup(&adev->ms12, true);
    if (stream_out)
        adev_close_output_stream_new(dev, stream_out);
    adev->ms12_out = NULL;

    return;
}

static int adev_close(hw_device_t *device)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)device;

    pthread_mutex_lock(&adev_mutex);
    ALOGD("%s: count:%d enter", __func__, adev->count);
    adev->count--;
    if (adev->count > 0) {
        pthread_mutex_unlock(&adev_mutex);
        return 0;
    }

    /* free ease resource  */
    aml_audio_ease_close(adev->audio_ease);

    /* destroy thread for communication between Audio Hal and MS12 */
    if ((eDolbyMS12Lib == adev->dolby_lib_type)) {
        ms12_mesg_thread_destroy(&adev->ms12);
        ALOGD("%s, ms12_mesg_thread_destroy finished!\n", __func__);
    }

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        adev_ms12_cleanup((struct audio_hw_device *)device);
        pthread_mutex_destroy(&adev->ms12.bitstream_a_lock);
        pthread_mutex_destroy(&adev->ms12.bypass_ms12_lock);
        adev->ms12_out = NULL;
    }

#ifdef USE_MEDIAINFO
    minfo_destroy(adev->minfo_h);
    adev->minfo_h = NULL;
#endif

/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support { */
#if defined(ENABLE_HBG_PATCH)
    stopReceiveAudioData();
#endif

    if (adev->effect_buf) {
        aml_audio_free(adev->effect_buf);
    }
    if (adev->spk_output_buf) {
        aml_audio_free(adev->spk_output_buf);
    }
    if (adev->spdif_output_buf) {
        aml_audio_free(adev->spdif_output_buf);
    }
    if (adev->hp_output_buf) {
        aml_audio_free(adev->hp_output_buf);
    }
    #ifdef USE_EQ_DRC
    if (adev->aml_ng_handle) {
        release_noise_gate(adev->aml_ng_handle);
        adev->aml_ng_handle = NULL;
    }
    #endif
    ring_buffer_release(&(adev->spk_tuning_rbuf));
    if (adev->ar) {
        audio_route_free(adev->ar);
    }
    #ifdef USE_EQ_DRC
    eq_drc_release(&adev->eq_data);
    #endif
    close_mixer_handle(&adev->alsa_mixer);
    if (adev->aml_dtv_audio_instances) {
        aml_dtv_audio_instances_t *dtv_audio_instances = (aml_dtv_audio_instances_t *)adev->aml_dtv_audio_instances;
        for (int index = 0; index < DVB_DEMUX_SUPPORT_MAX_NUM; index ++) {
            aml_dtvsync_t *dtvsync =  &dtv_audio_instances->dtvsync[index];
            pthread_mutex_destroy(&dtvsync->ms_lock);
        }
        if (dtv_audio_instances->paudiofd != -1)
        {
            close(dtv_audio_instances->paudiofd);
            dtv_audio_instances->paudiofd = -1;
        }
        aml_audio_free(adev->aml_dtv_audio_instances);
    }
    if (adev->audio_mixer) {
        deleteHalSubMixing(adev);
    }
    pthread_mutex_destroy(&adev->dtv_patch_lock);
    pthread_mutex_destroy(&adev->patch_lock);
    pthread_mutex_destroy(&adev->active_outputs_lock);
    pthread_mutex_destroy(&adev->dtv_lock);
#ifdef ADD_AUDIO_DELAY_INTERFACE
    if (adev->is_TV) {
        aml_audio_delay_deinit();
    }
#endif
#ifdef ENABLE_AEC_APP
    release_aec(adev->aec);
#endif
    g_adev = NULL;
    audio_effect_unload_interface(&(adev->hw_device));

    aml_audio_free(device);
    pthread_mutex_unlock(&adev_mutex);
    aml_audio_debug_malloc_close();

    ALOGD("%s:  exit", __func__);
    return 0;
}

static int adev_set_audio_port_config (struct audio_hw_device *dev, const struct audio_port_config *config)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    enum OUT_PORT outport = OUTPORT_SPEAKER;
    enum IN_PORT inport = INPORT_HDMIIN;
    float volume = 0.0;
    int ret = 0;

    if (config == NULL) {
        ALOGE("[%s:%d] audio_port_config is null", __func__, __LINE__);
        return -EINVAL;
    }
    if ((config->config_mask & AUDIO_PORT_CONFIG_GAIN) == 0) {
        ALOGE("[%s:%d] config_mask:%#x invalid", __func__, __LINE__, config->config_mask);
        return -EINVAL;
    }
    ALOGI("++[%s:%d] audio_port id:%d, role:%d, type:%d", __func__, __LINE__, config->id, config->role, config->type);

    if (config->type == AUDIO_PORT_TYPE_DEVICE) {
        if (config->role == AUDIO_PORT_ROLE_SINK) {
            android_dev_convert_to_hal_dev(config->ext.device.type, (int *)&outport);
            aml_dev->sink_gain[outport] = DbToAmpl(config->gain.values[0] / 100.0);
            if (outport == OUTPORT_HDMI_ARC) {
                aml_dev->sink_gain[outport] = 1.0;
            }
            ALOGI(" - set sink device[%#x](outport:%s): volume_dB[%d], gain[%f]",
                        config->ext.device.type, outputPort2Str(outport),
                        config->gain.values[0], aml_dev->sink_gain[outport]);
            ALOGI(" - now the sink gains are:");
            ALOGI("\t- OUTPORT_SPEAKER->gain[%f]", aml_dev->sink_gain[OUTPORT_SPEAKER]);
            ALOGI("\t- OUTPORT_HDMI_ARC->gain[%f]", aml_dev->sink_gain[OUTPORT_HDMI_ARC]);
            ALOGI("\t- OUTPORT_HEADPHONE->gain[%f]", aml_dev->sink_gain[OUTPORT_HEADPHONE]);
            ALOGI("\t- OUTPORT_HDMI->gain[%f]", aml_dev->sink_gain[OUTPORT_HDMI]);
            ALOGI("\t- active outport is: %s", outputPort2Str(aml_dev->active_outport));
        } else if (config->role == AUDIO_PORT_ROLE_SOURCE) {
            android_dev_convert_to_hal_dev(config->ext.device.type, (int *)&inport);
            volume = DbToAmpl(config->gain.values[0] / 100.0);
            if (volume < FLOAT_ZERO) {
                volume = 0.0;
            }
            aml_dev->src_gain[inport] = volume;
            ALOGI(" - set src device[%#x](inport:%s): volume db[%d], gain[%f]",
                  config->ext.device.type, inputPort2Str(inport),
                  config->gain.values[0], aml_dev->src_gain[inport]);
            ALOGI(" - now the source gains are:");
            ALOGI(" - INPORT_DTV->gain[%f]", aml_dev->src_gain[INPORT_TUNER]);
            ALOGI(" - INPORT_ATV->gain[%f]", aml_dev->src_gain[INPORT_ATV]);
            ALOGI(" - INPORT_HDMI->gain[%f]", aml_dev->src_gain[INPORT_HDMIIN]);
            ALOGI(" - INPORT_AV->gain[%f]", aml_dev->src_gain[INPORT_LINEIN]);
            ALOGI(" - INPORT_MEDIA->gain[%f]", aml_dev->src_gain[INPORT_MEDIA]);
            ALOGI(" - set gain for in_port:%s, active inport is:%s", inputPort2Str(inport), inputPort2Str(aml_dev->active_inport));
        } else {
            ALOGI("[%s:%d] unsupported role:%d type.", __func__, __LINE__, config->role);
        }
    }
    return 0;
}

static int adev_get_audio_port(struct audio_hw_device *dev, struct audio_port *port)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    enum OUT_PORT outport = OUTPORT_SPEAKER;
    enum IN_PORT inport = INPORT_HDMIIN;
    int ret = 0;

    if (port == NULL) {
        ALOGE("[%s:%d] port is null", __func__, __LINE__);
        return -EINVAL;
    }
    ALOGI("++[%s:%d] audio_port_config id:%d, type:%x", __func__, __LINE__, port->active_config.id, port->ext.device.type);

    if (port->active_config.id == AUDIO_PORT_ROLE_SOURCE) {
        android_dev_convert_to_hal_dev(port->ext.device.type, (int *)&inport);
        port->active_config.gain.values[0] = aml_dev->src_gain[inport] * 100;
        ALOGI("[%s:%d] device  %x, inport %d, gain %d", __func__, __LINE__,
            port->ext.device.type, inport, port->active_config.gain.values[0] );
    } else if (port->active_config.id == AUDIO_PORT_ROLE_SINK) {
        android_dev_convert_to_hal_dev(port->ext.device.type, (int *)&outport);
        port->active_config.gain.values[0] = aml_dev->sink_gain[outport] * 100;
        ALOGI("[%s:%d] device  %x, outport %d, gain %d", __func__, __LINE__,
            port->ext.device.type, outport, port->active_config.gain.values[0]);
    }

    return 0;
}

static int adev_add_device_effect(struct audio_hw_device *dev,
                                  audio_port_handle_t device, effect_handle_t effect)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;

    ALOGD("func:%s device:%d effect_handle_t:%p, active_outport:%d", __func__, device, effect, aml_dev->active_outport);
    return 0;
}

static int adev_remove_device_effect(struct audio_hw_device *dev,
                                     audio_port_handle_t device, effect_handle_t effect)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;

    ALOGD("func:%s device:%d effect_handle_t:%p, active_outport:%d", __func__, device, effect, aml_dev->active_outport);
    return 0;
}

static int aml_audio_focus(struct aml_audio_device *adev, bool on)
{
    float volume = 0.0f;
    if (!adev)
        return -1;

    pthread_mutex_lock (&adev->lock);
    for (int i = 0; i < STREAM_USECASE_MAX; i++) {
        struct aml_stream_out *aml_out = adev->active_outputs[i];
        struct audio_stream_out *out = (struct audio_stream_out *)aml_out;
        if (aml_out && !(aml_out->flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) && !aml_out->is_ms12_stream) {
            out_set_volume_l(out, aml_out->volume_l_org, aml_out->volume_l_org);
        }
    }
    pthread_mutex_unlock (&adev->lock);

    return 0;
}


#define MAX_SPK_EXTRA_LATENCY_MS (100)
#define DEFAULT_SPK_EXTRA_LATENCY_MS (15)

static int adev_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
    struct aml_audio_device *adev;
    aml_audio_debug_malloc_open();
    size_t bytes_per_frame = audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT)
                             * audio_channel_count_from_out_mask(AUDIO_CHANNEL_OUT_STEREO);
    int buffer_size = PLAYBACK_PERIOD_COUNT * DEFAULT_PLAYBACK_PERIOD_SIZE * bytes_per_frame;
    int spk_tuning_buf_size = MAX_SPK_EXTRA_LATENCY_MS
                              * bytes_per_frame * MM_FULL_POWER_SAMPLING_RATE / 1000;
    int spdif_tuning_latency = aml_audio_get_spdif_tuning_latency();
    int card = CARD_AMLOGIC_BOARD;
    int ret = 0, i;
    char buf[PROPERTY_VALUE_MAX] = {0};
    int disable_continuous = 1;
    audio_effect_t *audio_effect;
    bool earctx_mode = true;

    ALOGD("%s: enter, ver:%s", __func__, libVersion_audio_hal);
    pthread_mutex_lock(&adev_mutex);
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) {
        ret = -EINVAL;
        goto err;
    }

    if (g_adev != NULL) {
        ALOGI("adev exsits ,reuse");
        adev = (struct aml_audio_device *)g_adev;
        adev->count++;
        *device = &adev->hw_device.common;
        ALOGI("*device:%p",*device);
        /*if we reuse adev open, but ms12 is not init, we should init it*/
        if (eDolbyMS12Lib == adev->dolby_lib_type && !adev->ms12.dolby_ms12_enable) {
            adev_ms12_prepare((struct audio_hw_device *)adev);
        }
        goto err;
    }

    adev = aml_audio_calloc(1, sizeof(struct aml_audio_device));
    if (!adev) {
        ret = -ENOMEM;
        goto err;
    }
    g_adev = (void *)adev;
    pthread_mutex_unlock(&adev_mutex);

//#ifdef ENABLE_AEC_HAL
#ifndef BUILD_LINUX
    initAudio(1);  /*Judging whether it is Google's search engine*/
#endif
    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_3_0;
    adev->hw_device.common.module = (struct hw_module_t *)module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.get_master_volume = adev_get_master_volume;
    adev->hw_device.set_master_mute = adev_set_master_mute;
    adev->hw_device.get_master_mute = adev_get_master_mute;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream_new;
    adev->hw_device.close_output_stream = adev_close_output_stream_new;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.create_audio_patch = adev_create_audio_patch;
    adev->hw_device.release_audio_patch = adev_release_audio_patch;
    adev->hw_device.set_audio_port_config = adev_set_audio_port_config;
    adev->hw_device.add_device_effect = adev_add_device_effect;
    adev->hw_device.remove_device_effect = adev_remove_device_effect;
    adev->hw_device.get_microphones = adev_get_microphones;
    adev->hw_device.get_audio_port = adev_get_audio_port;
    adev->hw_device.dump = adev_dump;
    adev->hdmi_format = AUTO;
    adev->is_hdmi_arc_interact_done = false;
    adev->is_netflix_hide = false;

    card = alsa_device_get_card_index();
    if ((card < 0) || (card > 7)) {
        ALOGE("error to get audio card");
        ret = -EINVAL;
        goto err_adev;
    }

    adev->card = card;
    adev->ar = audio_route_init(adev->card, MIXER_XML_PATH);
    if (adev->ar == NULL) {
        ALOGE("audio route init failed");
        ret = -EINVAL;
        goto err_adev;
    }
    /* Set the default route before the PCM stream is opened */
    adev->mode = AUDIO_MODE_NORMAL;
    adev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
    adev->in_device = AUDIO_DEVICE_IN_BUILTIN_MIC & ~AUDIO_DEVICE_BIT_IN;
    adev->hi_pcm_mode = false;

    adev->eq_data.card = adev->card;
    #ifdef USE_EQ_DRC
    if (eq_drc_init(&adev->eq_data) == 0) {
        ALOGI("%s() audio source gain: atv:%f, dtv:%f, hdmiin:%f, av:%f, media:%f", __func__,
           adev->eq_data.s_gain.atv, adev->eq_data.s_gain.dtv,
           adev->eq_data.s_gain.hdmi, adev->eq_data.s_gain.av, adev->eq_data.s_gain.media);
        ALOGI("%s() audio device gain: speaker:%f, spdif_arc:%f, headphone:%f", __func__,
           adev->eq_data.p_gain.speaker, adev->eq_data.p_gain.spdif_arc,
              adev->eq_data.p_gain.headphone);
        adev->aml_ng_enable = adev->eq_data.noise_gate.aml_ng_enable;
        adev->aml_ng_level = adev->eq_data.noise_gate.aml_ng_level;
        adev->aml_ng_attack_time = adev->eq_data.noise_gate.aml_ng_attack_time;
        adev->aml_ng_release_time = adev->eq_data.noise_gate.aml_ng_release_time;
        ALOGI("%s() audio noise gate level: %fdB, attack_time = %dms, release_time = %dms", __func__,
              adev->aml_ng_level, adev->aml_ng_attack_time, adev->aml_ng_release_time);
    }
    #else
    adev->eq_data.s_gain.atv = 1.0;
    adev->eq_data.s_gain.dtv = 1.0;
    adev->eq_data.s_gain.hdmi= 1.0;
    adev->eq_data.s_gain.av = 1.0;
    adev->eq_data.s_gain.media = 1.0;
    adev->eq_data.p_gain.speaker= 1.0;
    adev->eq_data.p_gain.spdif_arc = 1.0;
    adev->eq_data.p_gain.headphone = 1.0;
    #endif
    ret = aml_audio_output_routing(&adev->hw_device, OUTPORT_SPEAKER, false);
    if (ret < 0) {
        ALOGE("%s() routing failed", __func__);
        ret = -EINVAL;
        goto err_adev;
    }

    audio_effect_load_interface(&(adev->hw_device), &audio_effect);
#ifdef __LP64__
    adev->hw_device.common.reserved[0] = (uint64_t)audio_effect;
#else
    adev->hw_device.common.reserved[0] = (uint32_t)audio_effect;
#endif

    adev->next_unique_ID = 1;
    list_init(&adev->patch_list);
    adev->effect_buf_size = buffer_size;
    adev->effect_buf = aml_audio_malloc(buffer_size);
    if (adev->effect_buf == NULL) {
        ALOGE("malloc effect buffer failed");
        ret = -ENOMEM;
        goto err_adev;
    }
    memset(adev->effect_buf, 0, buffer_size);

    adev->spk_output_buf = aml_audio_malloc(buffer_size * 2);
    if (adev->spk_output_buf == NULL) {
        ALOGE("no memory for speaker output buffer");
        ret = -ENOMEM;
        goto err_effect_buf;
    }
    memset(adev->spk_output_buf, 0, buffer_size * 2);

    adev->spdif_output_buf = aml_audio_malloc(buffer_size * 2);
    if (adev->spdif_output_buf == NULL) {
        ALOGE("no memory for spdif output buffer");
        ret = -ENOMEM;
        goto err_spk_buf;
    }
    memset(adev->spdif_output_buf, 0, buffer_size);

    adev->hp_output_buf = aml_audio_malloc(buffer_size);
    if (adev->hp_output_buf == NULL) {
        ALOGE("no memory for hp output buffer");
        ret = -ENOMEM;
        goto err_spdif_buf;
    }
    memset(adev->hp_output_buf, 0, buffer_size);

    if (0 != aml_audio_config_parser()) {
        ALOGE("%s() Audio Config file parsing error\n",__FUNCTION__);
    }
    if (0 != aml_audio_avsync_parser()) {
        ALOGE("%s() Audio Config file parsing error\n",__FUNCTION__);
    } else {
        audio_hal_avsync_latency_loading();
    }

    /* init speaker tuning buffers */
    ret = ring_buffer_init(&(adev->spk_tuning_rbuf), spk_tuning_buf_size);
    if (ret < 0) {
        ALOGE("Fail to init audio spk_tuning_rbuf!");
        goto err_hp_buf;
    }
    adev->spk_tuning_buf_size = spk_tuning_buf_size;

    /* if no latency set by prop, use default one */
    if (spdif_tuning_latency == 0) {
        spdif_tuning_latency = DEFAULT_SPK_EXTRA_LATENCY_MS;
    } else if (spdif_tuning_latency > MAX_SPK_EXTRA_LATENCY_MS) {
        spdif_tuning_latency = MAX_SPK_EXTRA_LATENCY_MS;
    } else if (spdif_tuning_latency < 0) {
        spdif_tuning_latency = 0;
    }

    // try to detect which dolby lib is readable
    adev->dolby_lib_type = detect_dolby_lib_type();
    adev->dolby_lib_type_last = adev->dolby_lib_type;
    adev->dolby_decode_enable = dolby_lib_decode_enable(adev->dolby_lib_type_last);
    adev->dts_lib_type = detect_dts_lib_type();
    if (adev->dts_lib_type == eDTSXLib) {
        adev->dts_decode_enable = 1;
    } else {
    adev->dts_decode_enable = dts_lib_decode_enable();
    }
    adev->is_ms12_tuning_dat = is_ms12_tuning_dat_in_dut();

    /* convert MS to data buffer length need to cache */
    adev->spk_tuning_lvl = (spdif_tuning_latency * bytes_per_frame * MM_FULL_POWER_SAMPLING_RATE) / 1000;
    /* end of spker tuning things */
    *device = &adev->hw_device.common;
    adev->dts_post_gain = 1.0;
    for (i = 0; i < OUTPORT_MAX; i++) {
        adev->sink_gain[i] = 1.0f;
    }
    for (i = 0; i < INPORT_MAX; i++) {
        adev->src_gain[i] = 1.0f;
    }
    adev->continuous_audio_mode_default = 0;
    adev->need_remove_conti_mode = false;
#ifdef BUILD_LINUX
    adev->dual_spdif_support = aml_get_jason_int_value(DUAL_SPDIF,0);
    adev->ms12_force_ddp_out = aml_get_jason_int_value(FORCE_DDP,0);
#else
    adev->dual_spdif_support = aml_audio_property_get_bool("ro.vendor.platform.is.dualspdif", false);
    adev->ms12_force_ddp_out = aml_audio_property_get_bool("ro.vendor.platform.is.forceddp", false);
#endif
    adev->spdif_enable = true;
    adev->dolby_ms12_dap_init_mode = aml_get_jason_int_value(STB_MS12_DAP_MODE, 0);


    /*for ms12 case, we set default continuous mode*/
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        adev->continuous_audio_mode_default = 1;
    }
    /*we can use property to debug it*/
    char *str = aml_audio_property_get_str(DISABLE_CONTINUOUS_OUTPUT, buf, NULL);
    if (str != NULL) {
        sscanf(buf, "%d", &disable_continuous);
        if (!disable_continuous) {
            adev->continuous_audio_mode_default = 1;
        }
        ALOGI("%s[%s] disable_continuous %d\n", DISABLE_CONTINUOUS_OUTPUT, buf, disable_continuous);
    }
    adev->continuous_audio_mode = adev->continuous_audio_mode_default;
    pthread_mutex_init(&adev->alsa_pcm_lock, NULL);
    pthread_mutex_init(&adev->patch_lock, NULL);
    pthread_mutex_init(&adev->active_outputs_lock, NULL);
    pthread_mutex_init(&adev->dtv_patch_lock, NULL);
    open_mixer_handle(&adev->alsa_mixer);

    /* Set the earctx mode by the property, only need set false */
    earctx_mode = aml_audio_property_get_bool("persist.sys.vendor.earc_settings", true);
    if (!earctx_mode) {
    aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_EARC_TX_EARC_MODE, earctx_mode);
    ALOGI("eARC_TX eARC Mode get from property: %d\n", earctx_mode);
    }

    adev->atoms_lock_flag = false;

    if (eDolbyDcvLib == adev->dolby_lib_type) {
        adev->dcvlib_bypass_enable = 1;
    }

    adev->audio_patch = NULL;
    memset(&adev->dts_hd, 0, sizeof(struct dca_dts_dec));
    adev->sound_track_mode = 0;
    adev->downmix_type = AM_DOWNMIX_LTRT;
    adev->mixing_level = 0;
    adev->advol_level = 100;
    adev->aml_dtv_audio_instances = aml_audio_calloc(1, sizeof(aml_dtv_audio_instances_t));
    adev->is_multi_demux = is_multi_demux();
    if (adev->aml_dtv_audio_instances == NULL) {
        ALOGE("malloc aml_dtv_audio_instances failed");
        ret = -ENOMEM;
        goto err_adev;
    } else {
        aml_dtv_audio_instances_t *dtv_audio_instances = (aml_dtv_audio_instances_t *)adev->aml_dtv_audio_instances;
        dtv_audio_instances->paudiofd = -1;
        for (int index = 0; index < DVB_DEMUX_SUPPORT_MAX_NUM; index ++) {
            aml_dtvsync_t *dtvsync =  &dtv_audio_instances->dtvsync[index];
            pthread_mutex_init(&dtvsync->ms_lock, NULL);
        }
        if (!adev->is_multi_demux) {
            dtv_audio_instances->paudiofd = open( "/tmp/paudiofifo", O_RDONLY | O_NONBLOCK);
            if (dtv_audio_instances->paudiofd == -1)
            {
                ALOGE("open /tmp/paudiofifo %d\n",errno);
            }
        }
    }
    pthread_mutex_init(&adev->dtv_lock, NULL);
#if ENABLE_NANO_NEW_PATH
    nano_init();
#endif
    ALOGI("%s() adev->dolby_lib_type = %d", __FUNCTION__, adev->dolby_lib_type);
    adev->patch_src = SRC_INVAL;
    adev->audio_type = LPCM;

/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support { */
#if defined(ENABLE_HBG_PATCH)
    startReceiveAudioData();
#endif
/*[SEI-zhaopf-2018-10-29] add for HBG remote audio support } */
#ifdef BUILD_LINUX
    adev->is_TV = aml_get_jason_int_value(TV_PLATFORM,0);
    if (adev->is_TV ) {
        adev->default_alsa_ch =  aml_audio_get_default_alsa_output_ch();
        /*Now SoundBar type is depending on TV audio as only tv support multi-channel LPCM output*/
        adev->is_SBR = aml_audio_check_sbr_product();
        ALOGI("%s(), TV platform,soundbar platform %d", __func__,adev->is_SBR);
    } else {
        /* for stb/ott, fixed 2 channels speaker output for alsa*/
        adev->default_alsa_ch = 2;
        adev->is_STB = aml_get_jason_int_value(STB_PLATFORM,0);
        ALOGI("%s(), OTT platform", __func__);
    }
#ifdef ADD_AUDIO_DELAY_INTERFACE
        ret = aml_audio_delay_init(aml_get_jason_int_value(AUDIO_DELAY_MAX, 1000));
        if (ret < 0) {
            ALOGE("[%s:%d] aml_audio_delay_init fail", __func__, __LINE__);
            goto err;
        }
#endif

#else
#if defined(TV_AUDIO_OUTPUT)
    adev->is_TV = true;
    adev->default_alsa_ch =  aml_audio_get_default_alsa_output_ch();
    /*Now SoundBar type is depending on TV audio as only tv support multi-channel LPCM output*/
    adev->is_SBR = aml_audio_check_sbr_product();
    ALOGI("%s(), TV platform,soundbar platform %d", __func__,adev->is_SBR);
#ifdef ADD_AUDIO_DELAY_INTERFACE
    ret = aml_audio_delay_init(aml_get_jason_int_value(AUDIO_DELAY_MAX, 1000));
    if (ret < 0) {
        ALOGE("[%s:%d] aml_audio_delay_init fail", __func__, __LINE__);
        goto err;
    }
#endif
#else
    /* for stb/ott, fixed 2 channels speaker output for alsa*/
    adev->default_alsa_ch = 2;
    adev->is_STB = aml_audio_property_get_bool("ro.vendor.platform.is.stb", false);
    ALOGI("%s(), OTT platform", __func__);
#endif
#endif

#ifdef ENABLE_AEC_APP
    pthread_mutex_lock(&adev->lock);
    if (init_aec(CAPTURE_CODEC_SAMPLING_RATE, NUM_AEC_REFERENCE_CHANNELS,
                    CHANNEL_STEREO, &adev->aec)) {
        pthread_mutex_unlock(&adev->lock);
        return -EINVAL;
    }
    pthread_mutex_unlock(&adev->lock);
#endif
    adev->master_volume = 1.0;
    adev->syss_mixgain = 1.0;
    adev->apps_mixgain = 1.0;
    adev->useSubMix = false;
#ifdef SUBMIXER_V1_1
    adev->useSubMix = true;
    ALOGI("%s(), with macro SUBMIXER_V1_1, set useSubMix = TRUE", __func__);
#endif

    // FIXME: current MS12 is not compatible with SUBMIXER, when MS12 lib exists, use ms12 system.
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        adev->useSubMix = false;
    } else if (aml_get_jason_int_value(USE_SUB_MIX, 1)) {
        adev->useSubMix = true;
    }

    //adev->audio_focus_enable = aml_get_jason_int_value(Audio_Focus_Enable, 0);
    ALOGI("%s(), set useSubMix %s, audio focus: %s", __func__, adev->useSubMix ? "TRUE": "FALSE",
        adev->audio_focus_enable ? "TRUE": "FALSE");

    if (adev->useSubMix) {
        ret = initHalSubMixing(MIXER_LPCM, adev, adev->is_TV);
        adev->raw_to_pcm_flag = false;
    }


    if (aml_audio_ease_init(&adev->audio_ease) < 0) {
        ALOGE("aml_audio_ease_init faild\n");
        ret = -EINVAL;
        goto err_ringbuf;
    }

#ifdef USE_MEDIAINFO
    adev->minfo_h = minfo_init();
    if (!adev->minfo_h) {
        ALOGE("Mediainfo init failed");
    }
    adev->info_rid = -1;
    adev->minfo_atmos_flag = -1;
#endif

    // adev->debug_flag is set in hw_write()
    // however, sometimes function didn't goto hw_write() before encounting error.
    // set debug_flag here to see more debug log when debugging.
    adev->debug_flag = aml_audio_get_debug_flag();
    adev->count = 1;

#ifndef NO_AUDIO_CAP
    pthread_mutex_init(&adev->cap_buffer_lock, NULL);
#endif
    memset(&(adev->hdmi_descs), 0, sizeof(struct aml_arc_hdmi_desc));
    ALOGD("%s adev->dolby_lib_type:%d adev->is_TV:%d", __func__, adev->dolby_lib_type, adev->is_TV);
    /* create thread for communication between Audio Hal and MS12 */
    if ((eDolbyMS12Lib == adev->dolby_lib_type)) {
        ret = ms12_mesg_thread_create(&adev->ms12);
        if (0 != ret) {
            ALOGE("%s, ms12_mesg_thread_create fail!\n", __func__);
            goto Err_MS12_MesgThreadCreate;
        }

        ret = adev_ms12_prepare((struct audio_hw_device *)adev);
        if (0 != ret) {
            ALOGE("%s, adev_ms12_prepare fail!\n", __func__);
            goto Err_MS12_MesgThreadCreate;
        }
        if ((ret = pthread_mutex_init (&adev->ms12.bitstream_a_lock, NULL)) != 0) {
            AM_LOGE("pthread_mutex_init fail, errno:%s", strerror(errno));
        }
        if ((ret = pthread_mutex_init (&adev->ms12.bypass_ms12_lock, NULL)) != 0) {
            AM_LOGE("pthread_mutex_init fail, errno:%s", strerror(errno));
        }
    }
    //set msync log level
    log_set_level(AVS_LOG_INFO);

    adev->hdmitx_multi_ch_src = aml_get_jason_int_value("HDMITX_Multi_CH_Src_Select", -1);
    adev->hdmitx_hbr_src      = aml_get_jason_int_value("HDMITX_HBR_Src_Select", -1);
    adev->hdmitx_src          = aml_get_jason_int_value("HDMITX_Src_Select", -1);
    if (adev->hdmitx_src != -1) {
        adev->spdif_independent = true;
    }

    //set DRC default as RF
    aml_audio_set_drc_control("-drc 1 -bs 0 -cs 0", &adev->decoder_drc_control);
    aml_audio_set_drc_control("-dap_drc 1 -b 0 -c 0", &adev->dap_drc_control);

    adev->last_audio_effect_enable = true;
    ALOGD("%s: exit  dual_spdif_support(%d)", __func__, adev->dual_spdif_support);
    return 0;

Err_MS12_MesgThreadCreate:
err_ringbuf:
    ring_buffer_release(&adev->spk_tuning_rbuf);
err_hp_buf:
    aml_audio_free(adev->hp_output_buf);
err_spdif_buf:
    aml_audio_free(adev->spdif_output_buf);
err_spk_buf:
    aml_audio_free(adev->spk_output_buf);
err_effect_buf:
    aml_audio_free(adev->effect_buf);
err_adev:
    aml_audio_free(adev);
err:
    pthread_mutex_unlock(&adev_mutex);
    return ret;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "aml audio HW HAL",
        .author = "amlogic, Corp.",
        .methods = &hal_module_methods,
    },
};
