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

#ifndef USE_MSYNC
#define LOG_TAG "audio_avsync_stub"
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
#include "aml_avsync_stub.h"

void* av_sync_attach(int session_id, enum sync_type type)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return NULL;
}

void av_sync_destroy(void *sync)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return;
}

int av_sync_pause(void *sync, bool pause)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return -1;
}

int av_sync_audio_render(
    void *sync,
    pts90K pts,
    struct audio_policy *policy)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return -1;
}

avs_start_ret av_sync_audio_start(
    void *sync,
    pts90K pts,
    pts90K delay,
    audio_start_cb cb,
    void *priv)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return AV_SYNC_ASTART_ERR;
}

int av_sync_get_clock(void *sync, pts90K *pts)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return -1;
}

void log_set_level(int level)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return;
}

int av_sync_set_speed(void *sync, float speed)
{
    ALOGE("[%s():%d] stub function error!", __func__, __LINE__);
    return;
}

#endif

