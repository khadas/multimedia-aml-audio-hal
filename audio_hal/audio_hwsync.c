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
#include <sys/utsname.h>
#include <cutils/properties.h>
#include "audio_hw_utils.h"
#include "audio_hwsync.h"
#include "audio_hw.h"
#include "dolby_lib_api.h"
#include "aml_audio_ms12_sync.h"
#include "aml_audio_timer.h"
#include "audio_hw_ms12_v2.h"

#include "aml_android_utils.h"
#define MSYNC_CALLBACK_WAIT_TIMEOUT_US (4000*1000)

static int aml_audio_get_hwsync_flag()
{
    int debug_flag = 0;
    debug_flag = aml_audio_property_get_int("vendor.media.audio.hal.hwsync", debug_flag);
    return debug_flag;
}

void audio_header_info_reset(audio_header_t *p_header)
{
    AM_LOGI("p_header %p", p_header);
    int fd = -1;
    if (p_header == NULL) {
        return;
    }
    p_header->hw_sync_state = HW_SYNC_STATE_HEADER;
    p_header->hw_sync_header_cnt = 0;
    p_header->hw_sync_frame_size = 0;
    p_header->bvariable_frame_size = 0;
    p_header->version_num = 0;
    p_header->eos = false;

    AM_LOGI("done");
    return;
}

audio_header_t *audio_header_info_init(void)
{
    audio_header_t *p_header = aml_audio_calloc(1, sizeof(audio_header_t));
    if (NULL == p_header) {
        AM_LOGE("malloc pheader failed, size(%zu)", sizeof(audio_header_t));
        return NULL;
    }
    audio_header_info_reset(p_header);
    return p_header;
}

//return bytes cost from input,
int audio_header_find_frame(audio_header_t *p_header,
        const void *in_buffer, size_t in_bytes, uint64_t *cur_pts, int *outsize)
{
    size_t remain = in_bytes;
    uint8_t *p = (uint8_t *)in_buffer;
    uint64_t time_diff = 0;
    int pts_found = 0;
    size_t  v2_hwsync_header = HW_AVSYNC_HEADER_SIZE_V2;
    int debug_enable = aml_audio_get_hwsync_flag();
    int header_sub_version = 0;
    if (p_header == NULL || in_buffer == NULL) {
        return 0;
    }

    while (remain > 0) {
        if (p_header->hw_sync_state == HW_SYNC_STATE_HEADER) {
            p_header->hw_sync_header[p_header->hw_sync_header_cnt++] = *p++;
            remain--;

            if (p_header->hw_sync_header_cnt == HW_SYNC_VERSION_SIZE) {
                if (!hwsync_header_valid(&p_header->hw_sync_header[0])) {
                    p_header->hw_sync_state = HW_SYNC_STATE_HEADER;
                    memcpy(p_header->hw_sync_header, p_header->hw_sync_header + 1, HW_SYNC_VERSION_SIZE - 1);
                    p_header->hw_sync_header_cnt--;
                    continue;
                }

                p_header->version_num = p_header->hw_sync_header[3];
                if (p_header->version_num != 1 && p_header->version_num != 2 && p_header->version_num != 3) {
                    ALOGI("invalid pheader version num %d", p_header->version_num);
                }
            }

            if ((p_header->version_num == 1 && p_header->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V1) ||
                (p_header->version_num == 2 && p_header->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V2) ||
                (p_header->version_num == 3 && p_header->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V3)) {

                if (p_header->version_num == 2 && p_header->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V2) {
                    v2_hwsync_header = hwsync_header_get_offset(&p_header->hw_sync_header[0]);
                    if (v2_hwsync_header > HW_AVSYNC_MAX_HEADER_SIZE) {
                        ALOGE("buffer overwrite, check the header size %zu \n", v2_hwsync_header);
                        break;
                    }
                    if (v2_hwsync_header > p_header->hw_sync_header_cnt) {
                        ALOGV("need skip more sync header, %zu\n",v2_hwsync_header);
                        continue;
                    }
                }

                if ((3 == p_header->version_num) && (HW_AVSYNC_HEADER_SIZE_V3 == p_header->hw_sync_header_cnt)) {
                    p_header->clip_front = hwsync_header_get_clip_front(p_header->hw_sync_header);
                    p_header->clip_back  = hwsync_header_get_clip_back(p_header->hw_sync_header);
                    AM_LOGI_IF(debug_enable, "clip_front:%"PRIu64", clip_back:%"PRIu64"", p_header->clip_front, p_header->clip_back);
                }

                if ((in_bytes - remain) > p_header->hw_sync_header_cnt) {
                    ALOGI("got the frame sync header cost %zu", in_bytes - remain);
                }

                p_header->hw_sync_body_cnt = hwsync_header_get_size(&p_header->hw_sync_header[0]);
                if (p_header->hw_sync_body_cnt != 0) {
                    p_header->hw_sync_state = HW_SYNC_STATE_BODY;
                }

                if (p_header->hw_sync_body_cnt > HWSYNC_MAX_BODY_SIZE) {
                    ALOGE("body max size =%d hw_sync_body_cnt =%d", HWSYNC_MAX_BODY_SIZE, p_header->hw_sync_body_cnt);
                    p_header->hw_sync_body_cnt = HWSYNC_MAX_BODY_SIZE;
                }

                if (p_header->hw_sync_frame_size && p_header->hw_sync_body_cnt) {
                    if (p_header->hw_sync_frame_size != p_header->hw_sync_body_cnt) {
                        p_header->bvariable_frame_size = 1;
                        ALOGV("old frame size=%d new=%d", p_header->hw_sync_frame_size, p_header->hw_sync_body_cnt);
                    }
                }
                p_header->hw_sync_frame_size = p_header->hw_sync_body_cnt;
                p_header->body_align_cnt = 0; //  alisan zz
                p_header->hw_sync_header_cnt = 0; //8.1

                uint64_t pts = 0, pts_raw = 0;
                pts_raw = pts = hwsync_header_get_pts(&p_header->hw_sync_header[0]);
                if (pts == HWSYNC_PTS_EOS) {
                    p_header->eos = true;
                    ALOGI("%s: eos detected!", __FUNCTION__);
                    continue;
                }
                header_sub_version = p_header->hw_sync_header[2];
                if (!header_sub_version)
                    pts = pts * 90 / 1000000;
                time_diff = get_pts_gap(pts, p_header->last_apts_from_header) / 90;
                if (debug_enable) {
                    ALOGI("pts 0x%"PRIx64",frame len %"PRIu32"\n", pts, p_header->hw_sync_body_cnt);
                    ALOGI("last pts 0x%"PRIx64",diff %"PRIu64" ms\n", p_header->last_apts_from_header, time_diff);
                }
                if (p_header->hw_sync_frame_size > HWSYNC_MAX_BODY_SIZE) {
                    ALOGE("pheader frame body %d bigger than pre-defined size %d, need check !!!!!\n",
                                p_header->hw_sync_frame_size,HWSYNC_MAX_BODY_SIZE);
                    p_header->hw_sync_state = HW_SYNC_STATE_HEADER;
                    return 0;
                }

                if (time_diff > 32) {
                    ALOGV("pts  time gap %"PRIx64" ms,last %"PRIx64",cur %"PRIx64"\n", time_diff,
                          p_header->last_apts_from_header, pts);
                }
                p_header->last_apts_from_header = pts;
                p_header->last_apts_from_header_raw = pts_raw;
                *cur_pts = (pts_raw == HWSYNC_PTS_NA) ? HWSYNC_PTS_NA : pts;
                pts_found = 1;
            }
            continue;
        } else if (p_header->hw_sync_state == HW_SYNC_STATE_BODY) {
            int m = (p_header->hw_sync_body_cnt < remain) ? p_header->hw_sync_body_cnt : remain;
            // process m bytes body with an empty fragment for alignment
            if (m > 0) {
                memcpy(p_header->hw_sync_body_buf + p_header->hw_sync_frame_size - p_header->hw_sync_body_cnt, p, m);
                p += m;
                remain -= m;
                p_header->hw_sync_body_cnt -= m;
                if (p_header->hw_sync_body_cnt == 0) {
                    p_header->hw_sync_state = HW_SYNC_STATE_HEADER;
                    p_header->hw_sync_header_cnt = 0;
                    *outsize = p_header->hw_sync_frame_size;
                    /*
                    sometimes the audioflinger burst size is smaller than pheader payload
                    we need use the last found pts when got a complete pheader payload
                    */
                    if (!pts_found) {
                        *cur_pts = (p_header->last_apts_from_header_raw == HWSYNC_PTS_NA) ?
                                    HWSYNC_PTS_NA : p_header->last_apts_from_header;

                    }
                    if (debug_enable) {
                        ALOGV("we found the frame total body,yeah\n");
                    }
                    break;//continue;
                }
            }
        }
    }
    return in_bytes - remain;
}

avsync_ctx_t *avsync_ctx_init(void)
{
    AM_LOGI("<in>");
    avsync_ctx_t *avsync_ctx = aml_audio_calloc(1, sizeof(avsync_ctx_t));
    if (NULL == avsync_ctx)
    {
        AM_LOGI("calloc size:%zu, error!", sizeof(avsync_ctx_t));
        return NULL;
    }

    avsync_ctx->mediasync_ctx      = NULL;
    avsync_ctx->msync_ctx          = NULL;
    avsync_ctx->last_lookup_apts   = -1;
    avsync_ctx->last_dec_out_frame = 0;
    avsync_ctx->last_output_apts   = -1;
    avsync_ctx->get_tuning_latency = NULL;
    avsync_ctx->payload_offset     = 0;
    pthread_mutex_init(&avsync_ctx->lock, NULL);
    memset(avsync_ctx->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);

    AM_LOGI("<out>");
    return avsync_ctx;
}

void avsync_ctx_reset(avsync_ctx_t *avsync_ctx)
{
    if (NULL == avsync_ctx) {
        return;
    }
    pthread_mutex_lock(&(avsync_ctx->lock));
    avsync_ctx->last_lookup_apts   = -1;
    avsync_ctx->last_dec_out_frame = 0;
    avsync_ctx->last_output_apts   = -1;
    avsync_ctx->get_tuning_latency = NULL;
    avsync_ctx->payload_offset     = 0;
    memset(avsync_ctx->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);

    if (NULL != avsync_ctx->mediasync_ctx) {
        audio_mediasync_t *mediasync_ctx = avsync_ctx->mediasync_ctx;
        mediasync_ctx->mediasync_id   = -1;
        mediasync_ctx->cur_outapts    = -1;
        mediasync_ctx->out_start_apts = -1;
        mediasync_ctx->in_apts        = -1;
        mediasync_ctx->apolicy.audiopolicy = MEDIASYNC_AUDIO_UNKNOWN;
    }

    if (NULL != avsync_ctx->msync_ctx) {
        audio_msync_t *msync_ctx = avsync_ctx->msync_ctx;
        msync_ctx->msync_start = false;
        msync_ctx->msync_action = AV_SYNC_AA_RENDER;
        msync_ctx->msync_action_delta = -1;
        msync_ctx->msync_rendered_pts = -1;
        msync_ctx->first_apts_flag  = false;//flag to indicate set first apts
        msync_ctx->msync_first_insert_flag = false;
        msync_ctx->first_apts = -1;  //record and print
    }
    pthread_mutex_unlock(&(avsync_ctx->lock));
    return;
}

audio_mediasync_t *mediasync_ctx_init(void)
{
    AM_LOGI("<in>");
    audio_mediasync_t *mediasync_ctx = aml_audio_calloc(1, sizeof(audio_mediasync_t));
    if (NULL == mediasync_ctx)
    {
        AM_LOGI("calloc size:%zu, error!", sizeof(audio_mediasync_t));
        return NULL;
    }

    mediasync_ctx->handle         = NULL;
    mediasync_ctx->mediasync_id   = -1;
    mediasync_ctx->cur_outapts    = -1;
    mediasync_ctx->out_start_apts = -1;
    mediasync_ctx->in_apts        = -1;
    mediasync_ctx->apolicy.audiopolicy = MEDIASYNC_AUDIO_UNKNOWN;
    AM_LOGI("<out>");
    return mediasync_ctx;
}

audio_msync_t *msync_ctx_init(void)
{
    AM_LOGI("<in>");
    audio_msync_t *msync_ctx = aml_audio_calloc(1, sizeof(audio_msync_t));
    if (NULL == msync_ctx)
    {
        AM_LOGI("calloc size:%zu, error!", sizeof(audio_msync_t));
        return NULL;
    }

    msync_ctx->msync_session = NULL;
    msync_ctx->msync_start = false;
    msync_ctx->msync_action = AV_SYNC_AA_RENDER;
    msync_ctx->msync_action_delta = -1;
    msync_ctx->msync_rendered_pts = -1;
    msync_ctx->first_apts_flag  = false;//flag to indicate set first apts
    msync_ctx->msync_first_insert_flag = false;
    msync_ctx->first_apts = -1;  //record and print
    pthread_mutex_init(&(msync_ctx->msync_mutex), NULL);
    pthread_cond_init(&(msync_ctx->msync_cond), NULL);

    AM_LOGI("<out>");
    return msync_ctx;
}

void msync_unblock_start(audio_msync_t *msync_ctx)
{
    pthread_mutex_lock(&(msync_ctx->msync_mutex));
    msync_ctx->msync_start = true;
    pthread_cond_signal(&(msync_ctx->msync_cond));
    pthread_mutex_unlock(&(msync_ctx->msync_mutex));
}

static int msync_start_callback(void *priv, avs_ascb_reason reason)
{
    ALOGI("msync_callback reason %d", reason);
    msync_unblock_start((audio_msync_t *)priv);
    return 0;
}

static inline uint32_t abs32(int32_t a)
{
    return ((a) < 0 ? -(a) : (a));
}

#define pts_gap(a, b)   abs32(((int)(a) - (int)(b)))

bool skip_check_when_gap(struct audio_stream_out *stream, size_t offset, uint64_t apts)
{
    struct aml_stream_out *aout   = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aout->dev;

    if ((adev->gap_offset != 0)
         && (!adev->gap_ignore_pts)
         && ((int)(offset - (adev->gap_offset & 0xffffffff)) >= 0)) {
        /* when PTS gap exists (Netflix specific), skip APTS reset between [adev->gap_pts, adev->gap_pts + 500ms] */
        AM_LOGI("gap_pts = 0x%"PRIu64"", apts);
        adev->gap_pts = apts;
        adev->gap_ignore_pts = true;
        set_ms12_main_audio_mute(&adev->ms12, true, 32);
    }

    if (adev->gap_ignore_pts) {
        if (pts_gap(apts, adev->gap_pts) < 90 * 500) {
            AM_LOGI("gap_pts = 0x%"PRIu64", adev->gap_pts:0x%x, skip", apts, adev->gap_pts);
            return true;
        } else {
            AM_LOGI("gap process done, recovered APTS/PCR checking");
            adev->gap_ignore_pts = false;
            adev->gap_offset = 0;
            adev->gap_passthrough_state = GAP_PASSTHROUGH_STATE_IDLE;
            if (true != adev->parental_control_av_mute) {
                set_ms12_main_audio_mute(&adev->ms12, false, 32);
            }
        }
    }

    return false;
}

int msync_set_first_pts(audio_msync_t *msync_ctx, uint32_t pts)
{
    if ((NULL == msync_ctx) || (NULL == msync_ctx->msync_session)) {
        AM_LOGE("msync_ctx:%p error!", msync_ctx);
        return -1;
    }

    msync_ctx->first_apts = pts;

    if (msync_ctx->msync_session) {
        int r = av_sync_audio_start(msync_ctx->msync_session, pts, 0,
                                   msync_start_callback, msync_ctx);
        if (r == AV_SYNC_ASTART_SYNC) {
            ALOGI("MSYNC AV_SYNC_ASTART_SYNC");
            msync_ctx->msync_action = AV_SYNC_AA_RENDER;
            msync_ctx->first_apts_flag = true;
        } else if (r == AV_SYNC_ASTART_ASYNC) {
            struct timespec ts;
            //ts_wait_time_us(&ts, MSYNC_CALLBACK_WAIT_TIMEOUT_US);
            pthread_mutex_lock(&(msync_ctx->msync_mutex));
            while (!msync_ctx->msync_start) {
                ALOGI("%s wait %d ms", __func__, MSYNC_CALLBACK_WAIT_TIMEOUT_US/1000);
                pthread_cond_timedwait(&(msync_ctx->msync_cond), &(msync_ctx->msync_mutex), &ts);
                msync_ctx->msync_start = true;
            }
            pthread_mutex_unlock(&(msync_ctx->msync_mutex));
            ALOGI("MSYNC AV_SYNC_ASTART_ASYNC");
            msync_ctx->msync_action = AV_SYNC_AA_RENDER;
            msync_ctx->first_apts_flag = true;
        } else if (r == AV_SYNC_ASTART_AGAIN) {
            ALOGI("MSYNC AV_SYNC_ASTART_AGAIN");
            msync_ctx->msync_action = AV_SYNC_AA_DROP;
        }
    }

    return 0;
}

int msync_get_policy(struct audio_stream_out *stream, uint64_t apts)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    avsync_ctx_t *avsync_ctx = aml_out->avsync_ctx;
    int force_action = 0;

    if ((NULL == aml_out) || (NULL == avsync_ctx) || (NULL == avsync_ctx->msync_ctx)) {
        AM_LOGE("avsync_type:%d, avsync_ctx:%p", aml_out->avsync_type, avsync_ctx);
        return -1;
    }

    audio_msync_t *msync_ctx = avsync_ctx->msync_ctx;
    do {
        if (false == msync_ctx->first_apts_flag) {
            pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
            msync_set_first_pts(msync_ctx, (uint32_t)apts);
            if (AV_SYNC_AA_RENDER == msync_ctx->msync_action) {
                AM_LOGI("START MSYNC action switched to AA_INSERT First");
                msync_ctx->msync_action = AV_SYNC_AA_INSERT;
                msync_ctx->msync_first_insert_flag = true;
            }
            pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
            break;
        }

        if (true == aml_out->eos) {
            pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
            msync_ctx->msync_action = AV_SYNC_AA_RENDER;
            pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
            break;
        }

        struct audio_policy policy;
        policy.action = msync_ctx->msync_action;
        av_sync_audio_render(msync_ctx->msync_session, apts, &policy);
        force_action = aml_audio_property_get_int("media.audiohal.action", 0);
        /* 0: default, no force action
         * 1: AA_RENDER
         * 2: AA_DROP
         * 3: AA_INSERT
         */
        pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
        msync_ctx->msync_action_delta = policy.delta;
        if (force_action) {
            policy.action = force_action - 1;
            if (policy.action == AV_SYNC_AA_INSERT) {
                msync_ctx->msync_action_delta = -1;
            } else if (policy.action == AV_SYNC_AA_DROP) {
                msync_ctx->msync_action_delta = 1;
            }
        }

        if (msync_ctx->msync_action != policy.action) {
            /* msync_action_delta is > 0 for AV_SYNC_AA_DROP
             * and MS12 "-sync <positive>" for insert
             */
            if (policy.action == AV_SYNC_AA_INSERT) {
                AM_LOGI("MSYNC action switched to AA_INSERT.");
            } else if (policy.action == AV_SYNC_AA_DROP) {
                AM_LOGI("MSYNC action switched to AA_DROP.");
            } else {
                AM_LOGI("MSYNC action switched to AA_RENDER.");
            }
            msync_ctx->msync_action = policy.action;
            msync_ctx->msync_first_insert_flag = false;
        }
        pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));
    } while (0);
    return 0;
}

/**************************************************************************************
 * Function:    avsync_reset_apts_tbl
 *
 * Description: Reset the pts table and the payload offset
 *
 * Inputs:      valid audio avsync_ctx_t pointer
 *
 * Outputs:     NONE
 *
 * Return:      NONE
 *
 * Notes:       For dolby ms12 cleanup, the consumed payload offset by dolby ms12 SDK will reset too,
                so, the pts table and the payload need reset correspond.
 **************************************************************************************/
void avsync_reset_apts_tbl(avsync_ctx_t *avsync_ctx)
{
    if (NULL == avsync_ctx) {
        ALOGE("%s avsync_ctx null point", __func__);
        return;
    }

    pthread_mutex_lock(&avsync_ctx->lock);
    ALOGI("%s Reset, payload_offset %zu", __func__,
           avsync_ctx->payload_offset);

    /* reset payload offset and pts table */
    avsync_ctx->payload_offset = 0;
    memset(avsync_ctx->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);
    pthread_mutex_unlock(&avsync_ctx->lock);
    return;
}

int avsync_checkin_apts(avsync_ctx_t *avsync_ctx, size_t offset, uint64_t apts)
{
    int i = 0;
    int ret = -1;
    int debug_enable = aml_audio_get_hwsync_flag();
    apts_tab_t *pts_tab = NULL;
    if (!avsync_ctx) {
        ALOGE("%s null point", __func__);
        return -1;
    }
    if (debug_enable) {
        ALOGI("++ %s checkin ,offset %zu,apts 0x%"PRIx64"", __func__, offset, apts);
    }
    pthread_mutex_lock(&avsync_ctx->lock);
    pts_tab = avsync_ctx->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid && (pts_tab[i].offset == offset)) {
            /* for duplicated record, reuse previous entry.
             * This may happen when msync returns ASYNC_AGAIN when first checkin
             * record at offset 0 are overwritten after dropping data
             */
            pts_tab[i].pts = apts;
            if (debug_enable) {
                ALOGI("%s:%d checkin done,offset %zu,apts 0x%"PRIx64"", __func__, __LINE__, offset, apts);
            }
            ret = 0;
            break;
        } else if (!pts_tab[i].valid) {
            pts_tab[i].pts = apts;
            pts_tab[i].offset = offset;
            pts_tab[i].valid = 1;
            if (debug_enable) {
                ALOGI("%s:%d checkin done,offset %zu,apts 0x%"PRIx64"", __func__, __LINE__, offset, apts);
            }
            ret = 0;
            break;
        }
    }
    pthread_mutex_unlock(&avsync_ctx->lock);
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
int avsync_lookup_apts(avsync_ctx_t *avsync_ctx, size_t offset, uint64_t *p_apts)
{
    int i = 0;
    size_t align  = offset;
    int ret = -1;
    //struct aml_stream_out  *out = NULL;
    int    debug_enable = 0;
    //struct aml_audio_device *adev = p_header->aout->dev;
    //struct audio_stream_out *stream = (struct audio_stream_out *)p_header->aout;
    //int    debug_enable = (adev->debug_flag > 8);
    uint32_t latency_frames = 0;
    uint64_t latency_pts = 0;
    apts_tab_t *pts_tab = NULL;
    uint64_t nearest_pts = -1;
    uint32_t nearest_offset = 0;
    uint32_t min_offset = 0x7fffffff;
    int match_index = -1;

    // add protection to avoid NULL pointer.
    if (!avsync_ctx) {
        ALOGE("%s null point", __func__);
        return -1;
    }

    debug_enable = aml_audio_get_hwsync_flag();

    pthread_mutex_lock(&avsync_ctx->lock);

#if 0
    /*the hw_sync_frame_size will be an issue if it is fixed one*/
    //ALOGI("Adaptive steam =%d", p_header->bvariable_frame_size);
    if (!p_header->bvariable_frame_size) {
        if (p_header->hw_sync_frame_size) {
            align = offset - offset % p_header->hw_sync_frame_size;
        } else {
            align = offset;
        }
    } else {
        align = offset;
    }
#endif
    pts_tab = avsync_ctx->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid) {
            if (pts_tab[i].offset == align) {
                *p_apts = pts_tab[i].pts;
                nearest_offset = pts_tab[i].offset;
                ret = 0;
                if (debug_enable) {
                    ALOGI("%s pts checkout done,offset %zu,align %zu,pts 0x%"PRIx64"",
                          __func__, offset, align, *p_apts);
                }
                break;
            } else if (pts_tab[i].offset < align) {
                /*find the nearest one*/
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
    if (i == HWSYNC_APTS_NUM) {
        if (-1 != nearest_pts) {
            ret = 0;
            *p_apts = nearest_pts;
            /*keep it as valid, it may be used for next lookup*/
            pts_tab[match_index].valid = 1;
#if 0
            /*sometimes, we can't get the correct ddp pts, but we have a nearest one
             *we add one frame duration
             */
            if (out->hal_internal_format == AUDIO_FORMAT_AC3 ||
                out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
                *p_apts += (1536 * 1000) / out->hal_rate * 90;
                ALOGI("correct nearest pts 0x%llx offset %u align %zu", *p_apts, nearest_offset, align);
            }
#endif
            if (debug_enable)
                ALOGI("find nearest pts 0x%"PRIx64" offset %"PRIu32" align %zu", *p_apts, nearest_offset, align);
        } else {
            ALOGE("%s,apts lookup failed,align %zu,offset %zu", __func__, align, offset);
        }
    }
#if 0
    if ((ret == 0) && audio_is_linear_pcm(out->hal_internal_format)) {
        int diff = 0;
        int pts_diff = 0;
        uint32_t frame_size = out->hal_frame_size;
        uint32_t sample_rate  = out->hal_rate;
        diff = (offset >= nearest_offset) ? (offset - nearest_offset) : 0;
        if ((frame_size != 0) && (sample_rate != 0)) {
            pts_diff = (diff * 1000/frame_size)/sample_rate;
        }
        *p_apts +=  pts_diff * 90;
        if (debug_enable) {
            ALOGI("data offset =%zu pts offset =%d diff =%" PRIuFAST16 " pts=0x%llx pts diff =%d", offset, nearest_offset, offset - nearest_offset, *p_apts, pts_diff);
        }
    }
#endif
    pthread_mutex_unlock(&avsync_ctx->lock);
    return ret;
}

av_sync_policy_e get_and_map_avsync_policy(avsync_ctx_t *avsync_ctx, int avsync_type)
{
    av_sync_policy_e audio_sync_policy = AV_SYNC_AUDIO_UNKNOWN;
    switch (avsync_type)
    {
        case AVSYNC_TYPE_MEDIASYNC:
            {
                audio_mediasync_t *mediasync_ctx = avsync_ctx->mediasync_ctx;
                if (NULL == mediasync_ctx) {
                    AM_LOGE("mediasync_ctx is %p", mediasync_ctx);
                    return audio_sync_policy;
                }
                switch (mediasync_ctx->apolicy.audiopolicy)
                {
                    case MEDIASYNC_AUDIO_DROP_PCM:
                        audio_sync_policy = AV_SYNC_AUDIO_DROP_PCM;
                        break;
                    case MEDIASYNC_AUDIO_INSERT:
                        audio_sync_policy = AV_SYNC_AUDIO_INSERT;
                        break;
                    case MEDIASYNC_AUDIO_NORMAL_OUTPUT:
                        audio_sync_policy = AV_SYNC_AUDIO_NORMAL_OUTPUT;
                        break;
                    case MEDIASYNC_AUDIO_ADJUST_CLOCK:
                        audio_sync_policy = AV_SYNC_AUDIO_ADJUST_CLOCK;
                        break;
                    case MEDIASYNC_AUDIO_RESAMPLE:
                        audio_sync_policy = AV_SYNC_AUDIO_RESAMPLE;
                        break;
                    case MEDIASYNC_AUDIO_MUTE:
                        audio_sync_policy = AV_SYNC_AUDIO_MUTE;
                        break;
                    default :
                        audio_sync_policy = AV_SYNC_AUDIO_UNKNOWN;
                        break;
                }
            }
            break;
        case AVSYNC_TYPE_MSYNC:
            {
                audio_msync_t *msync_ctx = avsync_ctx->msync_ctx;
                if (NULL == msync_ctx) {
                    AM_LOGE("msync_ctx is %p", msync_ctx);
                    return audio_sync_policy;
                }
                switch (msync_ctx->msync_action)
                {
                    case AV_SYNC_AA_DROP:
                        audio_sync_policy = AV_SYNC_AUDIO_DROP_PCM;
                        break;
                    case AV_SYNC_AA_INSERT:
                        audio_sync_policy = AV_SYNC_AUDIO_INSERT;
                        break;
                    case AV_SYNC_AA_RENDER:
                        audio_sync_policy = AV_SYNC_AUDIO_NORMAL_OUTPUT;
                        break;
                    default :
                        audio_sync_policy = AV_SYNC_AUDIO_UNKNOWN;
                        break;
                }
            }
            break;
        default :
            audio_sync_policy = AV_SYNC_AUDIO_UNKNOWN;
            break;
    }
    return audio_sync_policy;
}

