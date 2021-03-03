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
#include <stdbool.h>
#include <string.h>

#include "aml_audio_delay.h"

#define ALIGN(size, align) ((size + align - 1) & (~(align - 1)))

static aml_audio_delay_st g_stAudioDelay[AML_DELAY_BUTT];
static int g_stAudioInputDelay[AML_DELAY_INPUT_MAX] = {0, 0, 0, 0, 0};
static const int g_u32DelayMaxDefault[AML_DELAY_BUTT] = {250, 250, 250, 250};
static bool g_bAudioDelayInit = false;
static aml_audio_delay_input_type_e g_delay_input_type = AML_DELAY_INPUT_DEFAULT;

int aml_audio_delay_init()
{
    int i;
    int s32BfferSize = 0;
    unsigned int u32ChannelCnt;

    memset(&g_stAudioDelay, 0, sizeof(aml_audio_delay_st)*AML_DELAY_BUTT);

    for (i=0; i<AML_DELAY_BUTT; i++) {
        u32ChannelCnt = 2;
        if ((AML_DELAY_OUTPORT_ALL == i) || (AML_DELAY_OUTPORT_SPEAKER == i)) {
            /*calculate the max size for 8ch */
            u32ChannelCnt = 8;
        } else if (AML_DELAY_INPORT_ALL == i) {
            // worst case MAT input
            // 192k sr * 2 channel * 2 (bytes) * 16 (hbr)
            u32ChannelCnt = 16;
        }
        s32BfferSize = 192 * u32ChannelCnt * 4 * g_u32DelayMaxDefault[i];
        ring_buffer_init(&g_stAudioDelay[i].stDelayRbuffer, s32BfferSize);
    }

    for (i=0; i<AML_DELAY_INPUT_MAX; i++) {
        g_stAudioInputDelay[i] = 0;
    }

    g_delay_input_type = AML_DELAY_INPUT_DEFAULT;
    g_bAudioDelayInit = true;

    return 0;
}

int aml_audio_delay_deinit()
{
    if (!g_bAudioDelayInit) {
        ALOGW("[%s:%d] audio delay not initialized", __func__, __LINE__);
        return -1;
    }
    for (unsigned int i=0; i<AML_DELAY_BUTT; i++) {
        ring_buffer_release(&g_stAudioDelay[i].stDelayRbuffer);
    }
    g_bAudioDelayInit = false;
    return 0;
}

/*
* s32DelayTimeMs: In milliseconds
*/
int aml_audio_delay_set_time(aml_audio_delay_type_e enAudioDelayType, int s32DelayTimeMs)
{
    if (!g_bAudioDelayInit) {
        ALOGW("[%s:%d] audio delay not initialized", __func__, __LINE__);
        return -1;
    }
    if (enAudioDelayType < AML_DELAY_OUTPORT_SPEAKER || enAudioDelayType >= AML_DELAY_BUTT) {
        ALOGW("[%s:%d] delay type:%d invalid, min:%d, max:%d",
            __func__, __LINE__, enAudioDelayType, AML_DELAY_OUTPORT_SPEAKER, AML_DELAY_BUTT-1);
        return -1;
    }
    if (s32DelayTimeMs < 0 || s32DelayTimeMs > g_u32DelayMaxDefault[enAudioDelayType]) {
        ALOGW("[%s:%d] unsupport delay time:%dms, min:%dms, max:%dms",
            __func__, __LINE__, s32DelayTimeMs, 0, g_u32DelayMaxDefault[enAudioDelayType]);
        return -1;
    }
    g_stAudioDelay[enAudioDelayType].delay_time = s32DelayTimeMs;
    ALOGI("set audio output type:%d delay time: %dms", enAudioDelayType, s32DelayTimeMs);
    return 0;
}

int aml_audio_delay_get_time(aml_audio_delay_type_e enAudioDelayType)
{
    int delay = 0;

    if ((g_bAudioDelayInit) &&
        (enAudioDelayType >= AML_DELAY_OUTPORT_SPEAKER) &&
        (enAudioDelayType < AML_DELAY_BUTT)) {
        delay = g_stAudioDelay[enAudioDelayType].delay_time;
    }

    return delay;
}

int aml_audio_delay_input_set_time(aml_audio_delay_input_type_e enAudioDelayType, int s32DelayTimeMs)
{
    if (!g_bAudioDelayInit) {
        ALOGW("[%s:%d] audio delay not initialized", __func__, __LINE__);
        return -1;
    }

    if (enAudioDelayType < AML_DELAY_INPUT_HDMI || enAudioDelayType >= AML_DELAY_INPUT_MAX) {
        ALOGW("[%s:%d] delay input type:%d invalid, min:%d, max:%d",
        __func__, __LINE__, enAudioDelayType, AML_DELAY_INPUT_HDMI, AML_DELAY_INPUT_MAX-1);
        return -1;
    }

    if (s32DelayTimeMs < 0 || s32DelayTimeMs > g_u32DelayMaxDefault[AML_DELAY_INPORT_ALL]) {
        ALOGW("[%s:%d] unsupport delay time:%dms, min:%dms, max:%dms",
            __func__, __LINE__, s32DelayTimeMs, 0, g_u32DelayMaxDefault[AML_DELAY_INPORT_ALL]);
        return -1;
    }
    g_stAudioInputDelay[enAudioDelayType] = s32DelayTimeMs;
    ALOGI("set audio input type::%d delay time: %dms", enAudioDelayType, s32DelayTimeMs);

    if (g_delay_input_type == enAudioDelayType) {
        g_stAudioDelay[AML_DELAY_INPORT_ALL].delay_time = s32DelayTimeMs;
    }

    return 0;
}

int aml_audio_delay_clear(aml_audio_delay_type_e enAudioDelayType)
{
    if (!g_bAudioDelayInit) {
        ALOGW("[%s:%d] audio delay not initialized", __func__, __LINE__);
        return -1;
    }
    if (enAudioDelayType < AML_DELAY_OUTPORT_SPEAKER || enAudioDelayType >= AML_DELAY_BUTT) {
        ALOGW("[%s:%d] delay type:%d invalid, min:%d, max:%d",
            __func__, __LINE__, enAudioDelayType, AML_DELAY_OUTPORT_SPEAKER, AML_DELAY_BUTT-1);
        return -1;
    }

    ring_buffer_reset(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer);
    return 0;
}

int aml_audio_delay_input_set_type(aml_audio_delay_input_type_e enAudioDelayType)
{
    if (enAudioDelayType < AML_DELAY_INPUT_HDMI || enAudioDelayType >= AML_DELAY_INPUT_MAX) {
        ALOGW("[%s:%d] delay input type:%d invalid, min:%d, max:%d",
            __func__, __LINE__, enAudioDelayType, AML_DELAY_INPUT_HDMI, AML_DELAY_INPUT_MAX-1);
        return -1;
    }

    if (g_stAudioDelay[AML_DELAY_INPORT_ALL].delay_time != g_stAudioInputDelay[enAudioDelayType]) {
        aml_audio_delay_clear(AML_DELAY_INPORT_ALL);
        g_stAudioDelay[AML_DELAY_INPORT_ALL].delay_time = g_stAudioInputDelay[enAudioDelayType];
        ALOGI("Set input delay to %dms", g_stAudioDelay[AML_DELAY_INPORT_ALL].delay_time);
    }

    g_delay_input_type = enAudioDelayType;

    return 0;
}

static int _get_delay_ms_size(audio_format_t enFormat, int nChannel,
    int nSampleRate, int nSampleSize)
{
    int size;

    if (enFormat == AUDIO_FORMAT_AC3) {
        size = nSampleRate * 2 * 2;
    } else if ((enFormat == AUDIO_FORMAT_E_AC3) ||
        (enFormat == AUDIO_FORMAT_E_AC3_JOC)) {
        size = nSampleRate * 2 * 2 * 4;
    } else if (enFormat == AUDIO_FORMAT_MAT) {
        size = nSampleRate * 2 * 2 * 16;
    } else if (enFormat == AUDIO_FORMAT_IEC61937) {
        size = nSampleRate * 2 * 2;
    } else {
        /* PCM */
        size = nSampleRate * nChannel * nSampleSize;
    }

    return size / 1000;
}

static int _delay_process(aml_audio_delay_type_e enAudioDelayType, unsigned int u32OneMsSize,
   void *pData, int s32Size)
{
    int s32CurNeedDelaySize = 0;
    int s32AvailDataSize = 0;

    // calculate need delay total size
    s32CurNeedDelaySize = ALIGN(g_stAudioDelay[enAudioDelayType].delay_time * u32OneMsSize, 16);
    // get current ring buffer delay data size
    s32AvailDataSize = ALIGN((get_buffer_read_space(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer) / u32OneMsSize) * u32OneMsSize, 16);

    ring_buffer_write(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer, (unsigned char *)pData, s32Size, UNCOVER_WRITE);
    ALOGV("%s:%d AvailDataSize:%d, u32OneMsSize:%d", __func__, __LINE__, s32AvailDataSize, u32OneMsSize);

    // accumulate this delay data
    if (s32CurNeedDelaySize > s32AvailDataSize) {
        int s32NeedAddDelaySize = s32CurNeedDelaySize - s32AvailDataSize;
        if (s32NeedAddDelaySize >= s32Size) {
            memset(pData, 0, s32Size);
            ALOGD("%s:%d type:%d,accumulate Data, CurNeedDelaySize:%d, need more DelaySize:%d, size:%d", __func__, __LINE__,
                enAudioDelayType, s32CurNeedDelaySize, s32NeedAddDelaySize, s32Size);
        } else {
            // splicing this pData data
            memset(pData, 0, s32NeedAddDelaySize);
            ring_buffer_read(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer, (unsigned char *)pData+s32NeedAddDelaySize, s32Size-s32NeedAddDelaySize);
            ALOGD("%s:%d type:%d accumulate part pData CurNeedDelaySize:%d, need more DelaySize:%d, size:%d", __func__, __LINE__,
                enAudioDelayType, s32CurNeedDelaySize, s32NeedAddDelaySize, s32Size);
        }
    // decrease this delay data
    } else if (s32CurNeedDelaySize < s32AvailDataSize) {
        unsigned int u32NeedDecreaseDelaySize = s32AvailDataSize - s32CurNeedDelaySize;
        // drop this delay data
        unsigned int    u32ClearedSize = 0;
        for (;u32ClearedSize < u32NeedDecreaseDelaySize; ) {
            unsigned int u32ResidualClearSize = u32NeedDecreaseDelaySize - u32ClearedSize;
            if (u32ResidualClearSize > (unsigned int)s32Size) {
                ring_buffer_read(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer, (unsigned char *)pData, s32Size);
                u32ClearedSize += s32Size;
            } else {
                ring_buffer_read(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer, (unsigned char *)pData, u32ResidualClearSize);
                break;
            }
        }
        ring_buffer_read(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer, (unsigned char *)pData, s32Size);
        ALOGD("%s:%d type:%d drop delay data, CurNeedDelaySize:%d, NeedDecreaseDelaySize:%d, size:%d", __func__, __LINE__,
            enAudioDelayType, s32CurNeedDelaySize, u32NeedDecreaseDelaySize, s32Size);
    } else {
        ring_buffer_read(&g_stAudioDelay[enAudioDelayType].stDelayRbuffer, (unsigned char *)pData, s32Size);
        ALOGV("%s:%d do nothing, CurNeedDelaySize:%d, size:%d", __func__, __LINE__, s32CurNeedDelaySize, s32Size);
    }

    return 0;
}

int aml_audio_delay_input_process(void *pData, int s32Size,
    audio_format_t enFormat, int nChannel, int nsampleRate, int nSampleSize)
{
    unsigned int u32OneMsSize;

    if (!g_bAudioDelayInit) {
        ALOGW("[%s:%d] audio delay not initialized", __func__, __LINE__);
        return -1;
    }

    if (!s32Size) {
        return 0;
    }

    u32OneMsSize = _get_delay_ms_size(enFormat, nChannel, nsampleRate, nSampleSize);

    return _delay_process(AML_DELAY_INPORT_ALL, u32OneMsSize, pData, s32Size);
}

int aml_audio_delay_input_get_ms()
{
    return g_stAudioInputDelay[g_delay_input_type];
}

int aml_audio_delay_process(aml_audio_delay_type_e enAudioDelayType, void *pData, int s32Size, audio_format_t enFormat, int nChannel)
{
    unsigned int u32OneMsSize = 0;

    if (!g_bAudioDelayInit) {
        ALOGW("[%s:%d] audio delay not initialized", __func__, __LINE__);
        return -1;
    }

    if (!s32Size) {
        return 0;
    }

    if (enAudioDelayType < AML_DELAY_OUTPORT_SPEAKER || enAudioDelayType >= AML_DELAY_BUTT) {
        ALOGW("[%s:%d] delay type:%d invalid, min:%d, max:%d",
            __func__, __LINE__, enAudioDelayType, AML_DELAY_OUTPORT_SPEAKER, AML_DELAY_BUTT-1);
        return -1;
    }

    if (AML_DELAY_OUTPORT_ALL == enAudioDelayType) {
        u32OneMsSize = _get_delay_ms_size(enFormat, 8, 48000, 4);
    } else if (AML_DELAY_OUTPORT_SPEAKER == enAudioDelayType){
        u32OneMsSize = _get_delay_ms_size(AUDIO_FORMAT_PCM, nChannel, 48000, 2); // 48k * nChannel * 2Byte
    } else if (AML_DELAY_OUTPORT_SPDIF == enAudioDelayType){
        if (AUDIO_FORMAT_IEC61937 == enFormat) {
            u32OneMsSize = _get_delay_ms_size(enFormat, 2, 48000, 2); // 48k * 2ch * 2Byte
        } else {
            u32OneMsSize = _get_delay_ms_size(AUDIO_FORMAT_PCM, 2, 48000, 4); // 48k * 2ch * 4Byte [Notes: alsa only support 32bit(4Byte)]
        }
    } else if (AML_DELAY_OUTPORT_ARC == enAudioDelayType) {
        /* for PCM, ARC output is 32 bits (extracted from 8ch 32bit output) */
        u32OneMsSize = _get_delay_ms_size(enFormat, nChannel, 48000, 4);
    }

    return _delay_process(enAudioDelayType, u32OneMsSize, pData, s32Size);
}

