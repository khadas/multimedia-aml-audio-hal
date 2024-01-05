/*
 * Copyright (C) 2010 Amlogic Corporation.
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



#define LOG_TAG "audio_hw_hal_hwsync"
//#define LOG_NDEBUG 0
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <cutils/log.h>
#include <string.h>
#include <errno.h>
#include <utils/Timers.h>
#include "audio_hw_utils.h"
#include "audio_hwsync.h"
#include "audio_hw.h"
#include "audio_hwsync_wrap.h"
#include "audio_mediasync_wrap.h"

void* aml_hwsync_wrap_mediasync_create (void) {
    return mediasync_wrap_create();
}

bool aml_hwsync_wrap_get_id(void *mediasync, int32_t* id)
{
    if(mediasync) {
        return mediasync_wrap_allocInstance(mediasync, 0, 0, id);
    }
    return false;
}

bool aml_hwsync_wrap_set_id(audio_hwsync_t *p_hwsync, uint32_t id)
{

    if (p_hwsync->es_mediasync.mediasync) {
        return mediasync_wrap_bindInstance(p_hwsync->es_mediasync.mediasync, id, MEDIA_AUDIO);
    }
    return false;
}

bool aml_hwsync_wrap_release(audio_hwsync_t *p_hwsync)
{

    if (p_hwsync->es_mediasync.mediasync) {
        mediasync_wrap_destroy(p_hwsync->es_mediasync.mediasync);
        return true;
    }
    return false;
}


