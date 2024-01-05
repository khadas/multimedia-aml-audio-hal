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

#define NEW_MEDIASYNC

#ifdef NEW_MEDIASYNC

static void aml_hwsync_wrap_single_set_tsync_init(void)
{
    ALOGI("%s(), send tsync enable", __func__);
    sysfs_set_sysfs_str(TSYNC_ENABLE, "1"); // enable avsync
    sysfs_set_sysfs_str(TSYNC_MODE, "1"); // enable avsync
}

static void aml_hwsync_wrap_single_set_tsync_pause(void)
{
    ALOGI("%s(), send pause event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_PAUSE");
}

static void aml_hwsync_wrap_single_set_tsync_resume(void)
{
    ALOGI("%s(), send resuem event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_RESUME");
}

static void aml_hwsync_wrap_single_set_tsync_stop(void)
{
    ALOGI("%s(), send stop event and disable tsync", __func__);

    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_RESUME");
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_STOP");
    sysfs_set_sysfs_str(TSYNC_ENABLE, "0"); // disable avsync
}

static int aml_hwsync_wrap_single_set_tsync_start_pts(uint32_t pts)
{
    char buf[64] = {0};

    snprintf(buf, 64, "AUDIO_START:0x%x", pts);
    ALOGI("tsync -> %s", buf);
    return sysfs_set_sysfs_str(TSYNC_EVENT, buf);
}

static int aml_hwsync_wrap_single_set_tsync_start_pts64(uint64_t pts)
{
    char buf[64] = {0};
    sprintf (buf, "AUDIO_START:0x%"PRIx64"", pts & 0xffffffff);
    ALOGI("tsync -> %s", buf);
    return sysfs_set_sysfs_str(TSYNC_EVENT, buf);
}


static int aml_hwsync_wrap_single_get_tsync_pts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_PCRSCR, pts);
}

static int aml_hwsync_wrap_single_get_tsync_apts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_APTS, pts);
}


static int aml_hwsync_wrap_single_get_tsync_vpts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_VPTS, pts);
}

static int aml_hwsync_wrap_single_get_tsync_firstvpts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_FIRSTVPTS, pts);
}

static int aml_hwsync_wrap_single_get_tsync_video_started(uint32_t *video_started)
{
    if (!video_started) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_VSTARTED, video_started);
}

static int aml_hwsync_wrap_single_reset_tsync_pcrscr(uint32_t pts)
{
    char buf[64] = {0};

    snprintf(buf, 64, "0x%x", pts);
    ALOGI("tsync -> reset pcrscr 0x%x", pts);
    return sysfs_set_sysfs_str(TSYNC_APTS, buf);
}

void* aml_hwsync_wrap_mediasync_create (void) {
    return mediasync_wrap_create();
}

void aml_hwsync_wrap_set_tsync_init(audio_hwsync_t *p_hwsync)
{
    ALOGI("%s(), send tsync enable", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_set_tsync_init();
    }
}

void aml_hwsync_wrap_set_tsync_pause(audio_hwsync_t *p_hwsync)
{
    ALOGI("%s(), send pause event", __func__);
    if (!p_hwsync || (p_hwsync && !p_hwsync->use_mediasync)) {
        return aml_hwsync_wrap_single_set_tsync_pause();
    }
    mediasync_wrap_setPause(p_hwsync->es_mediasync.mediasync, true);
}

void aml_hwsync_wrap_set_tsync_resume(audio_hwsync_t *p_hwsync)
{
    ALOGI("%s(), send resuem event", __func__);
    bool ret = false;
    sync_mode mode = MEDIA_SYNC_MODE_MAX;
    if (!p_hwsync || (p_hwsync && !p_hwsync->use_mediasync)) {
        return aml_hwsync_wrap_single_set_tsync_resume();
    }
    mediasync_wrap_setPause(p_hwsync->es_mediasync.mediasync, false);
}

void aml_hwsync_wrap_set_tsync_stop(audio_hwsync_t *p_hwsync)
{
    ALOGI("%s(), send stop event", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_set_tsync_stop();
    }
    mediasync_wrap_clearAnchor(p_hwsync->es_mediasync.mediasync);
}

int aml_hwsync_wrap_set_tsync_start_pts(audio_hwsync_t *p_hwsync, uint32_t pts)
{
    ALOGI("%s(), set tsync start pts: %d", __func__, pts);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_set_tsync_start_pts(pts);
    }
    int64_t timeus = ((int64_t)pts) / 90 *1000;
    mediasync_wrap_setStartingTimeMedia(p_hwsync->es_mediasync.mediasync, timeus);
    return 0;
}

int aml_hwsync_wrap_set_tsync_start_pts64(audio_hwsync_t *p_hwsync, uint64_t pts)
{
    ALOGI("%s(), set tsync start pts64: %" PRId64 "", __func__, pts);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_set_tsync_start_pts64(pts);
    }
    int64_t timeus = pts / 90 *1000;
    mediasync_wrap_setStartingTimeMedia(p_hwsync->es_mediasync.mediasync, timeus);
    return 0;
}


int aml_hwsync_wrap_get_tsync_pts(audio_hwsync_t *p_hwsync, uint32_t *pts)
{
    int64_t timeus = 0;
    ALOGV("%s(), get tsync pts", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_get_tsync_pts(pts);
    }
    mediasync_wrap_getMediaTime(p_hwsync->es_mediasync.mediasync, systemTime(SYSTEM_TIME_MONOTONIC) / 1000LL, &timeus, 0);
    *pts = timeus / 1000 * 90;
    return 0;
}

int aml_hwsync_wrap_get_tsync_apts(audio_hwsync_t *p_hwsync, uint32_t *pts)
{
    int64_t timeus = 0;
    ALOGV("%s(), get tsync apts", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_get_tsync_apts(pts);
    }
    /*To do*/
    (void)p_hwsync;
    (void)pts;
    return 0;
}

int aml_hwsync_wrap_get_tsync_vpts(audio_hwsync_t *p_hwsync, uint32_t *pts)
{
    ALOGI("%s(), [To do ]get tsync vpts", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_get_tsync_vpts(pts);
    }
    /*To do*/
    (void)p_hwsync;
    (void)pts;
    return 0;
}

int aml_hwsync_wrap_get_tsync_firstvpts(audio_hwsync_t *p_hwsync, uint32_t *pts)
{
    ALOGI("%s(), [To do ]get tsync firstvpts", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_get_tsync_firstvpts(pts);
    }
    /*To do*/
    (void)p_hwsync;
    (void)pts;
    return 0;
}

int aml_hwsync_wrap_get_tsync_video_started(audio_hwsync_t *p_hwsync, uint32_t *video_started)
{
    ALOGI("%s(), [To do ]get tsync video started", __func__);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_get_tsync_video_started(video_started);
    }

    return 0;
}

int aml_hwsync_wrap_reset_tsync_pcrscr(audio_hwsync_t *p_hwsync, uint32_t pts)
{
    ALOGV("%s(), reset tsync pcr: %d", __func__, pts);
    if (!p_hwsync->use_mediasync) {
        return aml_hwsync_wrap_single_reset_tsync_pcrscr(pts);
    }
    int64_t timeus = ((int64_t)pts) / 90 *1000;
    mediasync_wrap_updateAnchor(p_hwsync->es_mediasync.mediasync, timeus, 0, 0);

    return 0;
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


void aml_hwsync_wrap_wait_video_start(audio_hwsync_t *p_hwsync, uint32_t wait_count)
{
    bool ret = false;
    int count = 0;
    int64_t outMediaUs = -1;
    sync_mode mode = MEDIA_SYNC_MODE_MAX;

    if (!p_hwsync->es_mediasync.mediasync) {
        return;
     }
    ret = mediasync_wrap_getSyncMode(p_hwsync->es_mediasync.mediasync, &mode);
    if (!ret || (mode != MEDIA_SYNC_VMASTER)) {
        return;
    }

    ret = mediasync_wrap_getTrackMediaTime(p_hwsync->es_mediasync.mediasync, &outMediaUs);
    if (!ret) {
        ALOGI("mediasync_wrap_getTrackMediaTime error");
        return;
    }
    ALOGI("start sync with video %lld", outMediaUs);
    if (outMediaUs <= 0) {
        ALOGI("wait video start");
        while (count < wait_count) {
            usleep(20000);
            ret = mediasync_wrap_getTrackMediaTime(p_hwsync->es_mediasync.mediasync, &outMediaUs);
            if (!ret) {
                return;
            }
            if (outMediaUs > 0) {
                break;
            }
            count++;
        }
    }
    ALOGI("video start");
    return;
}

void aml_hwsync_wrap_wait_video_drop(audio_hwsync_t *p_hwsync, uint32_t cur_pts, uint32_t wait_count)
{
    bool ret = false;
    int count = 0;
    int64_t nowUs;
    int64_t outRealMediaUs;
    int64_t outMediaPts;
    int64_t audio_cur_pts = 0;
    sync_mode mode = MEDIA_SYNC_MODE_MAX;

    if (!p_hwsync->es_mediasync.mediasync) {
        return;
    }
    ret = mediasync_wrap_getSyncMode(p_hwsync->es_mediasync.mediasync, &mode);

    nowUs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000LL;
    ret = mediasync_wrap_getMediaTime(p_hwsync->es_mediasync.mediasync, nowUs,
                                    &outRealMediaUs, false);
    if (!ret) {
        return;
    }
    outMediaPts = outRealMediaUs / 1000LL * 90;
    audio_cur_pts = (int64_t)cur_pts;
    ALOGI("====================, now audiopts %lld vpts %lld ", audio_cur_pts, outMediaPts);
    if ((audio_cur_pts - outMediaPts) > SYSTIME_CORRECTION_THRESHOLD) {
        bool ispause = false;
        bool ret = mediasync_wrap_getPause(p_hwsync->es_mediasync.mediasync, &ispause);
        if (ret && ispause) {
            mediasync_wrap_setPause(p_hwsync->es_mediasync.mediasync, false);
        }
        count = 0;
        while (count < wait_count) {
            int64_t nowUs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000LL;
            ret = mediasync_wrap_getMediaTime(p_hwsync->es_mediasync.mediasync, nowUs,
                                    &outRealMediaUs, false);
            if (!ret) {
                return;
            }
            outMediaPts = outRealMediaUs / 1000LL * 90;
            if ((audio_cur_pts - outMediaPts) <= SYSTIME_CORRECTION_THRESHOLD)
                break;
            usleep(20000);
            count++;
            ALOGI("fisrt audio wait video %d ms,now audiopts %lld vpts %" PRId64 " ", count * 20, audio_cur_pts, outMediaPts);
        }
    } else {
        bool ispause = false;
        bool ret = mediasync_wrap_getPause(p_hwsync->es_mediasync.mediasync, &ispause);
        if (ret && ispause) {
            mediasync_wrap_setPause(p_hwsync->es_mediasync.mediasync, false);
        }
    }
    mediasync_wrap_setSyncMode(p_hwsync->es_mediasync.mediasync, MEDIA_SYNC_AMASTER);

    return;
}



#else
void aml_hwsync_wrap_set_tsync_init(void)
{
    ALOGI("%s(), send tsync enable", __func__);
    sysfs_set_sysfs_str(TSYNC_ENABLE, "1"); // enable avsync
    sysfs_set_sysfs_str(TSYNC_MODE, "1"); // enable avsync
}

void aml_hwsync_wrap_set_tsync_pause(void)
{
    ALOGI("%s(), send pause event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_PAUSE");
}

void aml_hwsync_wrap_set_tsync_resume(void)
{
    ALOGI("%s(), send resuem event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_RESUME");
}

void aml_hwsync_wrap_set_tsync_stop(void)
{
    ALOGI("%s(), send stop event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_STOP");
}

int aml_hwsync_wrap_set_tsync_start_pts(uint32_t pts)
{
    char buf[64] = {0};

    snprintf(buf, 64, "AUDIO_START:0x%x", pts);
    ALOGI("tsync -> %s", buf);
    return sysfs_set_sysfs_str(TSYNC_EVENT, buf);
}

int aml_hwsync_wrap_set_tsync_start_pts64(uint64_t pts)
{
    char buf[64] = {0};
    sprintf (buf, "AUDIO_START:0x%"PRIx64"", pts & 0xffffffff);
    ALOGI("tsync -> %s", buf);
    return sysfs_set_sysfs_str(TSYNC_EVENT, buf);
}


int aml_hwsync_wrap_get_tsync_pts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_PCRSCR, pts);
}

int aml_hwsync_wrap_get_tsync_vpts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_VPTS, pts);
}

int aml_hwsync_wrap_get_tsync_firstvpts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_FIRSTVPTS, pts);
}

int aml_hwsync_wrap_reset_tsync_pcrscr(uint32_t pts)
{
    char buf[64] = {0};

    snprintf(buf, 64, "0x%x", pts);
    ALOGV("tsync -> reset pcrscr 0x%x", pts);
    return sysfs_set_sysfs_str(TSYNC_APTS, buf);
}
#endif
