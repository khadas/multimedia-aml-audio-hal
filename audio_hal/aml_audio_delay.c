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

#include "aml_audio_delay.h"
#include <aml_ringbuffer.h>


#define ONE_FRAME_MAX_MS   100
#define ALIGN(size, align) ((size + align - 1) & (~(align - 1)))

typedef struct delay_buf {
    unsigned char * buf_start;
    int buf_size;
    int wp;
    int rp;  // not used
    int delay_rp;
} delay_buf_t;

typedef struct delay_handle {
    int delay_time;
    int delay_max;
    struct ring_buffer stDelayRbuffer;
} delay_handle_t;


int aml_audiodelay_init(struct audio_hw_device *dev)
{
    int delay_max = 0;
    int buf_size = 0;
    unsigned char * buf_start = NULL;
    delay_handle_t * delay_handle = NULL;
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;

    delay_max =  adev->delay_max;

    delay_handle = calloc(1, sizeof(delay_handle_t));

    if (delay_handle == NULL) {
        return -1;
    }
    adev->delay_handle = delay_handle;

    /*calculate the max size for 192K 8ch 32 bit, ONE_FRAME_MAX_MS is used for one frame buffer*/
    buf_size = 192 * 8 * 4 * (delay_max + ONE_FRAME_MAX_MS);

    buf_start = calloc(1 , buf_size);
    if (buf_start == NULL) {
        ALOGE("malloc delay buffer failed for size: 0x%x\n", buf_size);
        return -1;
    }
    ring_buffer_init(&delay_handle->stDelayRbuffer, buf_size);

    delay_handle->delay_max              = delay_max;
    delay_handle->delay_time             = 0;

    adev->delay_handle = (void *)delay_handle;
    return 0;
}
int aml_audiodelay_close(struct audio_hw_device *dev)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    delay_handle_t * delay_handle = NULL;

    delay_handle = adev->delay_handle;

    if (delay_handle) {
        ring_buffer_release(&delay_handle->stDelayRbuffer);
        free(delay_handle);

    }

    adev->delay_handle = NULL;

    return 0;
}

int aml_audiodelay_process(struct audio_hw_device *pAudioDev, void * in_data, int size, audio_format_t format)
{
    delay_handle_t* phDelayHandle = NULL;
    unsigned int    u32OneMsSize = 0;
    int             s32CurNeedDelaySize = 0;
    int             s32AvailDataSize = 0;

    phDelayHandle = ((struct aml_audio_device *)pAudioDev)->delay_handle;
    if (phDelayHandle == NULL) {
        ALOGW("%s:%d delay_handle is null, pAudioDev:%#x", __func__, __LINE__, (unsigned int)pAudioDev);
        return -1;
    }

    if (format == AUDIO_FORMAT_E_AC3) {
        u32OneMsSize = 192 * 4;// * abs(adev->delay_ms);
    } else if (format == AUDIO_FORMAT_AC3) {
        u32OneMsSize = 48 * 4;// * abs(adev->delay_ms);
    } else {
        u32OneMsSize = 48 * 32;// * abs(adev->delay_ms);
    }

    // calculate need delay total size
    s32CurNeedDelaySize = ALIGN(((struct aml_audio_device *)pAudioDev)->delay_time * u32OneMsSize, 16);
    // get current ring buffer delay data size
    s32AvailDataSize = ALIGN((get_buffer_read_space(&phDelayHandle->stDelayRbuffer) / u32OneMsSize) * u32OneMsSize, 16);

    ring_buffer_write(&phDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size, UNCOVER_WRITE);
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
            ring_buffer_read(&phDelayHandle->stDelayRbuffer, (unsigned char *)in_data+s32NeedAddDelaySize, size-s32NeedAddDelaySize);
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
                ring_buffer_read(&phDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size);
                u32ClearedSize += size;
            } else {
                ring_buffer_read(&phDelayHandle->stDelayRbuffer, (unsigned char *)in_data, u32ResidualClearSize);
                break;
            }
        }
        ring_buffer_read(&phDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size);
        ALOGD("%s:%d drop delay data, CurNeedDelaySize:%d, NeedDecreaseDelaySize:%d, size:%d", __func__, __LINE__,
            s32CurNeedDelaySize, u32NeedDecreaseDelaySize, size);
    } else {
        ring_buffer_read(&phDelayHandle->stDelayRbuffer, (unsigned char *)in_data, size);
        ALOGV("%s:%d do nothing, CurNeedDelaySize:%d, size:%d", __func__, __LINE__, s32CurNeedDelaySize, size);
    }

    return 0;
}

