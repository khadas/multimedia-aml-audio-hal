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



#define LOG_TAG "audio_media_sync_util"
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
#include <sys/utsname.h>
#include <cutils/properties.h>
#include <tinyalsa/asoundlib.h>
#include <aml_android_utils.h>

#include "audio_media_sync_util.h"


audio_mediasync_util_t g_mediasync_util;
static pthread_mutex_t g_mediasync_util_lock = PTHREAD_MUTEX_INITIALIZER;

static int aml_audio_get_media_sync_flag()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int debug_flag = 0;
    ret = property_get("audio.media.sync.util.debug", buf, NULL);
    if (ret > 0) {
        debug_flag = atoi(buf);
    }
    return debug_flag;
}

audio_mediasync_util_t* aml_audio_mediasync_util_init()
{
    ALOGI("%s p_hwsync %p\n", __func__, &g_mediasync_util);
    pthread_mutex_lock(&g_mediasync_util_lock);
    audio_mediasync_util_t*p_hwsync = &g_mediasync_util;

    p_hwsync->last_lookup_apts = 0xffffffff;
    p_hwsync->same_pts_data_size = 0;
    memset(p_hwsync->pts_tab, 0, sizeof(pts_tab_t)*MEDIA_SYNC_APTS_NUM);
    p_hwsync->payload_offset = 0;
    p_hwsync->record_flag = false;
    p_hwsync->es_last_apts = 0xffffffff;
    p_hwsync->media_sync_debug = aml_audio_get_media_sync_flag();
    p_hwsync->insert_time_ms = 0;

    pthread_mutex_unlock(&g_mediasync_util_lock);

    ALOGI("%s done", __func__);
    return p_hwsync;
}

audio_mediasync_util_t* aml_audio_get_mediasync_util_handle()
{
    return &g_mediasync_util;
}


/**************************************************************************************
 * Function:    aml_audio_hwsync_reset_apts
 *
 * Description: Reset the pts table and the payload offset
 *
 * Inputs:      valid audio hwsync struct pointer (audio_hwsync_t)
 *
 * Outputs:     NONE
 *
 * Return:      NONE
 *
 * Notes:       For dolby ms12 cleanup, the consumed payload offset by dolby ms12 SDK will reset too,
                so, the pts table and the payload need reset correspond.
 **************************************************************************************/

int aml_audio_mediasync_util_checkin_apts(audio_mediasync_util_t *p_hwsync, size_t offset, uint64_t apts)
{
    int i = 0;
    int ret = -1;
    int debug_enable = p_hwsync->media_sync_debug;
    pts_tab_t *pts_tab = NULL;
    if (!p_hwsync) {
        ALOGE("%s null point", __func__);
        return -1;
    }
    if (debug_enable) {
        ALOGI("++ %s checkin ,offset %zu,apts 0x%llx", __func__, offset, apts);
    }
    pthread_mutex_lock(&g_mediasync_util_lock);
    pts_tab = p_hwsync->pts_tab;
    for (i = 0; i < MEDIA_SYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid && (pts_tab[i].offset == offset)) {
            /* for duplicated record, reuse previous entry.
             * This may happen when msync returns ASYNC_AGAIN when first checkin
             * record at offset 0 are overwritten after dropping data
             */
            pts_tab[i].pts = apts;
            if (debug_enable) {
                ALOGI("[%s:%d] checkin done,offset %zu,apts 0x%llx", __func__, __LINE__, offset, apts);
            }
            ret = 0;
            break;
        } else if (!pts_tab[i].valid) {
            pts_tab[i].pts = apts;
            pts_tab[i].offset = offset;
            pts_tab[i].valid = 1;
            if (debug_enable) {
                ALOGI("[%s:%d] checkin done,offset %zu,apts 0x%llx", __func__, __LINE__, offset, apts);
            }
            ret = 0;
            break;
        }
    }
    pthread_mutex_unlock(&g_mediasync_util_lock);
    return ret;
}

/*
for audio tunnel mode.the apts checkin align is based on
the audio payload size.
normally for Netflix case:
1)dd+ 768 byte per frame
2)LPCM from aac decoder, normally 4096 for LC AAC.8192 for SBR AAC
for the LPCM, the MS12 normally drain 6144 bytes per times which is
the each DD+ decoder output,we need align the size to 4096/8192 align
to checkout the apts.
*/
int aml_audio_mediasync_util_lookup_apts(audio_mediasync_util_t *p_hwsync, size_t offset, uint64_t *p_apts)
{
    int i = 0;
    size_t align  = 0;
    int ret = -1;
    struct aml_audio_device *adev = NULL;
    struct aml_stream_out  *out = NULL;
    int    debug_enable = 0;
    uint32_t latency_frames = 0;
    uint32_t latency_pts = 0;
    pts_tab_t *pts_tab = NULL;
    uint64_t nearest_pts = 0;
    uint32_t nearest_offset = 0;
    uint32_t min_offset = 0x7fffffff;
    int match_index = -1;

    // add protection to avoid NULL pointer.
    if (!p_hwsync) {
        ALOGE("%s null point", __func__);
        return -1;
    }

    debug_enable = p_hwsync->media_sync_debug;

    if (debug_enable) {
        ALOGI("%s offset %zu", __func__, offset);
    }
    pthread_mutex_lock(&g_mediasync_util_lock);

    align = offset;
    pts_tab = p_hwsync->pts_tab;
    for (i = 0; i < MEDIA_SYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid) {
            if (pts_tab[i].offset == align) {
                *p_apts = pts_tab[i].pts;
                nearest_offset = pts_tab[i].offset;
                ret = 0;
                if (debug_enable) {
                    ALOGI("%s pts checkout done,offset %zu,align %zu,pts 0x%llx",
                          __func__, offset, align, *p_apts);
                }
                break;
            } else if (pts_tab[i].offset < align) {
                /*find the nearest smaller apts*/
                if ((align - pts_tab[i].offset) < min_offset) {
                    min_offset = align - pts_tab[i].offset;
                    match_index = i;
                    nearest_pts = pts_tab[i].pts;
                    nearest_offset = pts_tab[i].offset;
                }
                pts_tab[i].valid = 0;
            }
        }
    }
    if (i == MEDIA_SYNC_APTS_NUM) {
        if (nearest_pts) {
            ret = 0;
            *p_apts = nearest_pts;
            /*keep it as valid, it may be used for next lookup*/
            pts_tab[match_index].valid = 1;
            if (debug_enable)
                ALOGI("find nearest smaller pts 0x%llx offset %u align %zu", *p_apts, nearest_offset, align);
        } else {
            ALOGE("%s,apts lookup failed,align %zu,offset %zu", __func__, align, offset);
        }
    }
    pthread_mutex_unlock(&g_mediasync_util_lock);
    return ret;
}
