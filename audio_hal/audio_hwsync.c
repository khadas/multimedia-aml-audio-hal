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
#include "audio_hwsync_wrap.h"
#include "aml_audio_ms12_sync.h"
#include "aml_audio_timer.h"

#include "tinyalsa_ext.h"

#include "aml_android_utils.h"
#define MSYNC_CALLBACK_WAIT_TIMEOUT_US (4000*1000)

static int aml_audio_get_hwsync_flag()
{
    int debug_flag = 0;
    debug_flag = aml_audio_property_get_int("vendor.media.audio.hal.hwsync", debug_flag);
    return debug_flag;
}


static bool check_support_mediasync()
{
    /*************************************************/
    /*    check_support_mediasync                    */
    /*    1. vendor.media.omx.use.omx2 be set true   */
    /*    2. android R use 5.4 kernel                */
    /*************************************************/
    struct utsname info;
    int kernel_version_major = 4;
    int kernel_version_minor = 9;

    if (getenv("vendor_media_omx_use_omx2")) {
        return aml_audio_property_get_bool("vendor.media.omx.use.omx2", 0);
    }

    if (uname(&info) || sscanf(info.release, "%d.%d", &kernel_version_major, &kernel_version_minor) <= 0) {
        ALOGW("Could not get linux version: %s", strerror(errno));
    }

    ALOGI("%s kernel_version_major:%d", __func__, kernel_version_major);
    if (kernel_version_major > 4) {
        ALOGI("%s kernel 5.4 use mediasync", __func__);
        return true;
    }
    return false;
}

void* aml_hwsync_mediasync_create()
{
    ALOGI("%s", __func__);
    if (check_support_mediasync()) {
        ALOGI("%s use mediasync", __func__);
        return aml_hwsync_wrap_mediasync_create();
    }
    ALOGI("%s use do not use mediasync", __func__);
    return NULL;
}

void aml_audio_hwsync_init(audio_hwsync_t *p_hwsync, struct aml_stream_out  *out)
{
    ALOGI("%s p_hwsync %p out %p\n", __func__, p_hwsync, out);
    int fd = -1;
    if (p_hwsync == NULL) {
        return;
    }
    p_hwsync->first_apts_flag = false;
    p_hwsync->msync_first_insert_flag = false;
    p_hwsync->video_valid_time = 0;
    p_hwsync->hw_sync_state = HW_SYNC_STATE_HEADER;
    p_hwsync->hw_sync_header_cnt = 0;
    p_hwsync->hw_sync_frame_size = 0;
    p_hwsync->bvariable_frame_size = 0;
    p_hwsync->version_num = 0;
    p_hwsync->wait_video_done = false;
    p_hwsync->eos = false;

    p_hwsync->last_lookup_apts = 0xffffffff;
    p_hwsync->last_ms12_latency_frames = 0;

    if (!p_hwsync->use_mediasync && (p_hwsync->es_mediasync.mediasync == NULL)) {
        p_hwsync->hwsync_id = -1;
    }

    memset(p_hwsync->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);
    pthread_mutex_init(&p_hwsync->lock, NULL);
    p_hwsync->payload_offset = 0;
    p_hwsync->aout = out;

/*
    if (p_hwsync->tsync_fd < 0) {
        fd = open(TSYNC_PCRSCR, O_RDONLY);
        p_hwsync->tsync_fd = fd;
        ALOGI("%s open tsync fd %d", __func__, fd);
    }
*/

    out->msync_start = false;

    ALOGI("%s done", __func__);
    out->tsync_status = TSYNC_STATUS_INIT;
    return;
}
void aml_audio_hwsync_release(audio_hwsync_t *p_hwsync)
{
    ALOGI("%s", __func__);
    if (!p_hwsync) {
        return;
    }
    aml_hwsync_wrap_release(p_hwsync);

    ALOGI("%s done", __func__);
}

bool aml_audio_hwsync_get_id(audio_hwsync_t *p_hwsync, int32_t* id)
{
    ALOGI("%s ==", __func__);
    return aml_hwsync_wrap_get_id(p_hwsync, id);
}

bool aml_audio_hwsync_set_id(audio_hwsync_t *p_hwsync, uint32_t id)
{
    ALOGI("%s ==", __func__);
    return aml_hwsync_wrap_set_id(p_hwsync, id);
}

//return bytes cost from input,
int aml_audio_hwsync_find_frame(audio_hwsync_t *p_hwsync,
        const void *in_buffer, size_t in_bytes, uint64_t *cur_pts, int *outsize)
{
    size_t remain = in_bytes;
    uint8_t *p = (uint8_t *)in_buffer;
    uint64_t time_diff = 0;
    int pts_found = 0;
    struct aml_audio_device *adev = p_hwsync->aout->dev;
    size_t  v2_hwsync_header = HW_AVSYNC_HEADER_SIZE_V2;
    int debug_enable = aml_audio_get_hwsync_flag();
    int header_sub_version = 0;
    if (p_hwsync == NULL || in_buffer == NULL) {
        return 0;
    }

    while (remain > 0) {
        if (p_hwsync->hw_sync_state == HW_SYNC_STATE_HEADER) {
            p_hwsync->hw_sync_header[p_hwsync->hw_sync_header_cnt++] = *p++;
            remain--;

            if (p_hwsync->hw_sync_header_cnt == HW_SYNC_VERSION_SIZE) {
                if (!hwsync_header_valid(&p_hwsync->hw_sync_header[0])) {
                    p_hwsync->hw_sync_state = HW_SYNC_STATE_HEADER;
                    memcpy(p_hwsync->hw_sync_header, p_hwsync->hw_sync_header + 1, HW_SYNC_VERSION_SIZE - 1);
                    p_hwsync->hw_sync_header_cnt--;
                    continue;
                }

                p_hwsync->version_num = p_hwsync->hw_sync_header[3];
                if (p_hwsync->version_num != 1 && p_hwsync->version_num != 2) {
                    ALOGI("invalid hwsync version num %d", p_hwsync->version_num);
                }
            }

            if ((p_hwsync->version_num == 1 && p_hwsync->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V1) ||
                (p_hwsync->version_num == 2 && p_hwsync->hw_sync_header_cnt == v2_hwsync_header)) {
                uint64_t pts = 0, pts_raw = 0;

                if (p_hwsync->version_num == 2 && p_hwsync->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V2) {
                    v2_hwsync_header = hwsync_header_get_offset(&p_hwsync->hw_sync_header[0]);
                    if (v2_hwsync_header > HW_AVSYNC_MAX_HEADER_SIZE) {
                        ALOGE("buffer overwrite, check the header size %zu \n", v2_hwsync_header);
                        break;
                    }

                    if (v2_hwsync_header > p_hwsync->hw_sync_header_cnt) {
                        ALOGV("need skip more sync header, %zu\n",v2_hwsync_header);
                        continue;
                    }
                }

                if ((in_bytes - remain) > p_hwsync->hw_sync_header_cnt) {
                    ALOGI("got the frame sync header cost %zu", in_bytes - remain);
                }

                p_hwsync->hw_sync_body_cnt = hwsync_header_get_size(&p_hwsync->hw_sync_header[0]);
                if (p_hwsync->hw_sync_body_cnt != 0) {
                    p_hwsync->hw_sync_state = HW_SYNC_STATE_BODY;
                }

                if (p_hwsync->hw_sync_body_cnt > HWSYNC_MAX_BODY_SIZE) {
                    ALOGE("body max size =%d hw_sync_body_cnt =%d", HWSYNC_MAX_BODY_SIZE, p_hwsync->hw_sync_body_cnt);
                    p_hwsync->hw_sync_body_cnt = HWSYNC_MAX_BODY_SIZE;
                }

                if (p_hwsync->hw_sync_frame_size && p_hwsync->hw_sync_body_cnt) {
                    if (p_hwsync->hw_sync_frame_size != p_hwsync->hw_sync_body_cnt) {
                        p_hwsync->bvariable_frame_size = 1;
                        ALOGV("old frame size=%d new=%d", p_hwsync->hw_sync_frame_size, p_hwsync->hw_sync_body_cnt);
                    }
                }
                p_hwsync->hw_sync_frame_size = p_hwsync->hw_sync_body_cnt;
                p_hwsync->body_align_cnt = 0; //  alisan zz
                p_hwsync->hw_sync_header_cnt = 0; //8.1
                pts_raw = pts = hwsync_header_get_pts(&p_hwsync->hw_sync_header[0]);
                if (pts == HWSYNC_PTS_EOS) {
                    p_hwsync->eos = true;
                    ALOGI("%s: eos detected!", __FUNCTION__);
                    /* in nonms12 render case, there would pop noise when playback ending, as there's no fade out.
                    *  and we use AED mute to avoid noise.
                    */
                    if (adev->dolby_lib_type != eDolbyMS12Lib && is_dolby_format(p_hwsync->aout->hal_internal_format)) {
                        set_aed_master_volume_mute(&adev->alsa_mixer, true);
                    }
                    continue;
                }
                header_sub_version = p_hwsync->hw_sync_header[2];
                if (!header_sub_version)
                    pts = pts * 90 / 1000000;
                time_diff = get_pts_gap(pts, p_hwsync->last_apts_from_header) / 90;
                if (debug_enable) {
                    ALOGI("pts 0x%"PRIx64",frame len %u\n", pts, p_hwsync->hw_sync_body_cnt);
                    ALOGI("last pts 0x%"PRIx64",diff %lld ms\n", p_hwsync->last_apts_from_header, time_diff);
                }
                if (p_hwsync->hw_sync_frame_size > HWSYNC_MAX_BODY_SIZE) {
                    ALOGE("hwsync frame body %d bigger than pre-defined size %d, need check !!!!!\n",
                                p_hwsync->hw_sync_frame_size,HWSYNC_MAX_BODY_SIZE);
                    p_hwsync->hw_sync_state = HW_SYNC_STATE_HEADER;
                    return 0;
                }

                if (time_diff > 32) {
                    ALOGV("pts  time gap %"PRIx64" ms,last %"PRIx64",cur %"PRIx64"\n", time_diff,
                          p_hwsync->last_apts_from_header, pts);
                }
                p_hwsync->last_apts_from_header = pts;
                p_hwsync->last_apts_from_header_raw = pts_raw;
                *cur_pts = (pts_raw == HWSYNC_PTS_NA) ? HWSYNC_PTS_NA : pts;
                pts_found = 1;
            }
            continue;
        } else if (p_hwsync->hw_sync_state == HW_SYNC_STATE_BODY) {
            int m = (p_hwsync->hw_sync_body_cnt < remain) ? p_hwsync->hw_sync_body_cnt : remain;
            // process m bytes body with an empty fragment for alignment
            if (m > 0) {
                memcpy(p_hwsync->hw_sync_body_buf + p_hwsync->hw_sync_frame_size - p_hwsync->hw_sync_body_cnt, p, m);
                p += m;
                remain -= m;
                p_hwsync->hw_sync_body_cnt -= m;
                if (p_hwsync->hw_sync_body_cnt == 0) {
                    p_hwsync->hw_sync_state = HW_SYNC_STATE_HEADER;
                    p_hwsync->hw_sync_header_cnt = 0;
                    *outsize = p_hwsync->hw_sync_frame_size;
                    /*
                    sometimes the audioflinger burst size is smaller than hwsync payload
                    we need use the last found pts when got a complete hwsync payload
                    */
                    if (!pts_found) {
                        *cur_pts = (p_hwsync->last_apts_from_header_raw == HWSYNC_PTS_NA) ?
                                    HWSYNC_PTS_NA : p_hwsync->last_apts_from_header;

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


void aml_audio_hwsync_msync_unblock_start(struct aml_stream_out *out)
{
    pthread_mutex_lock(&out->msync_mutex);
    out->msync_start = true;
    pthread_cond_signal(&out->msync_cond);
    pthread_mutex_unlock(&out->msync_mutex);
}

static int aml_audio_hwsync_msync_callback(void *priv, avs_ascb_reason reason)
{
    ALOGI("msync_callback reason %d", reason);
    aml_audio_hwsync_msync_unblock_start((struct aml_stream_out *)priv);
    return 0;
}


int aml_audio_hwsync_set_first_pts(audio_hwsync_t *p_hwsync, uint64_t pts)
{
    ALOGI("%s", __func__);
    uint32_t pts32;
    char tempbuf[128];
    int vframe_ready_cnt = 0;
    int delay_count = 0;

    if (p_hwsync == NULL) {
        return -1;
    }

    if (pts > 0xffffffff) {
        ALOGE("APTS exeed the 32bit range!");
        return -1;
    }

    pts32 = (uint32_t)pts;
    p_hwsync->first_apts = pts;

    if (p_hwsync->aout->msync_session) {
        ALOGI("%s apts:%d", __func__, pts32);
        int r = av_sync_audio_start(p_hwsync->aout->msync_session, pts32, 0,
                                   aml_audio_hwsync_msync_callback, p_hwsync->aout);

        if (r == AV_SYNC_ASTART_SYNC) {
            ALOGI("MSYNC AV_SYNC_ASTART_SYNC");
            p_hwsync->aout->msync_action = AV_SYNC_AA_RENDER;
            p_hwsync->first_apts_flag = true;
        } else if (r == AV_SYNC_ASTART_ASYNC) {
            struct timespec ts;
            ts_wait_time_us(&ts, MSYNC_CALLBACK_WAIT_TIMEOUT_US);
            pthread_mutex_lock(&p_hwsync->aout->msync_mutex);
            while (!p_hwsync->aout->msync_start) {
                ALOGI("%s wait %d ms", __func__, MSYNC_CALLBACK_WAIT_TIMEOUT_US/1000);
                pthread_cond_timedwait(&p_hwsync->aout->msync_cond, &p_hwsync->aout->msync_mutex, &ts);
                p_hwsync->aout->msync_start = true;
            }
            pthread_mutex_unlock(&p_hwsync->aout->msync_mutex);
            ALOGI("MSYNC AV_SYNC_ASTART_ASYNC");
            p_hwsync->aout->msync_action = AV_SYNC_AA_RENDER;
            p_hwsync->first_apts_flag = true;
        } else if (r == AV_SYNC_ASTART_AGAIN) {
            ALOGI("MSYNC AV_SYNC_ASTART_AGAIN");
            p_hwsync->aout->msync_action = AV_SYNC_AA_DROP;
        }
    }

    return 0;
}

static inline uint32_t abs32(int32_t a)
{
    return ((a) < 0 ? -(a) : (a));
}

#define pts_gap(a, b)   abs32(((int)(a) - (int)(b)))

/*
@offset :ms12 real costed offset
@p_adjust_ms: a/v adjust ms.if return a minus,means
 audio slow,need skip,need slow.return a plus value,means audio quick,need insert zero.
*/
int aml_audio_hwsync_audio_process(audio_hwsync_t *p_hwsync, size_t offset, uint64_t apts, int *p_adjust_ms)
{
    int ret = 0;
    //*p_adjust_ms = 0;
    uint32_t pcr = 0;
    uint32_t gap = 0;
    int gap_ms = 0;
    int debug_enable = 0;
    char tempbuf[32] = {0};
    struct aml_audio_device *adev = NULL;
    int32_t latency_frames = 0;
    struct audio_stream_out *stream = NULL;
    uint32_t latency_pts = 0;
    struct aml_stream_out  *out = p_hwsync->aout;
    struct timespec ts;
    int pcr_pts_gap = 0;
    //int alsa_pcm_delay_frames = 0;
    //int alsa_bitstream_delay_frames = 0;
    int ms12_pipeline_delay_frames = 0;
    ALOGV("%s,================", __func__);
    if (p_adjust_ms) *p_adjust_ms = 0;

    int pts_log = aml_audio_property_get_bool("media.audiohal.ptslog", false);
    int force_action = 0;


    // add protection to avoid NULL pointer.
    if (p_hwsync == NULL) {
        ALOGE("%s,p_hwsync == NULL", __func__);
        return 0;
    }

    if (p_hwsync->aout == NULL) {
        ALOGE("%s,p_hwsync->aout == NULL", __func__);
    } else {
        adev = p_hwsync->aout->dev;
        if (adev == NULL) {
            ALOGE("%s,adev == NULL", __func__);
        } else {
            debug_enable = aml_audio_get_hwsync_flag();
        }
    }

    if ((ret == 0) &&
        (adev->gap_offset != 0) &&
        !adev->gap_ignore_pts &&
        ((int)(offset - (adev->gap_offset & 0xffffffff)) >= 0)) {
        /* when PTS gap exists (Netflix specific), skip APTS reset between [adev->gap_pts, adev->gap_pts + 500ms] */
        ALOGI("gap_pts = 0x%x", apts);
        adev->gap_pts = apts;
        adev->gap_ignore_pts = true;
    }

    if ((adev->gap_ignore_pts) && (ret == 0)) {
        if (pts_gap(apts, adev->gap_pts) < 90 * 500) {
            return 0;
        } else {
            ALOGI("%s, Recovered APTS/PCR checking", __func__);
            adev->gap_ignore_pts = false;
            adev->gap_offset = 0;
            adev->gap_passthrough_state = GAP_PASSTHROUGH_STATE_IDLE;
        }
    }

    /*get MS12 pipe line delay + alsa delay*/
    stream = (struct audio_stream_out *)p_hwsync->aout;
    if (stream) {
        if (adev && (eDolbyMS12Lib == adev->dolby_lib_type)) {
            /*the offset is the end of frame, so we need consider the frame len*/
            if (AVSYNC_TYPE_MSYNC == p_hwsync->aout->avsync_type) {
                latency_frames = aml_audio_get_msync_ms12_tunnel_latency(stream);
                ms12_pipeline_delay_frames = dolby_ms12_main_pipeline_latency_frames(stream);
            } else {
                latency_frames = aml_audio_get_ms12_tunnel_latency(stream);
            }
#ifndef NO_AUDIO_CAP
            if (adev->cap_buffer) {
                latency_frames += adev->cap_delay * 48;
            }
#endif
            //alsa_pcm_delay_frames = out_get_ms12_latency_frames(stream, false);
            //alsa_bitstream_delay_frames = out_get_ms12_bitstream_latency_ms(stream) * 48;
        } else {
            latency_frames = (int32_t)out_get_latency_frames(stream);
        }
        if (latency_frames < 0) {
            latency_frames = 0;
        }
        latency_pts = latency_frames * 90/ 48;
    }

    if (p_hwsync->use_mediasync) {
        if (MEDIA_SYNC_ESMODE(out)) {
            if (pts_log)
                ALOGI("[%s:%d], apts=0x%llx, latency_pts=%x", __func__, __LINE__, apts, latency_pts);
            out->hwsync->es_mediasync.out_start_apts = apts;
            out->hwsync->es_mediasync.cur_outapts = apts - latency_pts;
            aml_hwsynces_ms12_get_policy(out);
        }
    } else {
        if (p_hwsync->first_apts_flag == false && (apts >= latency_pts) && AVSYNC_TYPE_TSYNC == p_hwsync->aout->avsync_type) {
            ALOGI("%s apts = 0x%x (%d ms) latency=0x%x (%d ms)", __FUNCTION__, apts, apts / 90, latency_pts, latency_pts/90);
            ALOGI("%s aml_audio_hwsync_set_first_pts = 0x%x (%d ms)", __FUNCTION__, apts - latency_pts, (apts - latency_pts)/90);
            aml_audio_hwsync_set_first_pts(p_hwsync, apts - latency_pts);
        }

        if (p_hwsync->first_apts_flag == false && AVSYNC_TYPE_MSYNC == p_hwsync->aout->avsync_type) {
            ALOGI("%s apts = 0x%x (%d ms) latency=0x%x (%d ms)", __FUNCTION__, apts, apts / 90, latency_pts, latency_pts/90);
            ALOGI("%s aml_audio_hwsync_set_first_pts = 0x%x (%d ms)", __FUNCTION__, apts - latency_pts, (apts - latency_pts)/90);
            aml_audio_hwsync_set_first_pts(p_hwsync, apts - latency_pts);
            p_hwsync->last_lookup_apts = apts;
            adev->ms12.ms12_main_input_size = 0;
            if (out->msync_action == AV_SYNC_AA_RENDER) {
                clock_gettime(CLOCK_MONOTONIC_RAW, &out->msync_rendered_ts);
                out->msync_rendered_pts = apts - latency_pts;
                ALOGI("START MSYNC action switched to AA_INSERT First");
                out->msync_action = AV_SYNC_AA_INSERT;
                p_hwsync->msync_first_insert_flag = true;
                return 0; // go out, at least have insert to next round.
            }
        }

        if (p_hwsync->first_apts_flag) {
            uint32_t apts_save = apts;
            apts -= latency_pts;

            if (p_hwsync->eos && (eDolbyMS12Lib == adev->dolby_lib_type) && continuous_mode(adev)) {
                return 0;
            }

            if (out->msync_session &&  AVSYNC_TYPE_MSYNC == p_hwsync->aout->avsync_type) {
                struct audio_policy policy;
                if (adev->continuous_audio_mode &&
                    eDolbyMS12Lib == adev->dolby_lib_type &&
                    !p_hwsync->msync_first_insert_flag) {
                   if (dolby_ms12_get_main_underrun()) {
                        /* when MS12 main pipeline gets underrun, the output latency does not reflect the real
                         * latency since muting frame can be inserted by MS12 when continuous output mode is
                         * enabled. Use last reported apts before underrun with monotonic time increment to
                         * report rendring time, with an upper bound set to latest check in PTS.
                         * Note main_underrun does not happen at the beginning before any samples
                         * are handled by MS12 system mixer.
                         */
                        uint32_t pts_adj = apts_save;
                        if (out->msync_rendered_ts.tv_sec) {
                            struct timespec ts;
                            uint32_t underrun_inc;
                            clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
                            underrun_inc = ((ts.tv_sec * 1000000000ULL + ts.tv_nsec) -
                                    (out->msync_rendered_ts.tv_sec * 1000000000ULL + out->msync_rendered_ts.tv_nsec))
                                    / 1000000 * 90;
                            if ((int)(underrun_inc + out->msync_rendered_pts) < apts_save) {
                                pts_adj = underrun_inc + out->msync_rendered_pts;
                            }
                        }

                        ALOGV("Use apts w/o delay deduction %u to replace %u due to underrun.", apts_save, pts_adj);
                        apts = pts_adj;
                    } else if (p_hwsync->last_lookup_apts == apts_save && p_hwsync->last_ms12_latency_frames == ms12_pipeline_delay_frames) {
                        /* only report audio pts when lookup_apts changes.
                         * the output from MS12 may come in smaller resolution (such as every 256 samples than
                         * input like 1536 samples for Dolby, then the rendered APTS become an almost fixed number
                         * with a small fluctuation from ALSA level at different monotonic time. To avoid such
                         * inaccurate reporting (growing monotonic time with almost same APTS), only when lookup
                         * PTS changed (new bit stream consumed), new rendering time is reported.
                         * However, if MS12 pipeline is stalled because it's in INSERT mode (paused), then lookup
                         * PTS is not changing also. MSYNC driver relies on continuous PTS rendering reporting to switch
                         * out of INSERT mode. When this happens, a rendering time reporting is still needed.
                         */
                        if (out->msync_action != AV_SYNC_AA_INSERT) {
                            /* skip reporting rendering time */
                            return 0;
                        }
                        /* still report last rendered APTS */
                        apts = out->msync_rendered_pts;
                    }
                    p_hwsync->last_ms12_latency_frames = ms12_pipeline_delay_frames;
                }

                if (pts_log) {
                    struct timespec ts, hw_ptr_ts = {0};
                    unsigned long hw_ptr = 0;
                    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
                    if (adev->pcm_handle[I2S_DEVICE])
                        pcm_get_hw_ptr(adev->pcm_handle[I2S_DEVICE], &hw_ptr, &hw_ptr_ts);
                    ALOGI("PTSLOG [%lld.%.9ld] injected %" PRIu64 ", offset:%d" PRIu64 ", lookup_pts:%u, latency_pts:%u, ms12 frames:%d, apts:%u, action:%d, hw_ptr[%lld.%.9ld:%lu]",
                          (long long)ts.tv_sec, ts.tv_nsec,
                          out->total_write_size, offset,
                          apts_save, latency_pts, ms12_pipeline_delay_frames, apts, out->msync_action,
                          (long long)hw_ptr_ts.tv_sec, hw_ptr_ts.tv_nsec, hw_ptr);
                }

                p_hwsync->last_lookup_apts = apts_save;
                policy.action = out->msync_action;
                if (apts != out->msync_rendered_pts)
                    av_sync_audio_render(out->msync_session, apts, &policy);
                out->msync_action_delta = policy.delta;
                out->msync_rendered_pts = apts;
                clock_gettime(CLOCK_MONOTONIC_RAW, &out->msync_rendered_ts);

                force_action = aml_audio_property_get_int("media.audiohal.action", 0);
                /* 0: default, no force action
                 * 1: AA_RENDER
                 * 2: AA_DROP
                 * 3: AA_INSERT
                 */
                if (force_action) {
                    policy.action = force_action - 1;
                    if (policy.action == AV_SYNC_AA_INSERT) {
                        out->msync_action_delta = -1;
                    } else if (policy.action == AV_SYNC_AA_DROP) {
                        out->msync_action_delta = 1;
                    }
                }

                if (out->msync_action != policy.action) {
                    /* msync_action_delta is > 0 for AV_SYNC_AA_DROP
                     * and MS12 "-sync <positive>" for insert
                     */
                    if (policy.action == AV_SYNC_AA_INSERT) {
                        ALOGI("MSYNC action switched to AA_INSERT.");
                    } else if (policy.action == AV_SYNC_AA_DROP) {
                        ALOGI("MSYNC action switched to AA_DROP.");
                    } else {
                        ALOGI("MSYNC action switched to AA_RENDER.");
                    }
                    out->msync_action = policy.action;
                    p_hwsync->msync_first_insert_flag = false;
                }

                if (debug_enable) {
                    uint32_t pcr;
                    av_sync_get_clock(out->msync_session, &pcr);
                    gap = get_pts_gap(pcr, apts);
                    gap_ms = gap / 90;
                    ALOGI("%s pcr 0x%x, apts 0x%x, gap %d(%d) ms, offset %d, apts_lookup=%d, latency=%d",
                        __func__, pcr, apts, gap, gap_ms, offset, apts_save / 90, latency_pts / 90);
                }
            }
        }
    }
    return ret;
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
void aml_audio_hwsync_reset_apts(audio_hwsync_t *p_hwsync)
{
    if (NULL == p_hwsync) {
        ALOGE("%s p_hwsync null point", __func__);
        return;
    }

    pthread_mutex_lock(&p_hwsync->lock);
    ALOGI("%s Reset, payload_offset %zu, first_apts 0x%llx, first_apts_flag %d", __func__,
           p_hwsync->payload_offset, p_hwsync->first_apts, p_hwsync->first_apts_flag);

    /* reset payload offset and pts table */
    p_hwsync->payload_offset = 0;
    memset(p_hwsync->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);
    pthread_mutex_unlock(&p_hwsync->lock);
    return;
}

int aml_audio_hwsync_checkin_apts(audio_hwsync_t *p_hwsync, size_t offset, uint64_t apts)
{
    int i = 0;
    int ret = -1;
    struct aml_audio_device *adev = p_hwsync->aout->dev;
    int debug_enable = aml_audio_get_hwsync_flag();
    apts_tab_t *pts_tab = NULL;
    if (!p_hwsync) {
        ALOGE("%s null point", __func__);
        return -1;
    }
    if (debug_enable) {
        ALOGI("++ %s checkin ,offset %zu,apts 0x%llx", __func__, offset, apts);
    }
    pthread_mutex_lock(&p_hwsync->lock);
    pts_tab = p_hwsync->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid && (pts_tab[i].offset == offset)) {
            /* for duplicated record, reuse previous entry.
             * This may happen when msync returns ASYNC_AGAIN when first checkin
             * record at offset 0 are overwritten after dropping data
             */
            pts_tab[i].pts = apts;
            if (debug_enable) {
                ALOGI("%s:%d checkin done,offset %zu,apts 0x%llx", __func__, __LINE__, offset, apts);
            }
            ret = 0;
            break;
        } else if (!pts_tab[i].valid) {
            pts_tab[i].pts = apts;
            pts_tab[i].offset = offset;
            pts_tab[i].valid = 1;
            if (debug_enable) {
                ALOGI("%s checkin done,offset %zu,apts 0x%llx", __func__, __LINE__, offset, apts);
            }
            ret = 0;
            break;
        }
    }
    pthread_mutex_unlock(&p_hwsync->lock);
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
int aml_audio_hwsync_lookup_apts(audio_hwsync_t *p_hwsync, size_t offset, uint64_t *p_apts)
{
    int i = 0;
    size_t align  = 0;
    int ret = -1;
    struct aml_audio_device *adev = NULL;
    struct aml_stream_out  *out = NULL;
    int    debug_enable = 0;
    //struct aml_audio_device *adev = p_hwsync->aout->dev;
    //struct audio_stream_out *stream = (struct audio_stream_out *)p_hwsync->aout;
    //int    debug_enable = (adev->debug_flag > 8);
    uint32_t latency_frames = 0;
    uint64_t latency_pts = 0;
    apts_tab_t *pts_tab = NULL;
    uint64_t nearest_pts = 0;
    uint32_t nearest_offset = 0;
    uint32_t min_offset = 0x7fffffff;
    int match_index = -1;

    // add protection to avoid NULL pointer.
    if (!p_hwsync) {
        ALOGE("%s null point", __func__);
        return -1;
    }

    if (p_hwsync->aout == NULL) {
        ALOGE("%s,p_hwsync->aout == NULL", __func__);
        return -1;
    }

    out = p_hwsync->aout;
    adev = p_hwsync->aout->dev;
    if (adev == NULL) {
        ALOGE("%s,adev == NULL", __func__);
    } else {
        debug_enable = aml_audio_get_hwsync_flag();
    }

    if (debug_enable) {
        ALOGI("%s offset %zu,first %d", __func__, offset, p_hwsync->first_apts_flag);
    }
    pthread_mutex_lock(&p_hwsync->lock);

    /*the hw_sync_frame_size will be an issue if it is fixed one*/
    //ALOGI("Adaptive steam =%d", p_hwsync->bvariable_frame_size);
    if (!p_hwsync->bvariable_frame_size) {
        if (p_hwsync->hw_sync_frame_size) {
            align = offset - offset % p_hwsync->hw_sync_frame_size;
        } else {
            align = offset;
        }
    } else {
        align = offset;
    }
    pts_tab = p_hwsync->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid) {
            if (pts_tab[i].offset == align) {
                *p_apts = pts_tab[i].pts;
                nearest_offset = pts_tab[i].offset;
                ret = 0;
                if (debug_enable) {
                    ALOGI("%s first flag %d,pts checkout done,offset %zu,align %zu,pts 0x%llx",
                          __func__, p_hwsync->first_apts_flag, offset, align, *p_apts);
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
        if (nearest_pts) {
            ret = 0;
            *p_apts = nearest_pts;
            /*keep it as valid, it may be used for next lookup*/
            pts_tab[match_index].valid = 1;
            /*sometimes, we can't get the correct ddp pts, but we have a nearest one
             *we add one frame duration
             */
            if (out->hal_internal_format == AUDIO_FORMAT_AC3 ||
                out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
                *p_apts += (1536 * 1000) / out->hal_rate * 90;
                ALOGI("correct nearest pts 0x%llx offset %u align %zu", *p_apts, nearest_offset, align);
            }
            if (debug_enable)
                ALOGI("find nearest pts 0x%llx offset %u align %zu", *p_apts, nearest_offset, align);
        } else {
            ALOGE("%s,apts lookup failed,align %zu,offset %zu", __func__, align, offset);
        }
    }
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
    pthread_mutex_unlock(&p_hwsync->lock);
    return ret;
}
