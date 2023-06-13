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

#ifndef _AUDIO_MEDIA_SYNC_UTIL_H_
#define _AUDIO_MEDIA_SYNC_UTIL_H_

#include <stdbool.h>
#include <pthread.h>

#define MEDIA_SYNC_APTS_NUM     512

typedef struct pts_tab {
    int  valid;
    size_t offset;
    uint64_t pts;
} pts_tab_t;

typedef struct  audio_mediasync_util {
    pts_tab_t pts_tab[MEDIA_SYNC_APTS_NUM];
    size_t payload_offset;
    //struct aml_stream_out  *aout;

    uint64_t last_lookup_apts;
    unsigned int same_pts_data_size;
    bool record_flag;
    uint64_t es_last_apts;
    int media_sync_debug;

    //do insert policy param
    struct timespec start_insert_time;
    int insert_time_ms;
} audio_mediasync_util_t;

audio_mediasync_util_t* aml_audio_mediasync_util_init();
audio_mediasync_util_t* aml_audio_get_mediasync_util_handle();
int aml_audio_mediasync_util_checkin_apts(audio_mediasync_util_t *p_hwsync, size_t offset, uint64_t apts);
int aml_audio_mediasync_util_lookup_apts(audio_mediasync_util_t *p_hwsync, size_t offset, uint64_t *p_apts);

#endif
