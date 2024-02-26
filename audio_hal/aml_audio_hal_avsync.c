/*
 * Copyright (C) 2019 Amlogic Corporation.
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
 *
 * Author:
 * yinli.xia@amlogic.com
 *
 * Function:
 * this file is created for starting play avsync
 */

#define LOG_TAG "audio_hw_hal_avsync"

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
#include <system/audio.h>
#include <time.h>
#include <utils/Timers.h>
#if ANDROID_PLATFORM_SDK_VERSION >= 25 // 8.0
#include <system/audio-base.h>
#endif
#include <hardware/audio.h>
#include <aml_android_utils.h>
#include <aml_data_utils.h>
#include "aml_audio_stream.h"
#include "aml_audio_timer.h"
#include "aml_data_utils.h"
#include "audio_hw.h"
#include "audio_hw_dtv.h"
#include "audio_hw_profile.h"
#include "audio_hw_utils.h"
#include "aml_audio_resampler.h"
#include "dolby_lib_api.h"
#include "audio_dtv_ad.h"
#include "alsa_device_parser.h"
#include "aml_audio_hal_avsync.h"

static unsigned int compare_clock(unsigned int clock1, unsigned int clock2, unsigned int factor)
{
    if (clock1 == clock2) {
        return true;
    }
    if (clock1 > clock2) {
        if (clock1 < clock2 + 60 * factor) {
            return true;
        }
    }
    if (clock1 < clock2) {
        if (clock2 < clock1 + 60 * factor) {
            return true;
        }
    }
    return false;
}

void dtv_adjust_i2s_output_clock(struct aml_audio_patch* patch, int direct, int step)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device * aml_dev = (struct aml_audio_device*)adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);
    int output_clock = 0;
    unsigned int i2s_current_clock = 0;
    i2s_current_clock = aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL);
    if (i2s_current_clock > DEFAULT_I2S_OUTPUT_CLOCK * 4 ||
        i2s_current_clock == 0 || step <= 0 || step > DEFAULT_DTV_OUTPUT_CLOCK) {
        return;
    }
    if (direct == DIRECT_SPEED) {
        if (i2s_current_clock >= patch->dtv_default_i2s_clock) {
            if (i2s_current_clock - patch->dtv_default_i2s_clock >=
                (patch->dtv_default_i2s_clock * DEFAULT_DTV_ADJUST_CLOCK_THRESHOLD / 100)) {
                ALOGI("already > i2s_step_clk 1M,no need speed adjust\n");
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        } else {
            int value = patch->dtv_default_i2s_clock - i2s_current_clock;
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        }
    } else if (direct == DIRECT_SLOW) {
        if (i2s_current_clock <= patch->dtv_default_i2s_clock) {
            if (patch->dtv_default_i2s_clock - i2s_current_clock >
                (patch->dtv_default_i2s_clock * DEFAULT_DTV_ADJUST_CLOCK_THRESHOLD / 100)) {
                ALOGI("already < 1M no need adjust slow, return\n");
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        } else {
            int value = i2s_current_clock - patch->dtv_default_i2s_clock;
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        }
    } else {
        if (compare_clock(i2s_current_clock, patch->dtv_default_i2s_clock, 1)) {
            return ;
        }
        if (i2s_current_clock > patch->dtv_default_i2s_clock) {
            int value = i2s_current_clock - patch->dtv_default_i2s_clock;
            if (value < 60) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        } else if (i2s_current_clock < patch->dtv_default_i2s_clock) {
            int value = patch->dtv_default_i2s_clock - i2s_current_clock;
            if (value < 60) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        }
    }
    return;
}

void dtv_adjust_earc_output_clock(struct aml_audio_patch* patch, int direct, int step)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);
    int output_clock, i, compare_factor = 1;
    unsigned int earc_current_clock = 0;
    unsigned int earc_default_clock = 0;
    eMixerCtrlID mixerID = AML_MIXER_ID_CHANGE_EARC_PLL;
    int device_index = alsa_device_update_pcm_index(PORT_EARC, PLAYBACK);

    if (device_index == -1) {
        return;
    }

    earc_current_clock = aml_mixer_ctrl_get_int(handle, mixerID);

    int audio_type = aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_EARC_TX_AUDIO_TYPE);

    if (audio_type == AML_AUDIO_CODING_TYPE_STEREO_LPCM ||
        audio_type == AML_AUDIO_CODING_TYPE_AC3 ||
        audio_type == AML_AUDIO_CODING_TYPE_AC3_LAYOUT_B ||
        audio_type == AML_AUDIO_CODING_TYPE_DTS) {
        patch->dtv_default_arc_clock = DEFAULT_EARC_OUTPUT_CLOCK;
        compare_factor = 5;
    } else if (audio_type == AML_AUDIO_CODING_TYPE_EAC3){
        patch->dtv_default_arc_clock = DEFAULT_EARC_OUTPUT_CLOCK * 4;
        compare_factor = 5 * 4;
        step *= 4;
    } else if (audio_type == AML_AUDIO_CODING_TYPE_MLP ||
        audio_type == AML_AUDIO_CODING_TYPE_DTS_HD ||
        audio_type == AML_AUDIO_CODING_TYPE_DTS_HD_MA) {
         patch->dtv_default_arc_clock = DEFAULT_EARC_OUTPUT_CLOCK * 4 * 4;
         compare_factor = 5 * 4 * 4;
         step *= 16;
    }
    if (aml_audio_get_debug_flag())
        ALOGI("dtv_adjust_earc_output_clock direct %d step %d spdif_current_clock %u",direct, step, earc_current_clock);
    if (earc_current_clock > DEFAULT_EARC_OUTPUT_CLOCK * 4 * 4 ||
        earc_current_clock == 0 || step <= 0 || step > DEFAULT_DTV_OUTPUT_CLOCK) {
        return;
    }
    if (direct == DIRECT_SPEED) {
        if (compare_clock(earc_current_clock, patch->dtv_default_arc_clock, compare_factor)) {
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("arc_clock 1 set %d to %d",earc_current_clock,aml_mixer_ctrl_get_int(handle, mixerID));
        } else if (earc_current_clock < patch->dtv_default_arc_clock) {
            int value = patch->dtv_default_arc_clock - earc_current_clock;
            if (value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
           if (aml_audio_get_debug_flag())
                ALOGI("arc_clock 2 set %d to %d",earc_current_clock,aml_mixer_ctrl_get_int(handle, mixerID));

        } else {
            if (aml_audio_get_debug_flag())
                ALOGI("arc_SPEED clk %d,default %d",earc_current_clock,patch->dtv_default_arc_clock);
            return ;
        }
    } else if (direct == DIRECT_SLOW) {
        if (compare_clock(earc_current_clock, patch->dtv_default_arc_clock, compare_factor)) {
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("arc_clock 3 set %d to %d",earc_current_clock,aml_mixer_ctrl_get_int(handle, mixerID));
        } else if (earc_current_clock > patch->dtv_default_arc_clock) {
            int value = earc_current_clock - patch->dtv_default_arc_clock;
            if (value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("arc_clock 4 set %d to %d",earc_current_clock,aml_mixer_ctrl_get_int(handle, mixerID));
        } else {
            if (aml_audio_get_debug_flag())
                ALOGI("arc_SLOW clk %d,default %d",earc_current_clock,patch->dtv_default_arc_clock);
            return ;
        }
    } else {
        if (compare_clock(earc_current_clock, patch->dtv_default_arc_clock, compare_factor)) {
            return ;
        }
        if (earc_current_clock > patch->dtv_default_arc_clock) {
            int value = earc_current_clock - patch->dtv_default_arc_clock;
            if (value < 60 || value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("arc_clock 5 set %d to %d",earc_current_clock,aml_mixer_ctrl_get_int(handle, mixerID));
        } else if (earc_current_clock < patch->dtv_default_arc_clock) {
            int value = patch->dtv_default_arc_clock - earc_current_clock;
            if (value < 60 || value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("arc_clock 6 set %d to %d",earc_current_clock,aml_mixer_ctrl_get_int(handle, mixerID));
        } else {
            return ;
        }
    }
}

void dtv_adjust_spdif_output_clock(struct aml_audio_patch* patch, int direct, int step, bool spdifb)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);
    int output_clock, i;
    unsigned int spdif_current_clock = 0;

    if (ATTEND_TYPE_NONE != aml_audio_earctx_get_type(aml_dev) && spdifb) {
        dtv_adjust_earc_output_clock(patch, direct, patch->arc_step_clk / patch->i2s_div_factor);
        return;
    }

    eMixerCtrlID mixerID = spdifb ? AML_MIXER_ID_CHANGE_SPDIFB_PLL : AML_MIXER_ID_CHANGE_SPDIF_PLL;
    spdif_current_clock = aml_mixer_ctrl_get_int(handle, mixerID);
    ALOGI("dtv_adjust_spdif_output_clock direct %d step %d spdifb %d spdif_current_clock %u",direct, step, spdifb, spdif_current_clock);
    if (spdif_current_clock > DEFAULT_SPDIF_PLL_DDP_CLOCK * 4 ||
        spdif_current_clock == 0 || step <= 0 || step > DEFAULT_DTV_OUTPUT_CLOCK) {
        return;
    }
    if (direct == DIRECT_SPEED) {
        if (compare_clock(spdif_current_clock, patch->dtv_default_spdif_clock, 1)) {
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_clock 1 set %d to %d",spdif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPDIF_PLL));
        } else if (spdif_current_clock < patch->dtv_default_spdif_clock) {
            int value = patch->dtv_default_spdif_clock - spdif_current_clock;
            if (value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_clock 2 set %d to %d",spdif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPDIF_PLL));

        } else {
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_SPEED clk %d,default %d",spdif_current_clock,patch->dtv_default_spdif_clock);
            return ;
        }
    } else if (direct == DIRECT_SLOW) {
        if (compare_clock(spdif_current_clock, patch->dtv_default_spdif_clock, 1)) {
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_clock 3 set %d to %d",spdif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPDIF_PLL));
        } else if (spdif_current_clock > patch->dtv_default_spdif_clock) {
            int value = spdif_current_clock - patch->dtv_default_spdif_clock;
            if (value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_clock 4 set %d to %d",spdif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPDIF_PLL));
        } else {
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_SLOW clk %d,default %d",spdif_current_clock,patch->dtv_default_spdif_clock);
            return ;
        }
    } else {
        if (compare_clock(spdif_current_clock, patch->dtv_default_spdif_clock, 1)) {
            return ;
        }
        if (spdif_current_clock > patch->dtv_default_spdif_clock) {
            int value = spdif_current_clock - patch->dtv_default_spdif_clock;
            if (value < 60 || value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_clock 5 set %d to %d",spdif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPDIF_PLL));
        } else if (spdif_current_clock < patch->dtv_default_spdif_clock) {
            int value = patch->dtv_default_spdif_clock - spdif_current_clock;
            if (value < 60 || value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_clock 6 set %d to %d",spdif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPDIF_PLL));
        } else {
            return ;
        }
    }
}

void dtv_adjust_output_clock(struct aml_audio_patch * patch, int direct, int step, bool dual)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    bool spdif_b = true;
    if (aml_audio_get_debug_flag())
        ALOGI("dtv_adjust_output_clock not set,%x", patch->dtv_pcm_readed);
    if (!aml_dev || step <= 0 || patch->dtv_audio_mode) {
        return;
    }
#if 0
    if (patch->decoder_offset < 512 * 2 * 10 &&
        ((patch->aformat == AUDIO_FORMAT_AC3) ||
         (patch->aformat == AUDIO_FORMAT_E_AC3))) {
        return;
    }
#endif
    if (patch->dtv_default_spdif_clock > DEFAULT_I2S_OUTPUT_CLOCK * 4 ||
        patch->dtv_default_spdif_clock == 0) {
        return;
    }
    patch->pll_state = direct;
    if (direct == DIRECT_SPEED) {
        clock_gettime(CLOCK_MONOTONIC, &(patch->speed_time));
    } else if (direct == DIRECT_SLOW) {
        clock_gettime(CLOCK_MONOTONIC, &(patch->slow_time));
    }
    if (patch->spdif_format_set == 0) {
        if (patch->dtv_default_i2s_clock > DEFAULT_SPDIF_PLL_DDP_CLOCK * 4 ||
            patch->dtv_default_i2s_clock == 0) {
            return;
        }
        ALOGI("i2s_step_clk:%d, i2s_div_factor:%d.", patch->i2s_step_clk, patch->i2s_div_factor);
        dtv_adjust_i2s_output_clock(patch, direct, patch->i2s_step_clk / patch->i2s_div_factor);
    } else if (!aml_dev->bHDMIARCon) {
        if (patch->dtv_default_i2s_clock > DEFAULT_SPDIF_PLL_DDP_CLOCK * 4 ||
            patch->dtv_default_i2s_clock == 0) {
            return;
        }
        ALOGI("i2s_step_clk:%d, spdif_step_clk:%d, i2s_div_factor:%d.", patch->i2s_step_clk, patch->spdif_step_clk, patch->i2s_div_factor);
        dtv_adjust_i2s_output_clock(patch, direct, patch->i2s_step_clk / patch->i2s_div_factor);
        dtv_adjust_spdif_output_clock(patch, direct, patch->spdif_step_clk / patch->i2s_div_factor, dual);
    } else {
        dtv_adjust_spdif_output_clock(patch, direct, patch->spdif_step_clk / 4, dual);
    }
}

static void dtv_adjust_output_clock_continue(struct aml_audio_patch * patch, int direct)
{
    struct timespec current_time;
    int time_cost = 0;
    static int last_div = 0;
    int adjust_interval = 0;
    patch->i2s_div_factor = aml_audio_property_get_int(PROPERTY_AUDIO_TUNING_CLOCK_FACTOR, DEFAULT_TUNING_CLOCK_FACTOR);
    adjust_interval = aml_audio_property_get_int("vendor.media.audio_hal.adjtime", 1000);
    if (last_div != patch->i2s_div_factor) {
        ALOGI("new_div=%d, adjust_interval=%d ms,spdif_format_set=%d\n",
            patch->i2s_div_factor, adjust_interval, patch->spdif_format_set);
        last_div = patch->i2s_div_factor;
    }

    if (patch->pll_state == DIRECT_NORMAL || patch->pll_state != direct) {
        ALOGI("pll_state=%d, direct=%d no need continue\n", patch->pll_state, direct);
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (direct == DIRECT_SPEED) {
        time_cost = calc_time_interval_us(&patch->speed_time, &current_time)/1000;
    } else if (direct == DIRECT_SLOW) {
        time_cost = calc_time_interval_us(&patch->slow_time, &current_time)/1000;
    }
    if (time_cost > adjust_interval && patch->spdif_format_set == 0) {
        ALOGI("over %d ms continue to adjust the clock\n", time_cost);
        dtv_adjust_output_clock(patch, direct, DEFAULT_DTV_ADJUST_CLOCK, false);
    }
    return;
}

/* input latency by format for ms12 case */
static int dtv_get_ms12_input_latency(struct audio_stream_out *stream, audio_format_t input_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct aml_audio_patch *patch = (struct aml_audio_patch *)aml_dev->audio_patch;
    int latency_ms = 0;
    char *prop_name = NULL;

    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        prop_name = DTV_AVSYNC_MS12_PCM_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_MS12_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3: {
        prop_name = DTV_AVSYNC_MS12_DD_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_MS12_DD_LATENCY;
        break;
    }
    case AUDIO_FORMAT_E_AC3: {
        prop_name = DTV_AVSYNC_MS12_DDP_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_MS12_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = DTV_AVSYNC_MS12_AC4_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_MS12_AC4_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AAC:
    case AUDIO_FORMAT_HE_AAC_V1:
    case AUDIO_FORMAT_HE_AAC_V2: {
        prop_name = DTV_AVSYNC_MS12_AAC_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_MS12_AAC_LATENCY;
        break;
    }
    default:
        break;
    }
    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }

    //ALOGI("%s IN input_format(%d), latency_ms(%d), prop_name(%s)", __func__, input_format, latency_ms, prop_name);

    return latency_ms;
}

/* outport latency for ms12 case */
static int dtv_get_ms12_port_latency(struct audio_stream_out *stream, enum OUT_PORT port, audio_format_t output_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    struct aml_audio_patch *patch = (struct aml_audio_patch *)aml_dev->audio_patch;
    int latency_ms = 0;
    char *prop_name = NULL;

    switch (port) {
        case OUTPORT_HDMI_ARC:
        {
            if (output_format == AUDIO_FORMAT_AC3) {
                if (aml_dev->ms12.is_bypass_ms12) {
                    latency_ms = DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DD_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DD_LATENCY_PROPERTY;
                } else {
                    latency_ms = DTV_AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY_PROPERTY;
                }
            }
            else if (output_format == AUDIO_FORMAT_E_AC3) {
                if (aml_dev->ms12.is_bypass_ms12) {
                    latency_ms = DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DDP_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DDP_LATENCY_PROPERTY;
                } else {
                    latency_ms = DTV_AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY_PROPERTY;
                }
            }
            else if (output_format == AUDIO_FORMAT_MAT) {
                latency_ms = DTV_AVSYNC_MS12_HDMI_ARC_OUT_MAT_LATENCY;
                prop_name = DTV_AVSYNC_MS12_HDMI_ARC_OUT_MAT_LATENCY_PROPERTY;
            } else {
                latency_ms = DTV_AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY;
                prop_name = DTV_AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY_PROPERTY;
            }
            break;
        }
        case OUTPORT_HDMI:
        {
            if (output_format == AUDIO_FORMAT_AC3) {
                if (aml_dev->ms12.is_bypass_ms12) {
                    latency_ms = DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DD_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DD_LATENCY_PROPERTY;
                } else {
                    latency_ms = AVSYNC_MS12_DTV_HDMI_OUT_DD_LATENCY;
                    prop_name = AVSYNC_MS12_DTV_HDMI_OUT_DD_LATENCY_PROPERTY;
                }
            }
            else if (output_format == AUDIO_FORMAT_E_AC3) {
                if (aml_dev->ms12.is_bypass_ms12) {
                    latency_ms = DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DDP_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DDP_LATENCY_PROPERTY;
                } else {
                    latency_ms = AVSYNC_MS12_DTV_HDMI_OUT_DDP_LATENCY;
                    prop_name = AVSYNC_MS12_DTV_HDMI_OUT_DDP_LATENCY_PROPERTY;
                }
            }
            else if (output_format == AUDIO_FORMAT_MAT) {
                latency_ms = AVSYNC_MS12_DTV_HDMI_OUT_MAT_LATENCY;
                prop_name = AVSYNC_MS12_DTV_HDMI_OUT_MAT_LATENCY_PROPERTY;
            }
            else {
                latency_ms = AVSYNC_MS12_DTV_HDMI_OUT_PCM_LATENCY;
                prop_name = AVSYNC_MS12_DTV_HDMI_OUT_PCM_LATENCY_PROPERTY;
            }
            break;

        }
        case OUTPORT_SPEAKER:
        case OUTPORT_AUX_LINE:
        {
            if (aml_dev->is_TV) {
                latency_ms = DTV_AVSYNC_MS12_TV_SPEAKER_LATENCY;
                prop_name = DTV_AVSYNC_MS12_TV_SPEAKER_LATENCY_PROPERTY;
            } else {
                latency_ms = DTV_AVSYNC_MS12_STB_SPEAKER_LATENCY;
                prop_name = DTV_AVSYNC_MS12_STB_SPEAKER_LATENCY_PROPERTY;
            }
            break;
        }
        case OUTPORT_A2DP:
        {
            if (aml_dev->is_TV) {
                if (aml_dev->tuner2mix_patch) {
                    latency_ms = DTV_AVSYNC_MS12_TV_MIX_A2DP_LATENCY;
                    prop_name = DTV_AVSYNC_MS12_TV_MIX_A2DP_LATENCY_PROPERTY;
                } else {
                    latency_ms = 0;
                }
            } else {
                latency_ms = 0;
            }
            break;
        }
        default :
            break;
    }
    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }


    //ALOGI("%s PORT (port:%d)latency_ms(%d), prop_name(%s)", __func__, port, latency_ms, prop_name);

    return latency_ms;
}

/* offset latency for ms12 case */
static int dtv_get_ms12_offset_latency(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int latency_ms = 0;
    int input_latency_ms = 0;
    int output_latency_ms = 0;
    int port_latency_ms = 0;

    input_latency_ms = dtv_get_ms12_input_latency(stream, aml_out->hal_internal_format);
    port_latency_ms = dtv_get_ms12_port_latency(stream, aml_dev->active_outport, aml_dev->ms12.optical_format);
    latency_ms = input_latency_ms + output_latency_ms + port_latency_ms;
    ALOGV("%s total latency %d(ms) hal_fmt %x(%d ms) opt_fmt %x(%d ms) port %d(%d ms) is_bypass_ms12 %d", __func__, latency_ms,
        aml_out->hal_internal_format, input_latency_ms, aml_dev->ms12.optical_format, output_latency_ms,
        aml_dev->active_outport, port_latency_ms, aml_dev->ms12.is_bypass_ms12);
    return latency_ms;
}
/* input latency by format for nonms12 case */
static int dtv_get_nonms12_input_latency(struct audio_stream_out *stream, audio_format_t input_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int latency_ms = 0;
    char *prop_name = NULL;

    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        prop_name = DTV_AVSYNC_NONMS12_PCM_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_NONMS12_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3: {
        prop_name = DTV_AVSYNC_NONMS12_DD_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_NONMS12_DD_LATENCY;
        break;
    }

    case AUDIO_FORMAT_E_AC3: {
        prop_name = DTV_AVSYNC_NONMS12_DDP_LATENCY_PROPERTY;
        latency_ms = DTV_AVSYNC_NONMS12_DDP_LATENCY;
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

/* outport latency for nonms12 case */
int dtv_get_nonms12_port_latency(struct audio_stream_out * stream, enum OUT_PORT port, audio_format_t output_format)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int latency_ms = 0;
    char *prop_name = NULL;

    switch (port) {
        case OUTPORT_HDMI_ARC:
        {
            if (output_format == AUDIO_FORMAT_AC3) {
                latency_ms = DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DD_LATENCY;
                prop_name = DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DD_LATENCY_PROPERTY;
            }
            else if (output_format == AUDIO_FORMAT_E_AC3) {
                latency_ms = DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DDP_LATENCY;
                prop_name = DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DDP_LATENCY_PROPERTY;
            }
            else {
                latency_ms = DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PCM_LATENCY;
                prop_name = DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PCM_LATENCY_PROPERTY;
            }
            break;
        }
        case OUTPORT_HDMI:
        {
            if (output_format == AUDIO_FORMAT_AC3) {
                latency_ms = AVSYNC_NONMS12_DTV_HDMI_OUT_DD_LATENCY;
                prop_name = AVSYNC_NONMS12_DTV_HDMI_OUT_DD_LATENCY_PROPERTY;
            }
            else if (output_format == AUDIO_FORMAT_E_AC3) {
                latency_ms = AVSYNC_NONMS12_DTV_HDMI_OUT_DDP_LATENCY;
                prop_name = AVSYNC_NONMS12_DTV_HDMI_OUT_DDP_LATENCY_PROPERTY;
            }
            else {
                latency_ms = AVSYNC_NONMS12_DTV_HDMI_OUT_PCM_LATENCY;
                prop_name = AVSYNC_NONMS12_DTV_HDMI_OUT_PCM_LATENCY_PROPERTY;
            }
            break;
        }
        case OUTPORT_SPEAKER:
        case OUTPORT_AUX_LINE:
        {
            if (aml_dev->is_TV) {
                latency_ms = DTV_AVSYNC_NONMS12_TV_SPEAKER_LATENCY;
                prop_name = DTV_AVSYNC_NONMS12_TV_SPEAKER_LATENCY_PROPERTY;
            } else {
                latency_ms = DTV_AVSYNC_NONMS12_STB_SPEAKER_LATENCY;
                prop_name = DTV_AVSYNC_NONMS12_STB_SPEAKER_LATENCY_PROPERTY;
            }
            break;
        }
        default :
            break;
    }

    if (prop_name) {
        latency_ms = aml_audio_property_get_int(prop_name, latency_ms);
    }
    return latency_ms;
}

/* offset latency for nonms12 case */
static int dtv_get_nonms12_offset_latency(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int latency_ms = 0;
    int input_latency_ms = 0;
    int output_latency_ms = 0;
    int port_latency_ms = 0;

    input_latency_ms = dtv_get_nonms12_input_latency(stream, aml_out->hal_internal_format);
    port_latency_ms = dtv_get_nonms12_port_latency(stream, aml_dev->active_outport, aml_dev->sink_format);
    latency_ms = input_latency_ms + output_latency_ms + port_latency_ms;
    ALOGI("%s total latency %d(ms) hal_fmt %x(%d ms) opt_fmt %x(%d ms) port %d(%d ms)", __func__, latency_ms,
        aml_out->hal_internal_format, input_latency_ms, aml_dev->sink_format, output_latency_ms,
        aml_dev->active_outport, port_latency_ms);
    return latency_ms;
}

/* get apts latency for avsync, uint: pts*/
int dtv_avsync_get_apts_latency(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int32_t video_delay_ms = 0;
    int32_t alsa_delay_ms = 0;
    int32_t tuning_ms = 0;
    int total_latency = 0;

    alsa_delay_ms = (int32_t)out_get_outport_latency(stream);
    if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
        tuning_ms = dtv_get_ms12_offset_latency(stream);
    } else {
        tuning_ms = dtv_get_nonms12_offset_latency(stream);
    }
#ifndef NO_AUDIO_CAP
    if (aml_dev->cap_buffer) {
        tuning_ms += aml_dev->cap_delay;
    } else {
        tuning_ms += aml_audio_property_get_int(PROPERTY_LOCAL_PASSTHROUGH_LATENCY, 0);
    }
#else
    tuning_ms += aml_audio_property_get_int(PROPERTY_LOCAL_PASSTHROUGH_LATENCY, 0);
#endif
    if (aml_dev->is_TV) {
        video_delay_ms = 0;
    }

    total_latency = (tuning_ms + alsa_delay_ms + video_delay_ms) * 90;
    return total_latency;
}


