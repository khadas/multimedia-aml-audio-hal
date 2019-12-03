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

#define LOG_TAG "aml_channel_delay"
//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <stdlib.h>
#include <string.h>

#include "aml_audio_delay.h"

#define ALIGN(size, align) ((size + align - 1) & (~(align - 1)))

int aml_audio_delay_init(aml_audio_delay_st **ppstAudioOutputDelayHandle)
{
    int s32BfferSize = 0;
    if (*ppstAudioOutputDelayHandle) {
        ALOGW("aml_audio_delay_st is already allocated");
        return 0;
    }
    *ppstAudioOutputDelayHandle = (aml_audio_delay_st *)calloc(1, sizeof(aml_audio_delay_st));

    if (*ppstAudioOutputDelayHandle == NULL) {
        ALOGW("aml_audio_delay_st calloc memory fail");
        return -1;
    }
    (*ppstAudioOutputDelayHandle)->delay_max              = OUTPUT_DELAY_MAX_MS;
    (*ppstAudioOutputDelayHandle)->delay_time             = 0;

    /*calculate the max size for 192K 8ch 32 bit, ONE_FRAME_MAX_MS is used for one frame buffer*/
    s32BfferSize = 192 * 8 * 4 * ((*ppstAudioOutputDelayHandle)->delay_max + ONE_FRAME_MAX_MS);
    ring_buffer_init(&(*ppstAudioOutputDelayHandle)->stDelayRbuffer, s32BfferSize);
    return 0;
}
int aml_audio_delay_close(aml_audio_delay_st **ppstAudioOutputDelayHandle)
{
    if (*ppstAudioOutputDelayHandle) {
        ring_buffer_release(&(*ppstAudioOutputDelayHandle)->stDelayRbuffer);
        free(*ppstAudioOutputDelayHandle);
    }

    *ppstAudioOutputDelayHandle = NULL;
    return 0;
}

/*
* s32DelayTimeMs: In milliseconds
*/
int aml_audio_delay_set_time(aml_audio_delay_st **ppstAudioOutputDelayHandle, int s32DelayTimeMs)
{
    if (s32DelayTimeMs < OUTPUT_DELAY_MIN_MS || s32DelayTimeMs > OUTPUT_DELAY_MAX_MS) {
        ALOGW("[%s:%d] unsupport delay time:%dms, min:%dms, max:%dms, set audio delay failed!",
            __func__, __LINE__, s32DelayTimeMs, OUTPUT_DELAY_MIN_MS, OUTPUT_DELAY_MAX_MS);
        return -1;
    }
    (*ppstAudioOutputDelayHandle)->delay_time = s32DelayTimeMs;
    ALOGI("set audio delay time: %dms", s32DelayTimeMs);
    return 0;
}

int aml_audio_delay_process(aml_audio_delay_st *pstAudioDelayHandle, void * in_data, int size, audio_format_t enFormat)
{
    unsigned int    u32OneMsSize = 0;
    int             s32CurNeedDelaySize = 0;
    int             s32AvailDataSize = 0;

    if (pstAudioDelayHandle == NULL) {
        return -1;
    }

    if (enFormat == AUDIO_FORMAT_E_AC3) {
        u32OneMsSize = 192 * 4;// * abs(adev->delay_ms);
    } else if (enFormat == AUDIO_FORMAT_AC3) {
        u32OneMsSize = 48 * 4;// * abs(adev->delay_ms);
    } else if (enFormat == AUDIO_FORMAT_MAT) {
        u32OneMsSize = 192 * 4 * 4;// * abs(adev->delay_ms);
    } else {
        u32OneMsSize = 48 * 32;// * abs(adev->delay_ms);
    }

    // calculate need delay total size
    s32CurNeedDelaySize = ALIGN(pstAudioDelayHandle->delay_time * u32OneMsSize, 16);
    // get current ring buffer delay data size
    s32AvailDataSize = ALIGN((get_buffer_read_space(&pstAudioDelayHandle->stDelayRbuffer) / u32OneMsSize) * u32OneMsSize, 16);

    ring_buffer_write(&pstAudioDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size, UNCOVER_WRITE);
    ALOGV("%s:%d AvailDataSize:%d", __func__, __LINE__, s32AvailDataSize);

    // accumulate this delay data
    if (s32CurNeedDelaySize > s32AvailDataSize) {
        int s32NeedAddDelaySize = s32CurNeedDelaySize - s32AvailDataSize;
        if (s32NeedAddDelaySize >= size) {
            memset(in_data, 0, size);
            ALOGD("%s:%d accumulate all in_data, CurNeedDelaySize:%d, AddDelaySize:%d, size:%d", __func__, __LINE__,
                s32CurNeedDelaySize, s32NeedAddDelaySize, size);
        } else {
            // splicing this in_data data
            memset(in_data, 0, s32NeedAddDelaySize);
            ring_buffer_read(&pstAudioDelayHandle->stDelayRbuffer, (unsigned char *)in_data+s32NeedAddDelaySize, size-s32NeedAddDelaySize);
            ALOGD("%s:%d accumulate part in_data CurNeedDelaySize:%d, AddDelaySize:%d, size:%d", __func__, __LINE__,
                s32CurNeedDelaySize, s32NeedAddDelaySize, size);
        }
    // decrease this delay data
    } else if (s32CurNeedDelaySize < s32AvailDataSize) {
        unsigned int u32NeedDecreaseDelaySize = s32AvailDataSize - s32CurNeedDelaySize;
        // drop this delay data
        unsigned int    u32ClearedSize = 0;
        for (;u32ClearedSize < u32NeedDecreaseDelaySize; ) {
            unsigned int u32ResidualClearSize = u32NeedDecreaseDelaySize - u32ClearedSize;
            if (u32ResidualClearSize > (unsigned int)size) {
                ring_buffer_read(&pstAudioDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size);
                u32ClearedSize += size;
            } else {
                ring_buffer_read(&pstAudioDelayHandle->stDelayRbuffer, (unsigned char *)in_data, u32ResidualClearSize);
                break;
            }
        }
        ring_buffer_read(&pstAudioDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size);
        ALOGD("%s:%d drop delay data, CurNeedDelaySize:%d, NeedDecreaseDelaySize:%d, size:%d", __func__, __LINE__,
            s32CurNeedDelaySize, u32NeedDecreaseDelaySize, size);
    } else {
        ring_buffer_read(&pstAudioDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size);
        ALOGV("%s:%d do nothing, CurNeedDelaySize:%d, size:%d", __func__, __LINE__, s32CurNeedDelaySize, size);
    }

    return 0;
}

