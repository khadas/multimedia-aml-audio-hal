/*
 * Copyright (C) 2018 Amlogic Corporation.
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

#define LOG_TAG "audio_hw_hal_dtv"
//#define LOG_NDEBUG 0

#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/hardware.h>
#include <inttypes.h>
#include <linux/ioctl.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/system_properties.h>
#include <system/audio.h>
#include <time.h>
#include <utils/Timers.h>

#if ANDROID_PLATFORM_SDK_VERSION >= 25 // 8.0
#include <system/audio-base.h>
#endif

#include <hardware/audio.h>

#include <aml_android_utils.h>
#include <aml_data_utils.h>

#include "aml_config_data.h"
#include "aml_audio_stream.h"
#include "audio_hw.h"
#include "audio_hw_dtv.h"
#include "audio_hw_profile.h"
#include "audio_hw_utils.h"
//#include "dtv_patch_out.h"
#include "aml_audio_resampler.h"
#if defined(MS12_V24_ENABLE) || defined(MS12_V26_ENABLE)
#include "audio_hw_ms12_v2.h"
#endif
#include "dolby_lib_api.h"
#include "audio_dtv_ad.h"
#include "alsa_config_parameters.h"
#include "alsa_device_parser.h"
#include "aml_audio_hal_avsync.h"
#include "aml_audio_spdifout.h"
#include "aml_audio_timer.h"
#include "aml_volume_utils.h"
#include "dmx_audio_es.h"
#include "uio_audio_api.h"
#include "aml_ddp_dec_api.h"
#include "audio_dtv_utils.h"
#include "audio_hw_ms12_common.h"

#define DTV_SKIPAMADEC "vendor.dtv.audio.skipamadec"
#define DTV_SYNCENABLE "vendor.media.dtvsync.enable"
#define DTV_ADSWITCH_PROPERTY   "vendor.media.audiohal.adswitch"

static struct timespec start_time;
const unsigned int mute_dd_frame[] = {
    0x5d9c770b, 0xf0432014, 0xf3010713, 0x2020dc62, 0x4842020, 0x57100404, 0xf97c3e1f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0xf97c75fe, 0x9fcfe7f3,
    0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0x3e5f9dff, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0x48149ff2, 0x2091,
    0x361e0000, 0x78bc6ddb, 0xbbbbe3f1, 0xb8, 0x0, 0x0, 0x0, 0x77770700, 0x361e8f77, 0x359f6fdb, 0xd65a6bad, 0x5a6badb5, 0x6badb5d6, 0xa0b5d65a, 0x1e000000, 0xbc6ddb36,
    0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35, 0xa6b5d6, 0x0, 0xb66de301, 0x1e8fc7db, 0x80bbbb3b, 0x0, 0x0,
    0x0, 0x0, 0x78777777, 0xb66de3f1, 0xd65af3f9, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x5a6b, 0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0,
    0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0x605a, 0x1e000000, 0xbc6ddb36, 0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35,
    0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0xa0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0, 0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xa6b5d65a, 0x0,
    0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0, 0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0x5a6bad, 0xe3010000, 0xc7dbb66d,
    0xbb3b1e8f, 0x80bb, 0x0, 0x0, 0x0, 0x77770000, 0xe3f17877, 0xf3f9b66d, 0xadb5d65a, 0x605a6b, 0x0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0,
    0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xa0b5, 0xdb361e00, 0xf178bc6d, 0xb8bbbbe3, 0x0, 0x0, 0x0, 0x0,
    0x77777707, 0xdb361e8f, 0xad359f6f, 0xb5d65a6b, 0x10200a6, 0x0, 0xdbb6f100, 0x8fc7e36d, 0xc0dddd1d, 0x0, 0x0, 0x0, 0x0, 0xbcbbbb3b, 0xdbb6f178, 0x6badf97c,
    0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xadb5, 0xb6f10000, 0xc7e36ddb, 0xdddd1d8f, 0xc0, 0x0, 0x0, 0x0, 0xbbbb3b00, 0xb6f178bc, 0xadf97cdb, 0xb5d65a6b, 0x4deb00ad
};

const unsigned int mute_ddp_frame[] = {
    0x7f01770b, 0x20e06734, 0x2004, 0x8084500, 0x404046c, 0x1010104, 0xe7630001, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xce7f9fcf, 0x7c3e9faf,
    0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xf37f9fcf, 0x9fcfe7ab, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x53dee7f3, 0xf0e9,
    0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d,
    0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0,
    0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0,
    0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a,
    0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0,
    0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db,
    0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0,
    0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000,
    0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5,
    0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x40, 0x7f227c55,
};
static int pcr_apts_diff;

static int create_dtv_output_stream_thread(struct aml_audio_patch *patch);
static int release_dtv_output_stream_thread(struct aml_audio_patch *patch);
static int create_dtv_input_stream_thread(struct aml_audio_patch *patch);
static int release_dtv_input_stream_thread(struct aml_audio_patch *patch);

static void ts_wait_time(struct timespec *ts, uint32_t time)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += time / 1000000;
    ts->tv_nsec += (time * 1000) % 1000000000;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -=1000000000;
    }
}

static void dtv_do_ease_out(struct aml_audio_device *aml_dev)
{
    if (aml_dev && aml_dev->audio_ease) {
        AM_LOGI("%s(), do fade out", __func__);
        start_ease_out(aml_dev);
        if (aml_dev->is_TV)
            usleep(AUDIO_FADEOUT_TV_DURATION_US);
        else
            usleep(AUDIO_FADEOUT_STB_DURATION_US);
    }
}
static void dtv_check_audio_reset()
{
    AM_LOGI("reset dtv audio port\n");
    aml_sysfs_set_str(AMSTREAM_AUDIO_PORT_RESET, "1");
}

void  clean_dtv_demux_info(aml_demux_audiopara_t *demux_info) {
    demux_info->demux_id = -1;
    demux_info->security_mem_level  = -1;
    demux_info->output_mode  = -1;
    demux_info->has_video  = 0;
    demux_info->main_fmt  = -1;
    demux_info->main_pid  = -1;
    demux_info->ad_fmt  = -1;
    demux_info->ad_pid  = -1;
    //demux_info->advol_level = 0;
    //demux_info->mixing_level = -32;
    demux_info->associate_audio_mixing_enable  = 0;
    demux_info->media_sync_id  = -1;
    demux_info->media_presentation_id  = -1;
    demux_info->media_first_lang  = -1;
    demux_info->media_second_lang  = -1;
    demux_info->ad_package_status  = -1;
}

int dtv_patch_handle_event(struct audio_hw_device *dev,int cmd, int val) {

    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int has_audio = 1;
    int audio_sync_mode = 0;
    float dtv_volume_switch = 1.0;
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)adev->aml_dtv_audio_instances;
    unsigned int path_id = val >> DVB_DEMUX_ID_BASE;

    if (NULL == patch) {
        AM_LOGE("patch NULL error, need create patch first!!");
        goto exit;
    }

    AM_LOGI("%p path_id %d cmd %d val %d\n", patch, path_id, cmd,val & ((1 << DVB_DEMUX_ID_BASE) - 1));
    if (path_id < 0  ||  path_id >= DVB_DEMUX_SUPPORT_MAX_NUM) {
        AM_LOGW("path_id %d is invalid !",path_id);
        goto exit;
    }

    void *demux_handle = dtv_audio_instances->demux_handle[path_id];
    aml_demux_audiopara_t *demux_info = &dtv_audio_instances->demux_info[path_id];
    aml_dtvsync_t *dtvsync =  &dtv_audio_instances->dtvsync[path_id];
    val = val & ((1 << DVB_DEMUX_ID_BASE) - 1);
    switch (cmd) {
        case AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID:
            demux_info->media_sync_id = val;
            AM_LOGI("demux_info->media_sync_id  %d", demux_info->media_sync_id);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE:
            AM_LOGI("DTV sound mode %d ", val);
            demux_info->output_mode = val;
            patch->mode = val;
            break;
        case AUDIO_DTV_PATCH_CMD_SET_MUTE:
            AM_LOGE ("Amlogic_HAL - %s: TV-Mute:%d.", __FUNCTION__,val);
            adev->tv_mute = val;
            break;
        case AUDIO_DTV_PATCH_CMD_SET_VOLUME:
            dtv_volume_switch = (float)val / 100; // val range is [0, 100], conversion range is [0, 1]
            if (patch->dtv_volume != dtv_volume_switch && dtv_volume_switch >= 0.0f && dtv_volume_switch <= 1.0f) {
                patch->dtv_volume = dtv_volume_switch;
                AM_LOGI ("dtv set volume:%f", patch->dtv_volume);
                if (patch->dtv_aml_out) {
                    struct audio_stream_out *stream_out = (struct audio_stream_out *)patch->dtv_aml_out;
                    stream_out->set_volume(stream_out, patch->dtv_volume, patch->dtv_volume);
                }
            } else {
                AM_LOGE("dtv set volume error! volume:%f", dtv_volume_switch);
            }
            break;
        case AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO:
            demux_info->has_video = val;
            AM_LOGI("has_video %d",demux_info->has_video);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO:
            demux_info->demux_id = val;
            AM_LOGI("demux_id %d",demux_info->demux_id);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL:
            demux_info->security_mem_level = val;
            AM_LOGI("security_mem_level set to %d", demux_info->security_mem_level);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_PID:
            demux_info->main_pid = val;
            AM_LOGI("main_pid %d",demux_info->main_pid);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_FMT:
            demux_info->main_fmt = val;
            AM_LOGI("main_fmt %d",demux_info->main_fmt);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_FMT:
            demux_info->ad_fmt = val;
            AM_LOGI("ad_fmt %d",demux_info->ad_fmt);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_PID:
            demux_info->ad_pid = val;
            AM_LOGI("ad_pid %d",demux_info->ad_pid);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT:
            /* do nothing for AD enable use cmd:AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE
               ad support(dual_decoder_support) is default support. */
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE:
            pthread_mutex_lock(&adev->lock);
            adev->associate_audio_mixing_enable = val;
            demux_info->associate_audio_mixing_enable = adev->associate_audio_mixing_enable;
            AM_LOGI("associate_audio_mixing_enable set to %d", adev->associate_audio_mixing_enable);

            pthread_mutex_unlock(&adev->lock);
            pthread_mutex_lock(&patch->dtv_cmd_process_mutex);
            if (path_id == dtv_audio_instances->demux_index_working) {
                dtv_patch_add_cmd(patch->dtv_cmd_list, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE, path_id);
                pthread_cond_signal(&patch->dtv_cmd_process_cond);
            } else {
                AM_LOGI("path_id %d not work ,cmd %d invalid", path_id, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE);
            }
            pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL:
            pthread_mutex_lock(&adev->lock);
            if (val < 0) {
                val = 0;
            } else if (val > 100) {
                val = 100;
            }
            adev->advol_level = val;

            AM_LOGI("advol set to %d", adev->advol_level);
            if ((eDolbyMS12Lib == adev->dolby_lib_type_last && ms12->dolby_ms12_enable) &&
                (path_id == dtv_audio_instances->demux_index_working)) {
                pthread_mutex_lock(&ms12->lock);
                set_ms12_ad_vol(ms12, adev->advol_level);
                pthread_mutex_unlock(&ms12->lock);
            }
            pthread_mutex_unlock(&adev->lock);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL:
            pthread_mutex_lock(&adev->lock);
            if (val < 0) {
                val = 0;
            } else if (val > 100) {
                val = 100;
            }
            adev->mixing_level = (val * 64 - 32 * 100) / 100; //[0,100] mapping to [-32,32]
            AM_LOGI("mixing_level set to %d", adev->mixing_level);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
                pthread_mutex_lock(&ms12->lock);
                dolby_ms12_set_user_control_value_for_mixing_main_and_associated_audio(adev->mixing_level);
                set_ms12_ad_mixing_level(ms12, adev->mixing_level);
                pthread_mutex_unlock(&ms12->lock);
            }
#if 0
            if (eDolbyDcvLib == adev->dolby_lib_type) {
                if (val > 0) {
                    adev->associate_audio_mixing_enable = 1;
                    demux_info->associate_audio_mixing_enable = adev->associate_audio_mixing_enable;
                }
                else {
                    adev->associate_audio_mixing_enable = 0;
                    demux_info->associate_audio_mixing_enable = adev->associate_audio_mixing_enable;
                }
            }
#endif
            pthread_mutex_unlock(&adev->lock);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_MEDIA_PRESENTATION_ID:
            demux_info->media_presentation_id = val;
            AM_LOGI("media_presentation_id %d",demux_info->media_presentation_id);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
                pthread_mutex_lock(&ms12->lock);
                set_ms12_ac4_presentation_group_index(ms12, demux_info->media_presentation_id);
                pthread_mutex_unlock(&ms12->lock);
            }
            break;
        case AUDIO_DTV_PATCH_CMD_SET_MEDIA_FIRST_LANG:
            demux_info->media_first_lang = val;
            char first_lang[4] = {0};
            dtv_convert_language_to_string(demux_info->media_first_lang,first_lang);
            AM_LOGI("media_first_lang %s,%x,%d",first_lang,val,val);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
                pthread_mutex_lock(&ms12->lock);
                set_ms12_ac4_1st_preferred_language_code(ms12, first_lang);
                pthread_mutex_unlock(&ms12->lock);
            }
            break;

        case AUDIO_DTV_PATCH_CMD_SET_MEDIA_SECOND_LANG:
            demux_info->media_second_lang = val;
            char second_lang[4] = {0};
            dtv_convert_language_to_string(demux_info->media_second_lang,second_lang);
            AM_LOGI("media_second_lang %s,%x,%d",second_lang, val, val);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
                pthread_mutex_lock(&ms12->lock);
                set_ms12_ac4_2nd_preferred_language_code(ms12, second_lang);
                pthread_mutex_unlock(&ms12->lock);
            }
            break;
        case AUDIO_DTV_PATCH_CMD_CONTROL:
            if (patch == NULL) {
                AM_LOGI("the audio patch is NULL");
                break;
            }
            if (val <= AUDIO_DTV_PATCH_CMD_NULL || val > AUDIO_DTV_PATCH_CMD_NUM) {
                AM_LOGW("Unsupported dtv patch cmd:%d", val);
                break;
            }
            AM_LOGI("Send dtv patch cmd:%s", dtvAudioPatchCmd2Str(val));
            if (val == AUDIO_DTV_PATCH_CMD_OPEN) {
                if (adev->is_multi_demux) {
                    Open_Dmx_Audio(&demux_handle,demux_info->demux_id, demux_info->security_mem_level);
                    AM_LOGI("path_id %d demux_hanle %p ",path_id, demux_handle);
                    dtv_audio_instances->demux_handle[path_id] = demux_handle;
                    Init_Dmx_Main_Audio(demux_handle, demux_info->main_fmt, demux_info->main_pid);
                    {
                        pthread_mutex_lock(&dtvsync->ms_lock);
                        dtvsync->mediasync_new = mediasync_wrap_create();
                        if (dtvsync->mediasync_new == NULL)
                            AM_LOGI("mediasync create failed");
                        else {
                            dtvsync->mediasync_id = demux_info->media_sync_id;
                            AM_LOGI("path_id:%d,dtvsync media_sync_id=%d", path_id, dtvsync->mediasync_id);
                            mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_ISOMXTUNNELMODE, &audio_sync_mode);
                            mediasync_wrap_bindInstance(dtvsync->mediasync_new, dtvsync->mediasync_id, MEDIA_AUDIO);
                            AM_LOGI("normal output version CMD open audio bind syncId:%d", dtvsync->mediasync_id);
                            mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_HASAUDIO, &has_audio);
                        }
                        pthread_mutex_unlock(&dtvsync->ms_lock);
                    }
                    AM_LOGI("create mediasync:%p", dtvsync->mediasync_new);
                } else {
                    int ret = uio_init_new(&patch->uio_fd);
                    //amstream driver first len is zero, set 4096 for default value
                    dtv_audio_instances->prelen = 4096;
                    if (ret < 0) {
                        AM_LOGI("uio init error! \n");
                        goto exit;
                    }

                    {
                        pthread_mutex_lock(&dtvsync->ms_lock);
                        dtvsync->mediasync_new = mediasync_wrap_create();
                        if (dtvsync->mediasync_new == NULL) {
                            AM_LOGI("mediasync create failed\n");
                        }
                        else {
                            dtvsync->mediasync_id = demux_info->media_sync_id;
                            AM_LOGI("path_id:%d,dtvsync media_sync_id=%d\n", path_id, dtvsync->mediasync_id);
                            mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_ISOMXTUNNELMODE, &audio_sync_mode);
                            mediasync_wrap_bindInstance(dtvsync->mediasync_new, dtvsync->mediasync_id, MEDIA_AUDIO);
                            AM_LOGI("normal output version CMD open audio bind syncId:%d\n", dtvsync->mediasync_id);
                            mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_HASAUDIO, &has_audio);
                        }
                        pthread_mutex_unlock(&dtvsync->ms_lock);
                    }
                }
            } else if (val == AUDIO_DTV_PATCH_CMD_CLOSE) {
                AM_LOGI("AUDIO_DTV_PATCH_CMD_CLOSE demux_hanle %p,%d,%p,%p\n",demux_handle,adev->is_multi_demux, dtvsync->mediasync_new, dtvsync->mediasync);

                //wait stop done, need wait input & output thread exit here for 40ms ease at stop cmd process
                int wait_count = 0;
                while ((0 != patch->output_thread_created) || (0 != patch->input_thread_created))
                {
                    aml_audio_sleep(10000); //10ms
                    wait_count++;
                    if (100 < wait_count)
                    {
                        AM_LOGI("1s timeout break!!!");
                        break;
                    }
                }

                if (adev->is_multi_demux) {
                    if (demux_handle) {
                        pthread_mutex_lock(&adev->dtv_lock);
                        Stop_Dmx_Main_Audio(demux_handle);
                        Destroy_Dmx_Main_Audio(demux_handle);

                        if (patch->ad_output_thread_created) {
                            Stop_Dmx_AD_Audio(demux_handle);
                            Destroy_Dmx_AD_Audio(demux_handle);
                        }
                        Close_Dmx_Audio(demux_handle);
                        demux_handle = NULL;
                        dtv_audio_instances->demux_handle[path_id] = NULL;
                        pthread_mutex_unlock(&adev->dtv_lock);

                        /*pthread_mutex_lock(&dtvsync->ms_lock);
                        if ((dtvsync->mediasync_new != NULL)) {
                            AM_LOGI("close mediasync_new:%p, mediasync:%p", dtvsync->mediasync_new, dtvsync->mediasync);
                            mediasync_wrap_destroy(dtvsync->mediasync_new);
                            dtvsync->mediasync_new = NULL;
                            dtvsync->mediasync = NULL;
                        }
                        pthread_mutex_unlock(&dtvsync->ms_lock);*/
                    }
                } else {
                    if (demux_handle) {
                        if (patch->ad_output_thread_created) {
                            pthread_mutex_lock(&adev->dtv_lock);
                            Stop_Dmx_AD_Audio(demux_handle);
                            Destroy_Dmx_AD_Audio(demux_handle);
                            Close_Dmx_Audio(demux_handle);
                            pthread_mutex_unlock(&adev->dtv_lock);
                        }
                        demux_handle = NULL;
                        dtv_audio_instances->demux_handle[path_id] = NULL;
                    }
                    uio_deinit_new(&patch->uio_fd);
                    /*pthread_mutex_lock(&dtvsync->ms_lock);
                    if ((dtvsync->mediasync_new != NULL)) {
                        AM_LOGI("close mediasync_new:%p, mediasync:%p", dtvsync->mediasync_new, dtvsync->mediasync);
                        mediasync_wrap_destroy(dtvsync->mediasync_new);
                        dtvsync->mediasync_new = NULL;
                        dtvsync->mediasync = NULL;
                    }
                    pthread_mutex_unlock(&dtvsync->ms_lock);*/
                }
                memset(demux_info, 0, sizeof(aml_demux_audiopara_t));
                demux_info->media_presentation_id  = -1;
                demux_info->media_first_lang  = -1;
                demux_info->media_second_lang  = -1;
            } else {
                if (patch == NULL) {
                    AM_LOGI("the audio patch is NULL");
                    break;
                }
                if (val <= AUDIO_DTV_PATCH_CMD_NULL || val > AUDIO_DTV_PATCH_CMD_STOP) {
                    ALOGW("Unsupported cmd :%d", val);
                    break;
                }
                pthread_mutex_lock(&patch->dtv_cmd_process_mutex);
                if (val == AUDIO_DTV_PATCH_CMD_START) {
                    dtv_audio_instances->demux_index_working = path_id;
                    patch->mode = demux_info->output_mode;
                    patch->dtv_aformat = demux_info->main_fmt;
                    patch->media_sync_id = demux_info->media_sync_id;
                    patch->pid = demux_info->main_pid;
                    patch->demux_info = demux_info;
                    patch->dtvsync = dtvsync;
                    if (dtvsync->mediasync_new != NULL) {
                        patch->dtvsync->mediasync = dtvsync->mediasync_new;
                        //dtvsync->mediasync_new = NULL;
                    }
                    patch->dtv_has_video = demux_info->has_video;
                    patch->demux_handle = dtv_audio_instances->demux_handle[path_id];
                    AM_LOGI("dtv_has_video %d",patch->dtv_has_video);
                    AM_LOGI("demux_index_working %d handle %p",dtv_audio_instances->demux_index_working, dtv_audio_instances->demux_handle[path_id]);
                }
                if (path_id == dtv_audio_instances->demux_index_working) {
                    dtv_patch_add_cmd(patch->dtv_cmd_list, val, path_id);
                    pthread_cond_signal(&patch->dtv_cmd_process_cond);
                } else {
                    AM_LOGI("path_id %d not work ,cmd %d invalid",path_id, val);
                }
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
            }
            break;
        default:
            AM_LOGI("invalid cmd %d", cmd);
    }

    return 0;
exit:

    AM_LOGI("dtv_patch_handle_event failed ");
    return -1;
}

int dtv_patch_get_latency(struct aml_audio_device *aml_dev)
{
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    if (patch == NULL ) {
        AM_LOGI("dtv patch == NULL");
        return -1;
    }
    int latencyms = 0;
    int64_t last_queue_es_apts = 0;
    if (aml_dev->is_multi_demux) {
        if (Get_Audio_LastES_Apts(patch->demux_handle, &last_queue_es_apts) == 0) {
             AM_LOGI("last_queue_es_apts %lld",last_queue_es_apts);
             patch->last_chenkin_apts = last_queue_es_apts;
        }
        latencyms = (patch->last_chenkin_apts - patch->dtvsync->cur_outapts) / 90;
    }
    return latencyms;
}

int dtv_get_demuxidbase() {
    return DVB_DEMUX_ID_BASE;
}

extern int do_output_standby_l(struct audio_stream *stream);
extern void adev_close_output_stream_new(struct audio_hw_device *dev,
        struct audio_stream_out *stream);
extern int adev_open_output_stream_new(struct audio_hw_device *dev,
                                       audio_io_handle_t handle __unused,
                                       audio_devices_t devices,
                                       audio_output_flags_t flags,
                                       struct audio_config *config,
                                       struct audio_stream_out **stream_out,
                                       const char *address __unused);
bool is_need_check_ad_substream(struct aml_audio_patch *patch) {
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    bool    is_need_check_ad_substream =  (eDolbyDcvLib == aml_dev->dolby_lib_type &&
                                           ( patch->aformat == AUDIO_FORMAT_E_AC3 ||
                                             patch->aformat == AUDIO_FORMAT_AC3 ) &&
                                           !patch->ad_substream_checked_flag);
    return is_need_check_ad_substream;

}

int patch_thread_get_cmd(struct aml_audio_patch *patch, int *cmd, int *path_id)
{
    if (patch->dtv_cmd_list->initd == 0) {
        return -1;
    }
    if (patch == NULL) {
        return -1;
    }

    if (dtv_patch_cmd_is_empty(patch->dtv_cmd_list) == 1) {
        return -1;
    } else {
        return dtv_patch_get_cmd(patch->dtv_cmd_list,cmd, path_id);
    }
}

int audio_set_spdif_clock(struct aml_stream_out *stream, int type)
{
    struct aml_audio_device *dev = stream->dev;
    bool is_dual_spdif = is_dual_output_stream((struct audio_stream_out *)stream);

    if (!dev || !dev->audio_patch) {
        AM_LOGE("dev or dev->audio_patch is NULL");
        return 0;
    }
    if (dev->patch_src != SRC_DTV || !dev->audio_patch->is_dtv_src) {
        AM_LOGE("[%s:%d] patch_src:%d, is_dtv_src:%d",
            __FUNCTION__, __LINE__,dev->patch_src, dev->audio_patch->is_dtv_src);
        return 0;
    }

    switch (type) {
    case AML_DOLBY_DIGITAL:
    case AML_DOLBY_DIGITAL_PLUS:
    case AML_DTS:
    case AML_DTS_HD:
        dev->audio_patch->spdif_format_set = 1;
        break;
    case AML_STEREO_PCM:
    default:
        dev->audio_patch->spdif_format_set = 0;
        break;
    }

    if (alsa_device_is_auge()) {
        if (dev->audio_patch->spdif_format_set) {
            if (stream->hal_internal_format == AUDIO_FORMAT_E_AC3 &&
                dev->bHDMIARCon && !is_dual_output_stream((struct audio_stream_out *)stream)) {
                dev->audio_patch->dtv_default_spdif_clock =
                    stream->config.rate * 128 * 4;
            } else {
                dev->audio_patch->dtv_default_spdif_clock =
                    stream->config.rate * 128;
            }
        } else {
            dev->audio_patch->dtv_default_spdif_clock =
                DEFAULT_I2S_OUTPUT_CLOCK / 2;
        }
    } else {
        if (dev->audio_patch->spdif_format_set) {
            dev->audio_patch->dtv_default_spdif_clock =
                stream->config.rate * 128 * 4;
        } else {
            dev->audio_patch->dtv_default_spdif_clock =
                DEFAULT_I2S_OUTPUT_CLOCK;
        }
    }

    dev->audio_patch->dtv_default_arc_clock = DEFAULT_EARC_OUTPUT_CLOCK;

    dev->audio_patch->spdif_step_clk =
        dev->audio_patch->dtv_default_spdif_clock / (aml_audio_property_get_int(
                                        PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS, DEFAULT_TUNING_PCR_CLOCK_STEPS));
    dev->audio_patch->i2s_step_clk =
        DEFAULT_I2S_OUTPUT_CLOCK / (aml_audio_property_get_int(
                                        PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS, DEFAULT_TUNING_PCR_CLOCK_STEPS));
    dev->audio_patch->arc_step_clk =
        dev->audio_patch->dtv_default_arc_clock / (aml_audio_property_get_int(
                                        PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS, DEFAULT_TUNING_PCR_CLOCK_STEPS));
    AM_LOGI("type=%d,spdif %d,dual %d, arc %d(%d), spdif_step_clk %d, i2s_step_clk %d, arc_step_clk %d",
        type, dev->audio_patch->spdif_step_clk, is_dual_spdif, dev->bHDMIARCon,
        aml_audio_earctx_get_type(dev), dev->audio_patch->spdif_step_clk, dev->audio_patch->i2s_step_clk, dev->audio_patch->arc_step_clk);
    dtv_adjust_output_clock(dev->audio_patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, is_dual_spdif);
    return 0;
}

static bool check_ad_enable(struct aml_audio_patch *patch)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    if (demux_info && VALID_PID(demux_info->ad_pid) && aml_dev->associate_audio_mixing_enable) {
        if (aml_dev->dolby_lib_type == eDolbyDcvLib) {
             if (patch->aformat == AUDIO_FORMAT_AC3 || patch->aformat == AUDIO_FORMAT_E_AC3 )  {
                audio_format_t output_format = get_non_ms12_output_format(patch->aformat, aml_dev);
                if (output_format == AUDIO_FORMAT_AC3 || output_format == AUDIO_FORMAT_E_AC3) {
                    return false;
                }
            }
        } else if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
            if (aml_dev->hdmi_format == BYPASS) {
                return false;
            }
        }
        return true;
    } else {
       return false;
    }
}

typedef struct audiopacketsinfo{
    int packetsSize;
    int duration;
    int isworkingchannel;
    int isneedupdate;
    int64_t packetsPts;
} driver_audio_packets_info;

int GetInputSizeandpts(aml_dtv_audio_instances_t *dtv_audio_instances,int64_t* pts,struct aml_audio_patch *patch)
{
    int nNextReadSize = 4096;
    driver_audio_packets_info driverpackage = {0};
    int retlen = 0;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    do
    {
        retlen=read(dtv_audio_instances->paudiofd, &driverpackage, sizeof(driver_audio_packets_info));
        * pts = driverpackage.packetsPts;
        if (aml_dev->debug_flag > 0)
            AM_LOGI("paudiofd:%d,%d,%llx \n",retlen,driverpackage.packetsSize,driverpackage.packetsPts);
        usleep(2000);
    }
    while (retlen != sizeof(driver_audio_packets_info)  && !patch->input_thread_exit);

    if (0 < driverpackage.packetsSize  &&  driverpackage.packetsSize < 6400)
    {
        nNextReadSize = driverpackage.packetsSize;
        dtv_audio_instances->prelen = driverpackage.packetsSize;
    }
    else
    {
        nNextReadSize = dtv_audio_instances->prelen;
    }
    return nNextReadSize;
}


void *audio_dtv_patch_input_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    package_list *list = patch->dtv_package_list;
    package_list *ad_list = patch->dtv_ad_package_list;
    void *demux_handle = patch->demux_handle;
    int ret = 0;
    AM_LOGI("++create a input stream success now!!!");
    int nNextFrameSize = 0; //next read frame size
    int inlen = 0;//real data size in in_buf
    int nInBufferSize = 0; //full buffer size
    char *inbuf = NULL;//real buffer
    char *ad_buffer = NULL;
    struct package *dtv_package = NULL;
    struct package *dtv_ad_package = NULL;
    struct mAudioEsDataInfo *mEsData = NULL ,*mAdEsData = NULL;
    int trycount = 0;
    int max_trycount = 2;
    int rlen = 0;//read buffer ret size
    struct mediasync_audio_queue_info audio_queue_info;
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    aml_dtvsync_t *Dtvsync = NULL ;
    aml_demux_audiopara_t *demux_info = NULL;
    int path_index = 0, working_path_index = 0;
    AM_LOGI("patch->input_thread_exit %d!!!", patch->input_thread_exit);

    prctl(PR_SET_NAME, (unsigned long)"dtv_input_data");
    dtv_package_list_init(patch->dtv_package_list);
    dtv_package_list_init(ad_list);
    AM_LOGI("init ad_list");
    while (!patch->input_thread_exit) {
        int nRet = 0;
        bool data_dump = aml_audio_property_get_bool("vendor.media.audiohal.patch.in", false);
        if (!aml_dev->is_multi_demux) {
            demux_handle = dtv_audio_instances->demux_handle[path_index];
            demux_info = &dtv_audio_instances->demux_info[path_index];
            if (dtv_package == NULL) {
                dtv_package = aml_audio_malloc(sizeof(struct package));
                memset(dtv_package,0,sizeof(struct package));
                if (!dtv_package) {
                    AM_LOGI("dtv_package malloc failed ");
                    pthread_mutex_unlock(&patch->mutex);
                    goto exit;
                }
            }
            if (inbuf != NULL) {
                aml_audio_free(inbuf);
                inbuf = NULL;
            }
            nInBufferSize = GetInputSizeandpts(dtv_audio_instances,&(dtv_package->pts),patch);
            inbuf = aml_audio_malloc(nInBufferSize);
            if (!inbuf) {
                AM_LOGE("inbuf malloc failed");
                pthread_mutex_unlock(&patch->mutex);
                goto exit;
            }

            int nNextReadSize = nInBufferSize;
            rlen = 0;
            while (nNextReadSize > 0 && !patch->input_thread_exit) {
                nRet = uio_read_buffer((unsigned char *)(inbuf + rlen), nNextReadSize, patch->input_thread_exit);
                if (aml_dev->debug_flag > 0)
                    AM_LOGI("uio_read_buffer nRet:%d \n",nRet);
                if (nRet <= 0) {
                    trycount++;
                    usleep(20000);
                    continue;
                }
                else
                {
                    rlen += nRet;
                    nNextReadSize -= nRet;
                    break ;
                }
            }
            trycount = 0;
            dtv_package->size = rlen;
            dtv_package->data = (char *)inbuf;

            if (check_ad_enable(patch) && (rlen > 4)) {
                int getadcount = 0;
                do {
                    nRet=Get_ADAudio_Es(demux_handle, &mAdEsData);
                    usleep(10000);
                    getadcount++;
                    if (getadcount > 2)
                    {
                    break;
                    }
                } while (nRet != AM_AUDIO_Dmx_SUCCESS && !patch->input_thread_exit);

                if (nRet == AM_AUDIO_Dmx_SUCCESS) {
                    dtv_ad_package->size = mAdEsData->size;
                    dtv_ad_package->data = (char *)mAdEsData->data;
                    demux_info->ad_pan  = mAdEsData->adpan;
                    demux_info->ad_fade = mAdEsData->adfade;
                    aml_audio_free(mAdEsData);
                    mAdEsData = NULL;
                } else {
                    dtv_ad_package->size = 0;
                    dtv_ad_package->data = NULL;
                }
            }
            if (data_dump) {
                aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/dtv_ad_audio_dmx.es", dtv_ad_package->data, dtv_ad_package->size);
                aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/dtv_main_audio_dmx.es", dtv_package->data, dtv_package->size);
            }

            /* add dtv package to package list */
            while (!patch->input_thread_exit && dtv_package) {
                pthread_mutex_lock(&patch->mutex);
                ret = dtv_package_add(list, dtv_package);
                if (ret == 0) {
                    if (aml_dev->debug_flag > 0) {
                        AM_LOGI("pthread_cond_signal dtv_package %p ", dtv_package);
                    }
                    pthread_cond_signal(&patch->cond);
                    pthread_mutex_unlock(&patch->mutex);
                    inbuf = NULL;
                    dtv_package = NULL;
                    demux_info->dtv_package = NULL;
                    break;
                } else {
                    AM_LOGI("list->pack_num %d full !!!", list->pack_num);
                    pthread_mutex_unlock(&patch->mutex);
                    usleep(150000);
                    continue;
                }
            }
              /* add dtv package to package list */
            while (!patch->input_thread_exit && dtv_ad_package) {
                pthread_mutex_lock(&patch->assoc_mutex);
                ret = dtv_package_add(ad_list, dtv_ad_package);
                if (ret == 0) {
                    if (aml_dev->debug_flag > 0) {
                        AM_LOGI("pthread_cond_signal dtv_package %p", dtv_ad_package);
                    }
                    pthread_cond_signal(&patch->ad_cond);
                    pthread_mutex_unlock(&patch->assoc_mutex);
                    dtv_ad_package = NULL;
                    demux_info->dtv_ad_package = NULL;
                    break;
                } else {
                    AM_LOGI("list->pack_num %d full !!!", ad_list->pack_num);
                    pthread_mutex_unlock(&patch->assoc_mutex);
                    usleep(150000);
                    continue;
                }
            }
        } else {
            path_index = 0;
            for (; path_index < DVB_DEMUX_SUPPORT_MAX_NUM; path_index++ ) {

                if (patch->input_thread_exit) {
                    break;
                }
                pthread_mutex_lock(&aml_dev->dtv_lock);
                demux_handle = dtv_audio_instances->demux_handle[path_index];
                demux_info = &dtv_audio_instances->demux_info[path_index];
                Dtvsync = &dtv_audio_instances->dtvsync[path_index];
                if (Dtvsync->mediasync_new != NULL && Dtvsync->mediasync != Dtvsync->mediasync_new) {
                    Dtvsync->mediasync = Dtvsync->mediasync_new;
                    //Dtvsync->mediasync_new = NULL;
                }

                dtv_package = demux_info->dtv_package;
                dtv_ad_package = demux_info->dtv_ad_package;
                mEsData = demux_info->mEsData;
                mAdEsData = demux_info->mADEsData;

                if (demux_handle == NULL) {
                    if (dtv_package) {
                       if (dtv_package->data) {
                           aml_audio_free(dtv_package->data);
                           dtv_package->data = NULL;
                       }
                       aml_audio_free(dtv_package);
                       dtv_package = NULL;
                       demux_info->dtv_package = NULL;
                    }
                    if (dtv_ad_package) {
                        if (dtv_ad_package->data) {
                            aml_audio_free(dtv_ad_package->data);
                            dtv_ad_package->data = NULL;
                        }
                        aml_audio_free(dtv_ad_package);
                        dtv_ad_package = NULL;
                        demux_info->dtv_ad_package = NULL;
                    }

                    demux_info->mEsData =  NULL;
                    demux_info->mADEsData = NULL;
                    pthread_mutex_unlock(&aml_dev->dtv_lock);
                    continue;
                } else {
                    if (dtv_package == NULL) {
                        dtv_package = aml_audio_calloc(1, sizeof(struct package));
                        if (!dtv_package) {
                            AM_LOGI("dtv_package malloc failed ");
                            goto exit;
                        } else
                            AM_LOGI_IF(aml_dev->debug_flag, "input, main package %p alloc", dtv_package);
                        memset(dtv_package, 0, sizeof(struct package));
                        if (dtv_ad_package == NULL && check_ad_enable(patch)) {
                            dtv_ad_package = aml_audio_calloc(1, sizeof(struct package));
                            if (!dtv_ad_package) {
                                AM_LOGI("dtv_package malloc failed ");
                                goto exit;
                            } else
                                AM_LOGI_IF(aml_dev->debug_flag, "input, ad package %p alloc", dtv_ad_package);
                        }

                        /* get dtv main and ad pakcage */
                        struct timespec current_ts,end_ts;
                        clock_gettime(CLOCK_MONOTONIC, &current_ts);
                        while (!patch->input_thread_exit) {
                            /* get main data */
                            if (mEsData == NULL) {
                                nRet = Get_MainAudio_Es(demux_handle,&mEsData);
                                if (nRet != AM_AUDIO_Dmx_SUCCESS) {
                                    if (path_index == dtv_audio_instances->demux_index_working) {
                                        trycount++;
                                        AM_LOGI_IF(aml_dev->debug_flag, "main trycount %d, %p", trycount, mEsData);
                                        if (mEsData) {
                                            patch->pts_dts_flag = mEsData->pts_dts_flag;
                                        }
                                        if (trycount > 4 ) {
                                            trycount = 0;
                                            break;
                                        } else {
                                           usleep(5000);
                                           continue;
                                        }
                                    } else {
                                        trycount = 0;
                                        break;
                                    }
                                } else {
                                   trycount = 0;
                                   AM_LOGI_IF(aml_dev->debug_flag, "main size %d, pts %0llx(%llx), pts flag %d", mEsData->size, mEsData->pts, patch->last_valid_main_pts, mEsData->pts_dts_flag);
                                   patch->pts_dts_flag = mEsData->pts_dts_flag;
                                   if (NULL_INT64 != mEsData->pts) {
                                        patch->last_valid_main_pts = mEsData->pts;
                                   } else {
                                        mEsData->pts = patch->last_valid_main_pts;
                                   }
                                   if (data_dump) {
                                       aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/dtv_main_audio_dmx.es", mEsData->data, mEsData->size);
                                   }
                                }
                            }

                            /* get ad data */
                            if (mAdEsData == NULL) {
                                if (check_ad_enable(patch)) {
                                    nRet = Get_ADAudio_Es(demux_handle, &mAdEsData);
                                    if (nRet != AM_AUDIO_Dmx_SUCCESS) {
                                        AM_LOGE("Get_ADAudio_Es failed");
                                        clock_gettime(CLOCK_MONOTONIC, &end_ts);
                                        int  data_get_cost_ms = calc_time_interval_us(&current_ts, &end_ts) / 1000;
                                        AM_LOGI_IF(aml_dev->debug_flag, "data_get_cost_ms %d ms, %d %d", data_get_cost_ms, dtv_package_is_empty(list), dtv_package_is_empty(ad_list));
                                        if (!dtv_package_is_empty(list) && data_get_cost_ms < 1000) {
                                            usleep(5000);
                                            continue;
                                        }
                                    }

                                    if (mAdEsData == NULL) {
                                        AM_LOGI("do not get ad es data trycount %d", trycount);
                                        demux_info->ad_package_status = AD_PACK_STATUS_HOLD;
                                        break;
                                    } else {
                                        AM_LOGI_IF(aml_dev->debug_flag, "ad size %d pts %0llx(%llx)", mAdEsData->size, mAdEsData->pts, patch->last_valid_ad_pts);
                                        if (NULL_INT64 != mAdEsData->pts) {
                                             patch->last_valid_ad_pts = mAdEsData->pts;
                                        } else {
                                             mAdEsData->pts = patch->last_valid_ad_pts;
                                        }
                                        if (data_dump) {
                                            aml_audio_dump_audio_bitstreams("/data/vendor/audiohal/dtv_ad_audio_dmx.es", mAdEsData->data, mAdEsData->size);
                                        }
                                        /* align ad and main data by pts compare */
                                        demux_info->ad_package_status = check_ad_package_status(mEsData->pts, mAdEsData->pts, demux_info);
                                        if (demux_info->ad_package_status == AD_PACK_STATUS_DROP) {
                                            if (mAdEsData->data) {
                                                aml_audio_free(mAdEsData->data);
                                                mAdEsData->data = NULL;
                                            }
                                            aml_audio_free(mAdEsData);
                                            mAdEsData = NULL;
                                            trycount = 0;
                                            demux_info->mADEsData = NULL;
                                            AM_LOGI("drop ad data");
                                            continue;
                                        } else if (demux_info->ad_package_status == AD_PACK_STATUS_HOLD) {
                                            demux_info->mADEsData = mAdEsData;
                                            AM_LOGI("hold ad data to wait main data ");
                                        }
                                        break;
                                    }
                                } else {
                                    break;
                                }

                            } else {
                                break;
                            }
                        }

                         /* dtv pack main ad and data data */
                        {
                            if (mEsData) {
                                dtv_package->size = mEsData->size;
                                dtv_package->data = (char *)mEsData->data;
                                dtv_package->pts = mEsData->pts;
                                aml_audio_free(mEsData);
                                mEsData = NULL;
                                demux_info->mEsData = NULL;
                                demux_info->dtv_package = dtv_package;
                            } else {
                                AM_LOGI_IF(aml_dev->debug_flag, " do not get mEsData");
                                aml_audio_free(dtv_package);
                                dtv_package = NULL;
                                demux_info->dtv_package = NULL;
                                demux_info->mEsData = NULL;
                                pthread_mutex_unlock(&aml_dev->dtv_lock);
                                usleep(10000);
                                continue;
                            }

                            if (mAdEsData && dtv_ad_package) {
                                demux_info->ad_package_status = check_ad_package_status(dtv_package->pts, mAdEsData->pts, demux_info);
                                if (demux_info->ad_package_status == AD_PACK_STATUS_HOLD) {
                                    aml_audio_free(dtv_ad_package);
                                    dtv_ad_package = NULL;
                                    demux_info->dtv_ad_package = NULL;
                                    AM_LOGI("%d, drop ad data", __LINE__);
                                } else {
                                    dtv_ad_package->size = mAdEsData->size;
                                    dtv_ad_package->data = (char *)mAdEsData->data;
                                    dtv_ad_package->pts = mAdEsData->pts;
                                    dtv_ad_package->ad_pan  = mAdEsData->adpan;
                                    dtv_ad_package->ad_fade = mAdEsData->adfade;
                                    aml_audio_free(mAdEsData);
                                    mAdEsData = NULL;
                                }
                                demux_info->mADEsData = mAdEsData;
                            } else if (dtv_ad_package) {
                                aml_audio_free(dtv_ad_package);
                                dtv_ad_package = NULL;
                                demux_info->mADEsData = NULL;
                                demux_info->dtv_ad_package = NULL;
                            }
                        }

                    }

                     /* mediasyn check dmx package */
                     {
                        audio_queue_info.apts = dtv_package->pts;
                        audio_queue_info.duration = Dtvsync->duration;
                        audio_queue_info.size = dtv_package->size;
                        audio_queue_info.isneedupdate = false;

                        if (path_index == dtv_audio_instances->demux_index_working)
                            audio_queue_info.isworkingchannel = true;
                        else
                            audio_queue_info.isworkingchannel = false;
                        audio_queue_info.tunit = MEDIASYNC_UNIT_PTS;
                        mediasync_wrap_queueAudioFrame(Dtvsync->mediasync, &audio_queue_info);
                        AM_LOGI_IF(aml_dev->debug_flag, "queue pts:%llx, size:%d, dur:%d isneedupdate %d, ret %d\n",
                                 dtv_package->pts, dtv_package->size, Dtvsync->duration,audio_queue_info.isneedupdate, nRet);
                        if (path_index != dtv_audio_instances->demux_index_working) {
                            AM_LOGI_IF(aml_dev->debug_flag, "path_index  %d dtv_audio_instances->demux_index_working %d",
                                path_index, dtv_audio_instances->demux_index_working);
                            if (audio_queue_info.isneedupdate) {
                                if (dtv_package) {
                                   if (dtv_package->data) {
                                       aml_audio_free(dtv_package->data);
                                       dtv_package->data = NULL;
                                   }
                                   aml_audio_free(dtv_package);
                                   dtv_package = NULL;
                                }
                                if (dtv_ad_package) {
                                   if (dtv_ad_package->data) {
                                       aml_audio_free(dtv_ad_package->data);
                                       dtv_ad_package->data = NULL;
                                   }
                                   aml_audio_free(dtv_ad_package);
                                   dtv_ad_package = NULL;
                                }
                            }

                            demux_info->dtv_package = dtv_package;
                            demux_info->dtv_ad_package = dtv_ad_package;
                            pthread_mutex_unlock(&aml_dev->dtv_lock);
                            if (!audio_queue_info.isneedupdate &&
                                dtv_audio_instances->demux_index_working == -1) {
                                usleep(5000);
                            }
                            continue;
                        }
                    }

                    /* add dtv package to package list */
                    while (!patch->input_thread_exit &&
                        path_index == dtv_audio_instances->demux_index_working && dtv_package && list) {
                        pthread_mutex_lock(&patch->mutex);
                        ret = dtv_package_add(list, dtv_package);
                        if (ret == 0) {
                            AM_LOGI_IF(aml_dev->debug_flag, "pthread_cond_signal dtv_package %p, pts:%llx, size:%d. num %d", dtv_package, dtv_package->pts, dtv_package->size, list->pack_num);
                            pthread_cond_signal(&patch->cond);
                            pthread_mutex_unlock(&patch->mutex);
                            dtv_package = NULL;
                            demux_info->dtv_package = NULL;
                            break;
                        } else {
                            AM_LOGI("list->pack_num %d full !!!", list->pack_num);
                            pthread_mutex_unlock(&patch->mutex);
                            pthread_mutex_unlock(&aml_dev->dtv_lock);
                            usleep(150000);
                            pthread_mutex_lock(&aml_dev->dtv_lock);
                            continue;
                        }
                    }
                    /* add dtv package to package list */
                    while (!patch->input_thread_exit &&
                        path_index == dtv_audio_instances->demux_index_working && check_ad_enable(patch) &&
                        dtv_ad_package && ad_list) {
                        pthread_mutex_lock(&patch->assoc_mutex);
                        ret = dtv_package_add(ad_list, dtv_ad_package);
                        if (ret == 0) {
                            AM_LOGI_IF(aml_dev->debug_flag, "pthread_cond_signal dtv_ad_package %p, pts:%llx, size:%d. num %d", dtv_ad_package, dtv_ad_package->pts, dtv_ad_package->size, ad_list->pack_num);
                            pthread_cond_signal(&patch->ad_cond);
                            pthread_mutex_unlock(&patch->assoc_mutex);
                            dtv_ad_package = NULL;
                            demux_info->dtv_ad_package = NULL;
                            break;
                        } else {
                            AM_LOGI("list->pack_num %d full !!!", ad_list->pack_num);
                            pthread_mutex_unlock(&patch->assoc_mutex);
                            pthread_mutex_unlock(&aml_dev->dtv_lock);
                            usleep(150000);
                            pthread_mutex_lock(&aml_dev->dtv_lock);
                            continue;
                        }
                    }
                }
                pthread_mutex_unlock(&aml_dev->dtv_lock);
                usleep(5000);
            }
        }
    }
exit:

    if (inbuf) {
        aml_audio_free(inbuf);
        dtv_package->data=NULL;
    }
    if (ad_buffer != NULL) {
        aml_audio_free(ad_buffer);
        ad_buffer = NULL;
    }
    if (!aml_dev->is_multi_demux ) {
        if (dtv_package) {
           if (dtv_package->data) {
               aml_audio_free(dtv_package->data);
               dtv_package->data = NULL;
           }
           aml_audio_free(dtv_package);
           dtv_package = NULL;
        }
        if (dtv_ad_package) {
           if (dtv_ad_package->data) {
               aml_audio_free(dtv_ad_package->data);
               dtv_ad_package->data = NULL;
           }
           aml_audio_free(dtv_ad_package);
           dtv_ad_package = NULL;
        }
    } else {
        for (path_index = 0; path_index < DVB_DEMUX_SUPPORT_MAX_NUM; path_index++ ) {

            pthread_mutex_lock(&aml_dev->dtv_lock);
            demux_info = &dtv_audio_instances->demux_info[path_index];
            dtv_package = demux_info->dtv_package;

            if (dtv_package) {
               if (dtv_package->data) {
                   aml_audio_free(dtv_package->data);
                   dtv_package->data = NULL;
               }
               aml_audio_free(dtv_package);
               dtv_package = NULL;
            }
            if (dtv_ad_package) {
               if (dtv_ad_package->data) {
                   aml_audio_free(dtv_ad_package->data);
                   dtv_ad_package->data = NULL;
               }
               aml_audio_free(dtv_ad_package);
               dtv_ad_package = NULL;
            }
            demux_info->dtv_package = NULL;
            demux_info->dtv_ad_package = NULL;
            pthread_mutex_unlock(&aml_dev->dtv_lock);
        }
    }

    dtv_package_list_flush(list);
    if (ad_list)
        dtv_package_list_flush(ad_list);
    patch->pts_dts_flag = 0;

    AM_LOGI("--patch->input_thread_exit %d ", patch->input_thread_exit);
    return ((void *)0);
}


float aml_audio_get_output_speed(struct aml_stream_out *aml_out)
{
    float speed = aml_out->output_speed;
    speed = aml_audio_property_get_float("vendor.media.audio.output.speed", speed);
    if (fabs(speed - aml_out->output_speed) > 1e-6)
        AM_LOGI("prop set speed change from %f to %f\n",
            aml_out->output_speed, speed);
    return speed;
}

void *audio_dtv_patch_output_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    package_list *list = patch->dtv_package_list;
    struct audio_stream_out *stream_out = NULL;
    struct aml_stream_out *aml_out = NULL;
    struct audio_config stream_config;
    int ret= 0;
    int apts_diff = 0;
    struct timespec ts;

    AM_LOGI("created.");
    stream_config.sample_rate = 48000;
    stream_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    stream_config.format = patch->aformat;
    /*
    may we just exit from a direct active stream playback
    still here.we need remove to standby to new playback
    */
    pthread_mutex_lock(&aml_dev->lock);
    aml_out = direct_active(aml_dev);
    if (aml_out) {
        AM_LOGI("stream %p active,need standby aml_out->usecase:%d ",
              aml_out, aml_out->usecase);
        pthread_mutex_unlock(&aml_dev->lock);
        /*
        there are several output cases. if there are no ms12 or submixing modules.
        we will output raw/lpcm directly.we need close device directly.
        we need call standy function to release the direct stream
        */
        aml_out->stream.common.standby((struct audio_stream *)aml_out);
        pthread_mutex_lock(&aml_dev->lock);
        if (aml_dev->need_remove_conti_mode == true) {
            AM_LOGI("conntinous mode still there,release ms12 here");
            aml_dev->need_remove_conti_mode = false;
            aml_dev->continuous_audio_mode = 0;
        }
    } else {
        AM_LOGI("cant get the aml_out now!!!");
    }
    aml_dev->mix_init_flag = false;
    pthread_mutex_unlock(&aml_dev->lock);
    if (aml_dev->is_TV) {
        patch->output_src = AUDIO_DEVICE_OUT_SPEAKER;
    } else {
        patch->output_src = AUDIO_DEVICE_OUT_AUX_DIGITAL;
    }

    if (aml_dev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
        patch->output_src = aml_dev->out_device;

    ret = adev_open_output_stream_new(patch->dev, 0,
                                      patch->output_src,        // devices_t
                                      AUDIO_OUTPUT_FLAG_DIRECT | AUDIO_OUTPUT_FLAG_HW_AV_SYNC, // flags
                                      &stream_config, &stream_out, "AML_DTV_SOURCE");
    if (ret < 0) {
        AM_LOGE("open output stream fail, ret = %d", ret);
        goto exit_open;
    }

    aml_out = (struct aml_stream_out *)stream_out;
    patch->dtv_aml_out = aml_out;

    if (aml_out->avsync_ctx)
    {
        aml_out->avsync_ctx->mediasync_ctx = mediasync_ctx_init();
        if (NULL == aml_out->avsync_ctx->mediasync_ctx) {
            goto exit_open;
        }
        aml_out->avsync_type = AVSYNC_TYPE_MEDIASYNC;
        pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
        aml_out->avsync_ctx->mediasync_ctx->handle       = patch->dtvsync->mediasync;
        aml_out->avsync_ctx->mediasync_ctx->mediasync_id = patch->dtvsync->mediasync_id;
        aml_out->avsync_ctx->get_tuning_latency          = dtv_avsync_get_apts_latency;
        pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
    }
    else
    {
        AM_LOGE("aml_out->avsync_ctx NULL error!");
        goto exit_open;
    }

    AM_LOGI("create a output stream(%p) success now, output_thread_exit %d", aml_out, patch->output_thread_exit);
    //patch->dtv_audio_mode = get_dtv_audio_mode();
    patch->dtv_audio_tune = AUDIO_FREE;
    patch->first_apts_lookup_over = 0;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;

    prctl(PR_SET_NAME, (unsigned long)"dtv_output_data");
    aml_out->output_speed = 1.0f;
    //aml_out->dtvsync_enable = aml_audio_property_get_bool(DTV_SYNCENABLE, true);
    while (!patch->output_thread_exit) {

        if (patch->dtv_decoder_state == AUDIO_DTV_PATCH_DECODER_STATE_PAUSE) {
            usleep(1000);
            continue;
        }

        pthread_mutex_lock(&patch->mutex);

        struct package *p_package = NULL;
        p_package = dtv_package_get(list);
        if (!p_package) {
            ts_wait_time(&ts, 100000);
            pthread_cond_timedwait(&patch->cond, &patch->mutex, &ts);
            //pthread_cond_wait(&patch->cond, &patch->mutex);
            pthread_mutex_unlock(&patch->mutex);
            continue;
        } else {
            patch->cur_package = p_package;
            AM_LOGI_IF(aml_dev->debug_flag, "package(%p) pts:%llx, size:%d, first_apts_flag:%d, num %d", p_package, p_package->pts, p_package->size,
                      patch->dtv_first_apts_flag, list->pack_num);
            /* if first package pts is invalid, we need drop it */
            if ((!patch->dtv_first_apts_flag) && (NULL_INT64 == p_package->pts))
            {
                if (p_package->data) {
                    aml_audio_free(p_package->data);
                    p_package->data = NULL;
                }
                aml_audio_free(p_package);
                p_package = NULL;
                pthread_mutex_unlock(&patch->mutex);
                continue;
            }
            patch->dtv_first_apts_flag = 1;
        }

        if ((aml_out->avsync_ctx) && (aml_out->avsync_ctx->mediasync_ctx)) {
            pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
            aml_out->avsync_ctx->mediasync_ctx->in_apts = patch->cur_package->pts;
            pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
        }

        aml_out->output_speed = aml_audio_get_output_speed(aml_out);
        pthread_mutex_unlock(&patch->mutex);
        pthread_mutex_lock(&(patch->dtv_output_mutex));

        aml_out->codec_type = get_codec_type(patch->aformat);
        if (aml_out->hal_internal_format != patch->aformat) {
            aml_out->hal_format = aml_out->hal_internal_format = patch->aformat;
            get_sink_format(stream_out);
        }

        if ((!p_package->data || !p_package->size)) {
            AM_LOGI("cur_package data invalid");
            goto free_package;
        }

        ret = stream_out->write(stream_out, p_package->data, p_package->size);
free_package:
        if (patch->cur_package) {
            if (patch->cur_package->data) {
                aml_audio_free(patch->cur_package->data);
                patch->cur_package->data = NULL;
            }
            aml_audio_free(patch->cur_package);
            patch->cur_package = NULL;
        }

        pthread_mutex_unlock(&(patch->dtv_output_mutex));
    }
    aml_audio_free(patch->out_buf);
exit_outbuf:
    AM_LOGI("patch->output_thread_exit %d", patch->output_thread_exit);
    //set_dolby_ms12_runtime_sync(&(aml_dev->ms12), 0);//set normal output policy to ms12
    if (aml_out) {
        adev_close_output_stream_new(dev, stream_out);
    }
    patch->dtv_aml_out = NULL;
exit_open:
    if (aml_dev->audio_ease) {
        aml_dev->patch_start = false;
    }
    AM_LOGI("--");
    return ((void *)0);
}

#define QUEUE_AD_DATA_WHEN_START 1
void *audio_dtv_patch_ad_output_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    package_list *ad_list = patch->dtv_ad_package_list;
    package_list *main_list = patch->dtv_package_list;
    struct audio_stream_out *ad_stream_out = NULL;
    struct aml_stream_out *ad_aml_out = NULL;
    struct audio_config stream_config;
    int ret= 0, ret_ad = 0;
    int apts_diff = 0;
    struct timespec ts, watch_start_ts;
    bool watch_flag = false, need_hold_data = false;

    AM_LOGI("++ created.");
    stream_config.sample_rate = 48000;
    stream_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    stream_config.format = patch->aformat;

    ret = adev_open_output_stream_new(patch->dev, 0,
                                      patch->output_src,        // devices_t
                                      AUDIO_OUTPUT_FLAG_DIRECT + AUDIO_OUTPUT_FLAG_AD_STREAM, // flags
                                      &stream_config, &ad_stream_out, "AML_DTV_SOURCE");
    if (ret < 0) {
        AM_LOGE("live open ad output stream fail, ret = %d", ret);
        goto exit_open;
    }
    ad_aml_out = (struct aml_stream_out *)ad_stream_out;
    int associate_audio_mixing_enable = -1;

    AM_LOGI("create a AD output stream(%p) success now, ad_output_thread_exit %d", ad_aml_out, patch->ad_output_thread_exit);

    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    prctl(PR_SET_NAME, (unsigned long)"dtv_ad_output_data");

    while (!patch->ad_output_thread_exit) {
        if (patch->dtv_decoder_state == AUDIO_DTV_PATCH_DECODER_STATE_PAUSE) {
            usleep(20000);
            continue;
        }
#if QUEUE_AD_DATA_WHEN_START
        /* as for DD_Disappearing-AA_ddp_DVB_h264_29fps.trp: first 10s has ad,
          medium 10s has no ad data, then last 10s has ad again.
          from no_ad to has_ad, need to use start threshold to queue some data,
          to make sure ad has data continuously with main in mixer process*/
        if (ad_list->pack_num == 0 && !watch_flag) {//1.save the ts when there's no ad data.
            clock_gettime(CLOCK_MONOTONIC, &watch_start_ts);
            watch_flag = true;
        }
        AM_LOGI_IF(aml_dev->debug_flag, "ad_num %d, main_num %d, watch_flag %d", ad_list->pack_num, main_list->pack_num, watch_flag);
        if (watch_flag && ad_list->pack_num != 0) {
            struct timespec cur_ts;
            clock_gettime(CLOCK_MONOTONIC, &cur_ts);
            int has_data_cost_ms = calc_time_interval_us(&watch_start_ts, &cur_ts) / 1000;
            if (has_data_cost_ms > 1000) {//2.if the period time from no_data to has_data > 1s, need queue
                need_hold_data = true;
            }
            watch_flag = false;
        }
        AM_LOGI_IF(aml_dev->debug_flag, "ad_num %d, main_num %d, watch_flag %d, need_hold_data %d", ad_list->pack_num, main_list->pack_num, watch_flag, need_hold_data);

        /*start threshold to make ad has enough data,
          avoid no ad when mix main data in ms12.
        */
        #define AD_START_THRESHOLD 2
        if (ad_list->pack_num > AD_START_THRESHOLD && need_hold_data) {
            need_hold_data = false;
        } else if (need_hold_data) {
            usleep(10000);
            continue;
        }
#endif

        if (associate_audio_mixing_enable != aml_dev->associate_audio_mixing_enable) {
            associate_audio_mixing_enable = aml_dev->associate_audio_mixing_enable;
            if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
                set_ms12_ad_mixing_enable(&(aml_dev->ms12), aml_dev->associate_audio_mixing_enable);
                set_ms12_ad_mixing_level(&(aml_dev->ms12), aml_dev->mixing_level);
                set_ms12_ad_vol(&(aml_dev->ms12), aml_dev->advol_level);
                AM_LOGI("associate_audio_mixing_enable: associate_audio_mixing_enable:%d, advol_level:%, mixing_level:%d",
                        aml_dev->associate_audio_mixing_enable, aml_dev->advol_level, aml_dev->mixing_level);
            }
            if (false == associate_audio_mixing_enable) {
                pthread_mutex_lock(&patch->assoc_mutex);
                dtv_package_list_flush(patch->dtv_package_list);
                pthread_mutex_unlock(&patch->assoc_mutex);
                usleep(20000);
                continue;
            }
        }

        pthread_mutex_lock(&patch->assoc_mutex);

        struct package *p_package = NULL;
        p_package = dtv_package_get(ad_list);
        if (!p_package) {
            ts_wait_time(&ts, 100000);
            pthread_cond_timedwait(&patch->ad_cond, &patch->assoc_mutex, &ts);
            //pthread_cond_wait(&patch->cond, &patch->mutex);
            pthread_mutex_unlock(&patch->assoc_mutex);
            continue;
        } else {
            patch->cur_ad_package = p_package;
            AM_LOGI_IF(aml_dev->debug_flag, "ad package %p, pts:%llx(sz %d), num %d", p_package, p_package->pts, p_package->size, ad_list->pack_num);
        }

        pthread_mutex_unlock(&patch->assoc_mutex);

        pthread_mutex_lock(&(patch->dtv_ad_output_mutex));
        if (ad_stream_out) {
            if ((!p_package->data || !p_package->size)) {
                AM_LOGI("cur_package ad data invalid");
                goto free_package;
            }
            struct audio_buffer abuffer_out = {0};
            abuffer_out.buffer = p_package->data;
            abuffer_out.size = p_package->size;
            abuffer_out.pts = p_package->pts;

            //use specific write func of ad, instead of out_write_new: to skip unnecessary operations in out_write_new.
            ret_ad = mixer_ad_buffer_write(ad_stream_out, &abuffer_out);

            AM_LOGI_IF((aml_dev->debug_flag > 1), "fade %d, pan %d, mixing_level %d, advol %d, ret_ad %d",
                p_package->ad_fade, p_package->ad_pan, aml_dev->mixing_level, aml_dev->advol_level, ret_ad);

            if (eDolbyMS12Lib == aml_dev->dolby_lib_type &&
                (ad_aml_out->hal_internal_format == AUDIO_FORMAT_HE_AAC_V1 || ad_aml_out->hal_internal_format == AUDIO_FORMAT_HE_AAC_V2) &&
                (p_package->ad_fade != aml_dev->ad_fade || p_package->ad_pan != aml_dev->ad_pan)) {
                struct dolby_ms12_desc *ms12 = &(aml_dev->ms12);
                set_ms12_fade_pan
                (ms12
                , p_package->ad_fade /* fade byte*/
                , 0 /*gain_byte_center*/
                , 0 /*gain_byte_front */
                , 0 /*gain_byte_surround*/
                , p_package->ad_pan /*pan byte*/
                );
                aml_dev->ad_fade = p_package->ad_fade;
                aml_dev->ad_pan = p_package->ad_pan;
            }

        }
free_package:
        if (patch->cur_ad_package) {
            if (patch->cur_ad_package->data) {
                aml_audio_free(patch->cur_ad_package->data);
                patch->cur_ad_package->data = NULL;
            }
            aml_audio_free(patch->cur_ad_package);
            patch->cur_ad_package = NULL;
        }
        pthread_mutex_unlock(&(patch->dtv_ad_output_mutex));
    }

exit_outbuf:
    AM_LOGI("patch->ad_output_thread_exit %d", patch->ad_output_thread_exit);

    if (ad_aml_out) {
        adev_close_output_stream_new(dev, ad_stream_out);
    }
exit_open:
    AM_LOGI("--");
    return ((void *)0);
}

//pip case only works with single dtv patch.
//number of demux handle greater than 1 is pip case
static bool IsPipRun(aml_dtv_audio_instances_t * dtv_audio_instances)
{
    int ret=0;
    {
        for (int index = 0; index < DVB_DEMUX_SUPPORT_MAX_NUM; index ++) {
            if (dtv_audio_instances->demux_handle[index] != 0)
            {
                ret++;
            }
        }
    }
    return ret > 1;
}

static void *audio_dtv_patch_process_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct audio_stream_in *stream_in = NULL;
    struct audio_config stream_config;
    int ret = 0, retry = 0;
    audio_format_t cur_aformat;
    int cmd = AUDIO_DTV_PATCH_CMD_NUM;
    int path_id  = 0;
    aml_dtvsync_t *dtvsync=NULL;
    struct mediasync_audio_format audio_format={0};
    stream_config.sample_rate = 48000;
    patch->chanmask = stream_config.channel_mask = AUDIO_CHANNEL_IN_STEREO;
    patch->aformat = stream_config.format = AUDIO_FORMAT_PCM_16_BIT;

    int switch_flag = aml_audio_property_get_int("vendor.media.audio.strategy.switch", 0);
    int show_first_nosync = aml_audio_property_get_int("vendor.media.video.show_first_frame_nosync", 1);
    patch->pre_latency = aml_audio_property_get_int(PROPERTY_PRESET_AC3_PASSTHROUGH_LATENCY, 30);
    patch->a_discontinue_threshold = aml_audio_property_get_int(
                                        PROPERTY_AUDIO_DISCONTINUE_THRESHOLD, 30 * 90000);
    patch->sync_para.cur_pts_diff = 0;
    patch->sync_para.in_out_underrun_flag = 0;
    patch->sync_para.pcr_adjust_max = aml_audio_property_get_int(
                                        PROPERTY_AUDIO_ADJUST_PCR_MAX, 1 * 90000);
    patch->sync_para.underrun_mute_time_min = aml_audio_property_get_int(
                                        PROPERTY_UNDERRUN_MUTE_MINTIME, 200);
    patch->sync_para.underrun_mute_time_max = aml_audio_property_get_int(
                                        PROPERTY_UNDERRUN_MUTE_MAXTIME, 1000);
    patch->sync_para.underrun_max_time =  aml_audio_property_get_int(
                                        PROPERTY_UNDERRUN_MAX_TIME, 5000);

    AM_LOGI("switch_flag=%d, show_first_nosync=%d, pre_latency=%d,discontinue:%d",
        switch_flag, show_first_nosync, patch->pre_latency,
        patch->a_discontinue_threshold);
    AM_LOGI("sync:pcr_adjust_max=%d", patch->sync_para.pcr_adjust_max);
    AM_LOGI("++ Enter.");
    patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
    aml_demux_audiopara_t *demux_info = NULL;
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    prctl(PR_SET_NAME, (unsigned long)"dtv_input_cmd");
    while (!patch->cmd_process_thread_exit ) {

        pthread_mutex_lock(&patch->dtv_cmd_process_mutex);
        switch (patch->dtv_decoder_state) {
        case AUDIO_DTV_PATCH_DECODER_STATE_INIT: {
            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_START;
            aml_dev->underrun_mute_flag = 0;
        }
        break;
        case AUDIO_DTV_PATCH_DECODER_STATE_START:

            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                pthread_cond_wait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (cmd == AUDIO_DTV_PATCH_CMD_START) {
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
                aml_dev->patch_start = false;
                memset(&patch->dtv_resample, 0, sizeof(struct resample_para));
                if (patch->resample_outbuf) {
                    memset(patch->resample_outbuf, 0, OUTPUT_BUFFER_SIZE * 3);
                }

                if (patch->dtv_aformat == ACODEC_FMT_AC3) {
                    patch->aformat = AUDIO_FORMAT_AC3;
                } else if (patch->dtv_aformat == ACODEC_FMT_EAC3) {
                    patch->aformat = AUDIO_FORMAT_E_AC3;
                } else if (patch->dtv_aformat == ACODEC_FMT_DTS) {
                    patch->aformat = AUDIO_FORMAT_DTS;
                } else if (patch->dtv_aformat == ACODEC_FMT_AAC ) {
                    if (eDolbyMS12Lib == aml_dev->dolby_lib_type_last)
                        patch->aformat = AUDIO_FORMAT_HE_AAC_V2;
                    else
                        patch->aformat = AUDIO_FORMAT_AAC;
                } else if (patch->dtv_aformat == ACODEC_FMT_AAC_LATM) {
                    if (eDolbyMS12Lib == aml_dev->dolby_lib_type_last)
                        patch->aformat = AUDIO_FORMAT_HE_AAC_V1;
                    else
                        patch->aformat = AUDIO_FORMAT_AAC_LATM;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG ) {
                    patch->aformat = AUDIO_FORMAT_MP3;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG1) {
                    patch->aformat = AUDIO_FORMAT_MP2;/* audio-base.h no define mpeg1 format */
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG2) {
                    patch->aformat = AUDIO_FORMAT_MP2;
                } else if (patch->dtv_aformat == ACODEC_FMT_AC4) {
                   patch->aformat = AUDIO_FORMAT_AC4;
                } else {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                }

                AM_LOGI("patch->demux_handle %p patch->aformat %0x", patch->demux_handle, patch->aformat);
                dtvsync = &dtv_audio_instances->dtvsync[path_id];
                patch->dtvsync = dtvsync;
                if (dtvsync->mediasync_new != NULL) {
                    audio_format.format = patch->dtv_aformat;
                    mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_AUDIOFORMAT, &audio_format);
                    patch->dtvsync->mediasync = dtvsync->mediasync_new;
                    //dtvsync->mediasync_new = NULL;
                }
                patch->dtv_first_apts_flag = 0;
                patch->outlen_after_last_validpts = 0;
                patch->last_valid_pts = 0;
                patch->first_apts_lookup_over = 0;
                patch->ac3_pcm_dropping = 0;
                patch->last_audio_delay = 0;
                patch->pcm_inserting = false;
                patch->startplay_firstvpts = 0;
                patch->startplay_first_checkinapts = 0;
                patch->startplay_pcrpts = 0;
                patch->startplay_apts_lookup = 0;
                patch->startplay_vpts = 0;
                patch->last_valid_main_pts = NULL_INT64;
                patch->last_valid_ad_pts   = NULL_INT64;

                patch->dtv_pcm_readed = patch->dtv_pcm_writed = 0;
                patch->numDecodedSamples = patch->numOutputSamples = 0;
                //flush old data when start dmx
                if (aml_dev->is_multi_demux) {
                    if (patch->demux_handle && (!IsPipRun(dtv_audio_instances))) {
                        Flush_Dmx_Audio(patch->demux_handle);
                    }
                    if (!IsPipRun(dtv_audio_instances)) {
                        dtv_package_list_flush(patch->dtv_package_list);
                        if (patch->dtv_ad_package_list)
                            dtv_package_list_flush(patch->dtv_ad_package_list);
                    }
                    Start_Dmx_Main_Audio(patch->demux_handle);
                }
                create_dtv_input_stream_thread(patch);
                create_dtv_output_stream_thread(patch);
            } else {
                AM_LOGI("unsupport cmd, state %d cmd %d !",
                      patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;
        case AUDIO_DTV_PATCH_DECODER_STATE_RUNING:

            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            if ((0 == patch->ad_output_thread_created) && check_ad_enable(patch)) {
                int ad_start_flag;
                demux_info = (aml_demux_audiopara_t *)patch->demux_info;
                if (aml_dev->is_multi_demux) {
                    if (demux_info && VALID_PID(demux_info->ad_pid)) {
                        if (aml_audio_property_get_bool("vendor.media.dtv.pesmode", true)) {
                            if (VALID_AD_FMT_UK(demux_info->ad_fmt)) {
                                Init_Dmx_AD_Audio(patch->demux_handle, demux_info->ad_fmt, demux_info->ad_pid, 1);
                            } else {
                                Init_Dmx_AD_Audio(patch->demux_handle, demux_info->ad_fmt, demux_info->ad_pid, 0);
                            }
                        }
                        else {
                            Init_Dmx_AD_Audio(patch->demux_handle, demux_info->ad_fmt, demux_info->ad_pid, 0);
                        }
                    }
                } else {
                    Open_Dmx_Audio(&patch->demux_handle,demux_info->demux_id, demux_info->security_mem_level);
                    dtv_audio_instances->demux_handle[path_id] = patch->demux_handle;
                    Init_Dmx_AD_Audio(patch->demux_handle, demux_info->ad_fmt, demux_info->ad_pid, 1);
                }
                AM_LOGI("path_id %d demux_handle %p, demux_info->ad_pid:%d",path_id, patch->demux_handle, demux_info->ad_pid);
                ad_start_flag = Start_Dmx_AD_Audio(patch->demux_handle);
                if (0 == ad_start_flag) {
                    patch->ad_output_thread_exit = 0;
                    pthread_mutex_init(&patch->dtv_ad_output_mutex, NULL);
                    ret = pthread_create(&(patch->audio_ad_output_threadID), NULL,
                                         audio_dtv_patch_ad_output_threadloop, patch);
                    if (ret != 0) {
                        AM_LOGE("Create ad output thread fail!\n");
                        pthread_mutex_destroy(&patch->dtv_ad_output_mutex);
                        return -1;
                    }
                    patch->ad_output_thread_created = 1;
                } else {
                    AM_LOGE("Start_Dmx_AD_Audio fail! demux_handle:%p, ad_pid:%d, ad_fmt:%d", patch->demux_handle, demux_info->ad_pid, demux_info->ad_fmt);
                }
            }

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                pthread_cond_wait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (cmd == AUDIO_DTV_PATCH_CMD_PAUSE) {
                AM_LOGI("now start pause the audio decoder");
                if (aml_dev->is_multi_demux) {
                    path_id = dtv_audio_instances->demux_index_working;
                    patch->demux_handle = dtv_audio_instances->demux_handle[path_id];
                    aml_dtvsync_t *dtvsync = &dtv_audio_instances->dtvsync[path_id];
                    if (dtvsync->mediasync) {
                        mediasync_wrap_setPause(dtvsync->mediasync, true);
                    }
                    //if (patch->demux_handle) {
                    //    Stop_Dmx_Main_Audio(patch->demux_handle);
                    //    Stop_Dmx_AD_Audio(patch->demux_handle);
                    //}
                }  else {
                    aml_dtvsync_t *dtvsync = &dtv_audio_instances->dtvsync[0];
                    if (dtvsync->mediasync) {
                        mediasync_wrap_setPause(dtvsync->mediasync, true);
                    }
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_PAUSE;
                AM_LOGI("now end pause the audio decoder");
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                AM_LOGI("now stop the audio decoder");
                dtv_do_ease_out(aml_dev);
                struct audio_stream_out *stream_out = (struct audio_stream_out *)patch->dtv_aml_out;
                struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
                aml_out->fast_quit = true;
                AM_LOGI("aml_out:%p, set fast_quit:%d", aml_out, aml_out->fast_quit);

                //non-pip case ,Make sure you get the first new data
                //Always hold a current package in input_stream thread,so need release it.
                if (!IsPipRun(dtv_audio_instances) && aml_dev->is_multi_demux) {
                    release_dtv_input_stream_thread(patch);
                }
                release_dtv_output_stream_thread(patch);
                if (aml_dev->is_multi_demux) {
                    dtv_package_list_flush(patch->dtv_package_list);
                    if (patch->dtv_ad_package_list)
                        dtv_package_list_flush(patch->dtv_ad_package_list);
                }
                dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
                if (aml_dev->is_multi_demux) {
                    path_id = dtv_audio_instances->demux_index_working;
                    patch->demux_handle = dtv_audio_instances->demux_handle[path_id];
                    //if (patch->demux_handle && (!IsPipRun(dtv_audio_instances))) {
                    //    Flush_Dmx_Audio(patch->demux_handle);
                    //}
                }  else {
                    //dtv_assoc_audio_stop(1);
                }
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
           } else {
                AM_LOGI("unsupport cmd, state:%d cmd:%d !",
                      patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;

        case AUDIO_DTV_PATCH_DECODER_STATE_PAUSE:

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                pthread_cond_wait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (cmd == AUDIO_DTV_PATCH_CMD_RESUME) {
                if (aml_dev->is_multi_demux) {
                    aml_dtvsync_t *dtvsync = &dtv_audio_instances->dtvsync[path_id];
                    if (dtvsync->mediasync) {
                        mediasync_wrap_setPause(dtvsync->mediasync, false);
                    }
                } else {
                    aml_dtvsync_t *dtvsync = &dtv_audio_instances->dtvsync[0];
                    if (dtvsync->mediasync) {
                        mediasync_wrap_setPause(dtvsync->mediasync, false);
                    }
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                AM_LOGI("now stop the audio decoder now");

                if (aml_dev->is_multi_demux) {
                    path_id = dtv_audio_instances->demux_index_working;
                    patch->demux_handle = dtv_audio_instances->demux_handle[path_id];
                    //if (patch->demux_handle) {
                    //    Stop_Dmx_Main_Audio(patch->demux_handle);
                    //    Stop_Dmx_AD_Audio(patch->demux_handle);
                    //}
                }  else {
                   // dtv_assoc_audio_stop(1);
                }
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
            } else {
                AM_LOGI("unsupport cmd, state:%d cmd:%d !",
                      patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;
        default:
            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }
            break;
        }
        pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
    }

exit:
    AM_LOGI("++ now release  the audio decoder");
    release_dtv_input_stream_thread(patch);
    release_dtv_output_stream_thread(patch);
    //dtv_assoc_audio_stop(1);
    dtv_check_audio_reset();
    AM_LOGI("-- Exit");
    pthread_exit(NULL);
}

static int create_dtv_output_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    AM_LOGI("++ main:%d, ad:%d %d\n", patch->output_thread_created, patch->ad_output_thread_created);

    if (patch->output_thread_created == 0) {
        patch->output_thread_exit = 0;
        pthread_mutex_init(&patch->dtv_output_mutex, NULL);
        patch->dtv_replay_flag = true;
        ret = pthread_create(&(patch->audio_output_threadID), NULL,
                             audio_dtv_patch_output_threadloop, patch);
        if (ret != 0) {
            AM_LOGE("Create main output thread fail!\n");
            pthread_mutex_destroy(&patch->dtv_output_mutex);
            return -1;
        }
        patch->output_thread_created = 1;
    }

    AM_LOGI("--");
    return 0;
}

static int release_dtv_output_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    AM_LOGI("++ main:%d, ad:%d, %d\n", patch->output_thread_created, patch->ad_output_thread_created);
    if (patch->output_thread_created == 1) {
        patch->output_thread_exit = 1;
        pthread_cond_signal(&patch->cond);
        pthread_join(patch->audio_output_threadID, NULL);
        pthread_mutex_destroy(&patch->dtv_output_mutex);
        patch->output_thread_created = 0;
    }
    if (patch->ad_output_thread_created == 1) {
        patch->ad_output_thread_exit = 1;
        pthread_cond_signal(&patch->ad_cond);
        pthread_join(patch->audio_ad_output_threadID, NULL);
        pthread_mutex_destroy(&patch->dtv_ad_output_mutex);
        patch->ad_output_thread_created = 0;
    }
    AM_LOGI("--");
    return 0;
}


static int create_dtv_input_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    AM_LOGI("++ input_thread_created:%d\n", patch->input_thread_created);

    if (patch->input_thread_created == 0) {
        patch->input_thread_exit = 0;
        pthread_mutex_init(&patch->dtv_input_mutex, NULL);
        patch->dtv_replay_flag = true;
        ret = pthread_create(&(patch->audio_input_threadID), NULL,
                             audio_dtv_patch_input_threadloop, patch);
        if (ret != 0) {
            AM_LOGE("Create output thread fail!\n");
            pthread_mutex_destroy(&patch->dtv_input_mutex);
            return -1;
        }

        patch->input_thread_created = 1;
    }
    AM_LOGI("--");
    return 0;
}

static int release_dtv_input_stream_thread(struct aml_audio_patch * patch)
{
    int ret = 0;
    AM_LOGI("++ input_thread_created:%d\n", patch->input_thread_created);
    if (patch->input_thread_created == 1) {
        patch->input_thread_exit = 1;
        pthread_join(patch->audio_input_threadID, NULL);
        pthread_mutex_destroy(&patch->dtv_input_mutex);
        patch->input_thread_created = 0;
    }
    AM_LOGI("--");
    return 0;
}

int create_dtv_patch_l(struct audio_hw_device *dev, audio_devices_t input,
                       audio_devices_t output __unused)
{
    struct aml_audio_patch *patch;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    aml_demux_audiopara_t *demux_audiopara;
    int period_size = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    pthread_attr_t attr;
    struct sched_param param;
    aml_demux_audiopara_t *demux_info = NULL;
    int ret = 0;
    // AM_LOGI("++%s live period_size %d\n", __func__, period_size);
    //pthread_mutex_lock(&aml_dev->patch_lock);
    if (aml_dev->audio_patch) {
        AM_LOGD("patch exists, first release it");
        if (aml_dev->audio_patch->is_dtv_src) {
            return ret;
            //release_dtv_patch_l(aml_dev);
        } else {
            release_patch_l(aml_dev);
        }
    }
    patch = aml_audio_calloc(1, sizeof(*patch));
    if (!patch) {
        ret = -1;
        goto err;
    }

    if (aml_dev->dtv_i2s_clock == 0) {
        aml_dev->dtv_i2s_clock = aml_mixer_ctrl_get_int(&(aml_dev->alsa_mixer), AML_MIXER_ID_CHANGE_I2S_PLL);
        aml_dev->dtv_spidif_clock = aml_mixer_ctrl_get_int(&(aml_dev->alsa_mixer), AML_MIXER_ID_CHANGE_SPDIF_PLL);
    }


    // save dev to patch
    patch->dev = dev;
    patch->input_src = input;
    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
    patch->is_dtv_src = true;
    patch->startplay_avsync_flag = 1;
    patch->ad_substream_checked_flag = false;
    patch->dtv_volume = 1.0;
    patch->pts_dts_flag = 0;

    patch->output_thread_exit = 0;
    patch->cmd_process_thread_exit = 0;
    memset(&patch->sync_para, 0, sizeof(struct avsync_para));

    patch->i2s_div_factor = aml_audio_property_get_int(PROPERTY_AUDIO_TUNING_CLOCK_FACTOR, DEFAULT_TUNING_CLOCK_FACTOR);
    if (patch->i2s_div_factor == 0)
        patch->i2s_div_factor = DEFAULT_TUNING_CLOCK_FACTOR;

    pthread_mutex_lock(&aml_dev->dtv_patch_lock);
    aml_dev->audio_patch = patch;
    pthread_mutex_unlock(&aml_dev->dtv_patch_lock);
    aml_dev->start_mute_flag = false;
    pthread_mutex_init(&patch->mutex, NULL);
    pthread_cond_init(&patch->cond, NULL);
    pthread_cond_init(&patch->dtv_cmd_process_cond, NULL);
    pthread_mutex_init(&patch->dtv_cmd_process_mutex, NULL);
    pthread_mutex_init(&patch->apts_cal_mutex, NULL);

    patch->dtv_cmd_list = aml_audio_calloc(1, sizeof(struct cmd_node));
     if (!patch->dtv_cmd_list) {
        ret = -1;
        goto err;
    }

    init_cmd_list(patch->dtv_cmd_list);

    if (!aml_dev->is_multi_demux)
    {
        //clean pipe data
        int retlen = 0;
        char dropdata[256] = {0};
        aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
        do
        {
            retlen=read(dtv_audio_instances->paudiofd, dropdata, 256);
        }
        while (retlen > 0);
    }

    patch->dtv_package_list = aml_audio_calloc(1, sizeof(package_list));
    if (!patch->dtv_package_list) {
        ret = -1;
        goto err;
    }
    patch->dtv_ad_package_list = aml_audio_calloc(1, sizeof(package_list));
    if (!patch->dtv_ad_package_list) {
        ret = -1;
        goto err;
    }

    ret = pthread_create(&(patch->audio_cmd_process_threadID), NULL,
                         audio_dtv_patch_process_threadloop, patch);
    if (ret != 0) {
        AM_LOGE("Create process thread fail!");
        goto err;
    }

    if (aml_dev->tuner2mix_patch) {
        ret = ring_buffer_init(&patch->dtvin_ringbuffer, 4 * 8192 * 2 * 16);
        AM_LOGI("aring_buffer_init ret=%d\n", ret);
        if (ret == 0) {
            patch->dtvin_buffer_inited = 1;
        }
    }
    patch->dtv_aformat = aml_dev->dtv_aformat;
    patch->dtv_output_clock = 0;
    patch->dtv_default_i2s_clock = aml_dev->dtv_i2s_clock;
    patch->dtv_default_spdif_clock = aml_dev->dtv_spidif_clock;
    patch->spdif_format_set = 0;
    patch->spdif_step_clk = 0;
    patch->i2s_step_clk = 0;
    patch->pid = -1;
    patch->debug_para.debug_last_checkin_apts = 0;
    patch->debug_para.debug_last_checkin_vpts = 0;
    patch->debug_para.debug_last_out_apts = 0;
    patch->debug_para.debug_last_out_vpts = 0;
    patch->debug_para.debug_last_demux_pcr = 0;
    patch->debug_para.debug_time_interval = aml_audio_property_get_int(PROPERTY_DEBUG_TIME_INTERVAL, DEFULT_DEBUG_TIME_INTERVAL);
    patch->ad_output_thread_created = 0;

    AM_LOGI("--");
    return 0;

err:
    if (patch->dtv_package_list)
        aml_audio_free(patch->dtv_package_list);
    if (patch->dtv_ad_package_list)
        aml_audio_free(patch->dtv_ad_package_list);
    deinit_cmd_list(patch->dtv_cmd_list);
    aml_audio_free(patch);
    aml_dev->audio_patch = NULL;

    return ret;
}

int release_dtv_patch_l(struct aml_audio_device *aml_dev)
{
    if (aml_dev == NULL) {
        AM_LOGE("aml_dev == NULL");
        return 0;
    }
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    struct audio_hw_device *dev   = (struct audio_hw_device *)aml_dev;
    AM_LOGI("++ Enter");
    if (patch == NULL) {
        AM_LOGE("patch == NULL");
        return 0;
    }

    patch->cmd_process_thread_exit = 1;
    pthread_cond_signal(&patch->dtv_cmd_process_cond);
    pthread_join(patch->audio_cmd_process_threadID, NULL);
    pthread_mutex_destroy(&patch->dtv_cmd_process_mutex);
    pthread_cond_destroy(&patch->dtv_cmd_process_cond);
    if (patch->resample_outbuf) {
        aml_audio_free(patch->resample_outbuf);
        patch->resample_outbuf = NULL;
    }
    patch->pid = -1;
    //release_dtvin_buffer(patch);
    if (patch->dtv_package_list)
        aml_audio_free(patch->dtv_package_list);
    if (patch->dtv_ad_package_list)
        aml_audio_free(patch->dtv_ad_package_list);
    deinit_cmd_list(patch->dtv_cmd_list);

    pthread_mutex_lock(&aml_dev->dtv_patch_lock);
    aml_audio_free(patch);
    aml_dev->audio_patch = NULL;
    pthread_mutex_unlock(&aml_dev->dtv_patch_lock);

    if (aml_dev->start_mute_flag != 0)
        aml_dev->start_mute_flag = 0;
    aml_dev->underrun_mute_flag = 0;
    AM_LOGI("-- Exit");

    return 0;
}

int create_dtv_patch(struct audio_hw_device *dev, audio_devices_t input,
                     audio_devices_t output)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    aml_dtv_audio_instances_t *dtv_instances = (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    int ret = 0;
    pthread_mutex_lock(&aml_dev->patch_lock);
    if (aml_dev->patch_src == SRC_DTV)
        dtv_instances->dvb_path_count++;
    AM_LOGI("dtv_instances->dvb_path_count %d", dtv_instances->dvb_path_count);
    ret = create_dtv_patch_l(dev, input, output);
    pthread_mutex_unlock(&aml_dev->patch_lock);

    return ret;
}

int release_dtv_patch(struct aml_audio_device *aml_dev)
{
    int ret = 0;
    aml_dtv_audio_instances_t *dtv_instances = (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    pthread_mutex_lock(&aml_dev->patch_lock);
    if (aml_dev->patch_src == SRC_DTV && dtv_instances->dvb_path_count > 0) {
        dtv_instances->dvb_path_count--;
        AM_LOGI("dtv_instances->dvb_path_count %d", dtv_instances->dvb_path_count);
        if (dtv_instances->dvb_path_count == 0) {
            aml_dev->synctype = AVSYNC_TYPE_NULL;
            ret = release_dtv_patch_l(aml_dev);
        }
    }
    pthread_mutex_unlock(&aml_dev->patch_lock);
    return ret;
}

void dtv_in_write(struct audio_stream_out *stream, const void* buffer, size_t bytes)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    int abuf_level = 0;

    if (stream == NULL || buffer == NULL || bytes == 0) {
        AM_LOGW("stream:%p or buffer:%p is null, or bytes:%d = 0.", stream, buffer, bytes);
        return;
    }
    if ((adev->patch_src == SRC_DTV) && (patch->dtvin_buffer_inited == 1)) {
        abuf_level = get_buffer_write_space(&patch->dtvin_ringbuffer);
        if (abuf_level <= (int)bytes) {
            AM_LOGI("dtvin ringbuffer is full");
            return;
        }
        ring_buffer_write(&patch->dtvin_ringbuffer, (unsigned char *)buffer, bytes, UNCOVER_WRITE);
    }
    //AM_LOGI("[%s] dtvin write ringbuffer successfully,abuf_level=%d", __FUNCTION__,abuf_level);
}
int dtv_in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    int ret = 0;
    unsigned int es_length = 0;
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *adev = in->dev;

    if (stream == NULL || buffer == NULL || bytes == 0) {
        AM_LOGW("stream:%p or buffer:%p is null, or bytes:%d = 0.", stream, buffer, bytes);
        return bytes;
    }

    struct aml_audio_patch *patch = adev->audio_patch;
    //AM_LOGI("[%s] patch->aformat=0x%x patch->dtv_decoder_ready=%d bytes:%d\n", __FUNCTION__,patch->aformat,patch->dtv_decoder_ready,bytes);

    if (patch->dtvin_buffer_inited == 1) {
        int abuf_level = get_buffer_read_space(&patch->dtvin_ringbuffer);
        if (abuf_level <= (int)bytes) {
            memset(buffer, 0, sizeof(unsigned char)* bytes);
            ret = bytes;
        } else {
            ret = ring_buffer_read(&patch->dtvin_ringbuffer, (unsigned char *)buffer, bytes);
            //AM_LOGI("[%s] abuf_level =%d ret=%d\n", __FUNCTION__,abuf_level,ret);
        }
        return bytes;
    } else {
        memset(buffer, 0, sizeof(unsigned char)* bytes);
        ret = bytes;
        return ret;
    }
    return ret;
}

bool dtv_is_secure(void *dtv_instances)
{
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)dtv_instances;
    if (dtv_audio_instances) {
        return dtv_audio_instances->demux_info[dtv_audio_instances->demux_index_working].security_mem_level != 0;
    } else {
        return false;
    }
}

int dtv_patch_get_es_pts_dts_flag(struct aml_audio_device *aml_dev)
{
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    if (patch == NULL) {
        AM_LOGI("dtv patch == NULL");
        return 0;
    }
    AM_LOGI("pts_dts_flag %d", patch->pts_dts_flag);
    return patch->pts_dts_flag;
}

