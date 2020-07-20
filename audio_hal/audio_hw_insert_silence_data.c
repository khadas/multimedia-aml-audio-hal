/*
 * Copyright (C) 2020 Amlogic Corporation.
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
#include "aml_DRC_param_gen.h"
#include "aml_EQ_param_gen.h"

#if ANDROID_PLATFORM_SDK_VERSION >= 25 //8.0
#include <system/audio-base.h>
#endif

#include <hardware/audio.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include <audio_route/audio_route.h>
#include <aml_data_utils.h>
#include <spdifenc_wrap.h>
#include <aml_volume_utils.h>
#include <aml_android_utils.h>
#include <aml_alsa_mixer.h>

#include "audio_format_parse.h"
#include "aml_volume_utils.h"
#include "aml_data_utils.h"
#include "alsa_manager.h"
#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "alsa_manager.h"
#include "alsa_device_parser.h"
#include "aml_audio_stream.h"
#include "alsa_config_parameters.h"
#include "aml_avsync_tuning.h"
#include "audio_a2dp_hw.h"

int insert_silence_data(struct audio_stream_out *stream
                  , const void *buffer
                  , size_t bytes
                  , int  adjust_ms
                  , audio_format_t output_format)
{
    int ret = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;

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
                if (adev->atoms_lock_flag ||
                    (adev->ms12.is_dolby_atmos && adev->ms12_main1_dolby_dummy == false)) {
                    bAtmos = 1;
                }
                raw_buf = aml_audio_get_muteframe(output_format, &raw_size, bAtmos);
                ALOGI("insert atmos=%d raw frame size=%d times=%d", bAtmos, raw_size, insert_frame);
                if (raw_buf && (raw_size > 0)) {
                    temp_buf = malloc(raw_size);
                    if (!temp_buf) {
                        ALOGE("%s malloc failed", __func__);
                        pthread_mutex_unlock(&adev->alsa_pcm_lock);
                        return -1;
                    }
                    for (i = 0; i < insert_frame; i++) {
                        memcpy(temp_buf, raw_buf, raw_size);
                        ret = aml_alsa_output_write(stream, (void*)temp_buf, raw_size, output_format);
                        if (ret < 0) {
                            ALOGE("%s alsa write fail when insert", __func__);
                            break;
                        }
                    }
                    free(temp_buf);
                }
            }
        } else {
            memset((void*)buffer, 0, bytes);
            if (aml_out->a2dp_out) {
                adjust_bytes = 48 * 4 * abs(adjust_ms); // 2ch 16bit
            } else if (output_format == AUDIO_FORMAT_E_AC3) {
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
                char *buf = malloc(1024);
                int write_size = 0;
                if (!buf) {
                    ALOGE("%s malloc failed", __func__);
                    pthread_mutex_unlock(&adev->alsa_pcm_lock);
                    return -1;
                }
                memset(buf, 0, 1024);
                while (adjust_bytes > 0) {
                    write_size = adjust_bytes > 1024 ? 1024 : adjust_bytes;
#ifdef ENABLE_BT_A2DP
                    if (adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
                        ret = a2dp_out_write(stream, (void*)buf, write_size);
                    } else
#endif
                    {
                        ret = aml_alsa_output_write(stream, (void*)buf, write_size, output_format);
                    }
                    if (ret < 0) {
                        ALOGE("%s alsa write fail when insert", __func__);
                        break;
                    }
                    adjust_bytes -= write_size;
                }
                free(buf);
                buf = NULL;
            }
        }
    }

    return ret;
}


