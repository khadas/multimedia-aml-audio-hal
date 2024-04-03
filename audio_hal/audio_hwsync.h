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



#ifndef _AUDIO_HWSYNC_H_
#define _AUDIO_HWSYNC_H_

#include <stdbool.h>
#ifdef BUILD_LINUX
#include <pthread.h>
#endif

#ifdef USE_MSYNC
#include <aml_avsync.h>
#include <aml_avsync_log.h>
#else
#include "aml_avsync_stub.h"
#endif
#include "audio_mediasync_wrap.h"
#define SYSTIME_CORRECTION_THRESHOLD        (90000*10/100)
#define NSEC_PER_SECOND 1000000000ULL
#define HW_SYNC_STATE_HEADER 0
#define HW_SYNC_STATE_BODY   1
#define HW_SYNC_STATE_RESYNC 2
#define HW_SYNC_VERSION_SIZE 4

#define HW_AVSYNC_HEADER_SIZE_V1 16
#define HW_AVSYNC_HEADER_SIZE_V2 20
#define HW_AVSYNC_HEADER_SIZE_V3 36

/*head size is calculated with mOffset = ((int) Math.ceil(HEADER_V2_SIZE_BYTES / frameSizeInBytes)) * frameSizeInBytes;
 *current we only support to 8ch, the headsize is 32
 */
#define HW_AVSYNC_MAX_HEADER_SIZE  40  /*max 8ch */


//TODO: After precisely calc the pts, change it back to 1s
#define APTS_DISCONTINUE_THRESHOLD          (90000/10*11)
#define APTS_DISCONTINUE_THRESHOLD_MIN    (90000/1000*100)
#define APTS_DISCONTINUE_THRESHOLD_MIN_35MS    (90000/1000*35)
#define APTS_TSYNC_DROP_THRESHOLD_MIN_300MS    (90000/1000*300)
#define APTS_TSYNC_START_WAIT_THRESHOLD_US     (1*1000*1000)

#define APTS_DISCONTINUE_THRESHOLD_MAX    (5*90000)

#define HWSYNC_APTS_NUM     512
#define HWSYNC_MAX_BODY_SIZE  (65536)  ///< Will do fine tune according to the bitstream.
#define HWSYNC_PTS_EOS UINT64_MAX
#define HWSYNC_PTS_NA  (UINT64_MAX-1)

enum hwsync_status {
    CONTINUATION,  // good sync condition
    ADJUSTMENT,    // can be adjusted by discarding or padding data
    RESYNC,        // pts need resync
};

typedef struct apts_tab {
    int  valid;
    size_t offset;
    uint64_t pts;
} apts_tab_t;

typedef enum {
    ESSYNC_AUDIO_DROP   = 0,
    ESSYNC_AUDIO_OUTPUT = 1,
    ESSYNC_AUDIO_EXIT   = 2,
} sync_process_res;

typedef struct audio_mediasync {
    void* handle;
    int mediasync_id;
    int64_t cur_outapts;
    int64_t out_start_apts;
    struct mediasync_audio_policy apolicy;
    int64_t in_apts;
}audio_mediasync_t;//for AudioProcess mode

typedef struct audio_msync {
    void *msync_session;
    pthread_mutex_t msync_mutex;
    pthread_cond_t msync_cond;
    bool msync_start;
    avs_audio_action msync_action;
    int msync_action_delta;
    uint32_t msync_rendered_pts;
    bool first_apts_flag;//flag to indicate set first apts
    bool msync_first_insert_flag;
    uint64_t first_apts;  //record and print
}audio_msync_t;//for AudioProcess mode

typedef enum {
    AV_SYNC_AUDIO_UNKNOWN = 0,
    AV_SYNC_AUDIO_NORMAL_OUTPUT,
    AV_SYNC_AUDIO_DROP_PCM,
    AV_SYNC_AUDIO_INSERT,
    AV_SYNC_AUDIO_HOLD,
    AV_SYNC_AUDIO_MUTE,
    AV_SYNC_AUDIO_RESAMPLE,
    AV_SYNC_AUDIO_ADJUST_CLOCK,
} av_sync_policy_e;

typedef struct audio_avsync {
    audio_mediasync_t *mediasync_ctx;
    audio_msync_t *msync_ctx;
    uint64_t last_lookup_apts;
    uint64_t last_dec_out_frame;
    uint64_t last_output_apts;
    apts_tab_t pts_tab[HWSYNC_APTS_NUM];
    size_t payload_offset;
    int (*get_tuning_latency)(struct audio_stream_out *stream);
    pthread_mutex_t lock;
}avsync_ctx_t;

typedef struct  audio_header {
/* header */
    uint8_t hw_sync_header[HW_AVSYNC_MAX_HEADER_SIZE];
    size_t hw_sync_header_cnt;
    int hw_sync_state;
    uint32_t hw_sync_body_cnt;
    uint32_t hw_sync_frame_size;
    int      bvariable_frame_size;   //toD
    uint8_t hw_sync_body_buf[HWSYNC_MAX_BODY_SIZE];
    uint8_t body_align_cnt;    //toD
    int version_num;
    uint64_t last_apts_from_header;
    uint64_t last_apts_from_header_raw;
    bool eos;
    uint64_t clip_front;
    uint64_t clip_back;
    pthread_mutex_t lock;
} audio_header_t;

static inline bool hwsync_header_valid(uint8_t *header)
{
    return (header[0] == 0x55) &&
           (header[1] == 0x55) &&
           (header[2] == 0x00 || header[2] == 0x01) &&
           (header[3] == 0x01 || header[3] == 0x02 || header[3] == 0x03);
}

static inline uint64_t hwsync_header_get_pts(uint8_t *header)
{
    return (((uint64_t)header[8]) << 56) |
           (((uint64_t)header[9]) << 48) |
           (((uint64_t)header[10]) << 40) |
           (((uint64_t)header[11]) << 32) |
           (((uint64_t)header[12]) << 24) |
           (((uint64_t)header[13]) << 16) |
           (((uint64_t)header[14]) << 8) |
           ((uint64_t)header[15]);
}

static inline uint32_t hwsync_header_get_size(uint8_t *header)
{
    return (((uint32_t)header[4]) << 24) |
           (((uint32_t)header[5]) << 16) |
           (((uint32_t)header[6]) << 8) |
           ((uint32_t)header[7]);
}

static inline uint32_t hwsync_header_get_offset(uint8_t *header)
{
    return (((uint32_t)header[16]) << 24) |
           (((uint32_t)header[17]) << 16) |
           (((uint32_t)header[18]) << 8) |
           ((uint32_t)header[19]);
}

static inline char hwsync_header_get_sub_version(uint8_t *header)
{
    return header[2];
}

static inline uint64_t hwsync_header_get_clip_front(uint8_t *header)
{
    return (((uint64_t)header[20]) << 56) |
           (((uint64_t)header[21]) << 48) |
           (((uint64_t)header[22]) << 40) |
           (((uint64_t)header[23]) << 32) |
           (((uint64_t)header[24]) << 24) |
           (((uint64_t)header[25]) << 16) |
           (((uint64_t)header[26]) << 8) |
           ((uint64_t)header[27]);
}
static inline uint64_t hwsync_header_get_clip_back(uint8_t *header)
{
    return (((uint64_t)header[28]) << 56) |
           (((uint64_t)header[29]) << 48) |
           (((uint64_t)header[30]) << 40) |
           (((uint64_t)header[31]) << 32) |
           (((uint64_t)header[32]) << 24) |
           (((uint64_t)header[33]) << 16) |
           (((uint64_t)header[34]) << 8) |
           ((uint64_t)header[35]);
}

static inline uint64_t get_pts_gap(uint64_t a, uint64_t b)
{
    if (a >= b) {
        return (a - b);
    } else {
        return (b - a);
    }
}

audio_header_t *audio_header_info_init(void);
void audio_header_info_reset(audio_header_t *p_header);
int audio_header_find_frame(audio_header_t *p_header,
        const void *in_buffer, size_t in_bytes,
        uint64_t *cur_pts, int *outsize);

void avsync_ctx_reset(avsync_ctx_t *avsync_ctx);
avsync_ctx_t *avsync_ctx_init(void);
audio_mediasync_t *mediasync_ctx_init(void);
audio_msync_t *msync_ctx_init(void);
void avsync_reset_apts_tbl(avsync_ctx_t *avsync_ctx);
int avsync_checkin_apts(avsync_ctx_t *avsync_ctx, size_t offset, uint64_t apts);
int avsync_lookup_apts(avsync_ctx_t *avsync_ctx, size_t offset, uint64_t *p_apts);
void msync_unblock_start(audio_msync_t *msync_ctx);
int msync_set_first_pts(audio_msync_t *msync_ctx, uint32_t pts);
bool skip_check_when_gap(struct audio_stream_out *stream, size_t offset, uint64_t apts);
int msync_get_policy(struct audio_stream_out *stream, uint64_t apts);
av_sync_policy_e get_and_map_avsync_policy(avsync_ctx_t *avsync_ctx, int avsync_type);
#endif
