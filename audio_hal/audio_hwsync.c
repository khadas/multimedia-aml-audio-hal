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



#define LOG_TAG "audio_hw_hwsync"
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
#include "audio_hw_utils.h"
#include "audio_hwsync.h"
#include "audio_hw.h"
#include "dolby_lib_api.h"

void aml_hwsync_set_tsync_pause(void)
{
    ALOGI("%s(), send pause event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_PAUSE");
}

void aml_hwsync_set_tsync_resume(void)
{
    ALOGI("%s(), send resuem event", __func__);
    sysfs_set_sysfs_str(TSYNC_EVENT, "AUDIO_RESUME");
}

int aml_hwsync_set_tsync_start_pts(uint32_t pts)
{
    char buf[64] = {0};

    snprintf(buf, 64, "AUDIO_START:0x%x", pts);
    ALOGI("tsync -> %s", buf);
    return sysfs_set_sysfs_str(TSYNC_EVENT, buf);
}

int aml_hwsync_open_tsync(void)
{
    return open(TSYNC_PCRSCR, O_RDONLY);
}

void aml_hwsync_close_tsync(int fd)
{
    if (fd >= 0) {
        ALOGE("%s(), fd = %d", __func__, fd);
        close(fd);
    }
}

int aml_hwsync_get_tsync_pts_by_handle(int fd, uint32_t *pts)
{
    char valstr[64];
    uint val = 0;
    int nread;

    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    if (fd >= 0) {
        memset(valstr, 0, 64);
        lseek(fd, 0, SEEK_SET);
        nread = read(fd, valstr, 64 - 1);
        if (nread < 0) {
            nread = 0;
        }
        valstr[nread] = '\0';
    } else {
        ALOGE("invalid fd\n");
        return -EINVAL;
    }

    if (sscanf(valstr, "0x%x", &val) < 1) {
        ALOGE("unable to get pts from: fd(%d), str(%s)", fd, valstr);
        return -EINVAL;
    }
    *pts = val;
    return 0;
}

int aml_hwsync_get_tsync_pts(uint32_t *pts)
{
    if (!pts) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    return get_sysfs_uint(TSYNC_PCRSCR, pts);
}

int aml_hwsync_reset_tsync_pcrscr(uint32_t pts)
{
    char buf[64] = {0};

    snprintf(buf, 64, "0x%x", pts);
    ALOGI("tsync -> reset pcrscr 0x%x", pts);
    return sysfs_set_sysfs_str(TSYNC_APTS, buf);
}

static int aml_audio_hwsync_get_pcr(audio_hwsync_t *p_hwsync, uint *value)
{
    int fd = -1, nread;
    char valstr[64];
    uint val = 0;
    off_t offset;
    if (!p_hwsync) {
        ALOGE("invalid pointer %s", __func__);
        return -1;
    }
    if (p_hwsync->tsync_fd < 0) {
        fd = open(TSYNC_PCRSCR, O_RDONLY);
        p_hwsync->tsync_fd = fd;
        ALOGI("%s open tsync fd %d", __func__, fd);
    } else {
        fd = p_hwsync->tsync_fd;
    }
    if (fd >= 0) {
        memset(valstr, 0, 64);
        offset = lseek(fd, 0, SEEK_SET);
        nread = read(fd, valstr, 64 - 1);
        if (nread < 0) {
            nread = 0;
        }
        valstr[nread] = '\0';
    } else {
        ALOGE("%s unable to open file %s\n", __func__, TSYNC_PCRSCR);
        return -1;
    }
    if (sscanf(valstr, "0x%x", &val) < 1) {
        ALOGE("%s unable to get pcr from: %s,fd %d,offset %ld", __func__, valstr, fd, offset);
        return -1;
    }
    *value = val;
    return 0;
}

void aml_audio_hwsync_init(audio_hwsync_t *p_hwsync, struct aml_stream_out  *out)
{
    int fd = -1;
    if (p_hwsync == NULL) {
        return;
    }
    p_hwsync->first_apts_flag = false;
    p_hwsync->hw_sync_state = HW_SYNC_STATE_HEADER;
    p_hwsync->hw_sync_header_cnt = 0;
    p_hwsync->hw_sync_frame_size = 0;
    p_hwsync->bvariable_frame_size = 0;
    p_hwsync->version_num = 0;
    memset(p_hwsync->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);
    pthread_mutex_init(&p_hwsync->lock, NULL);
    p_hwsync->payload_offset = 0;
    p_hwsync->aout = out;
    p_hwsync->eos = false;
    if (p_hwsync->tsync_fd < 0) {
        fd = open(TSYNC_PCRSCR, O_RDONLY);
        p_hwsync->tsync_fd = fd;
        ALOGI("%s open tsync fd %d", __func__, fd);
    }
    out->tsync_status = TSYNC_STATUS_INIT;
#ifdef USE_MSYNC
    out->msync_start = false;
#endif
    ALOGI("%s done", __func__);
    return;
}
void aml_audio_hwsync_release(audio_hwsync_t *p_hwsync)
{
    if (!p_hwsync) {
        return;
    }
    if (p_hwsync->tsync_fd >= 0) {
        close(p_hwsync->tsync_fd);
    }
    p_hwsync->tsync_fd = -1;
    ALOGI("%s done", __func__);
}
//return bytes cost from input,
int aml_audio_hwsync_find_frame(audio_hwsync_t *p_hwsync,
        const void *in_buffer, size_t in_bytes, uint64_t *cur_pts, int *outsize)
{
    size_t remain = in_bytes;
    uint8_t *p = (uint8_t *)in_buffer;
    uint64_t time_diff = 0;
    int pts_found = 0;
    struct aml_audio_device *adev;
    size_t  v2_hwsync_header = HW_AVSYNC_HEADER_SIZE_V2;
    int debug_enable;
    if (p_hwsync == NULL || in_buffer == NULL) {
        return 0;
    }
    adev = p_hwsync->aout->dev;
    debug_enable = (adev->debug_flag > 8);

    //ALOGI(" --- out_write %d, cache cnt = %d, body = %d, hw_sync_state = %d", out_frames * frame_size, out->body_align_cnt, out->hw_sync_body_cnt, out->hw_sync_state);
    while (remain > 0) {
        //if (p_hwsync->hw_sync_state == HW_SYNC_STATE_RESYNC) {
        //}
        if (p_hwsync->hw_sync_state == HW_SYNC_STATE_HEADER) {
            //ALOGI("Add to header buffer [%d], 0x%x", p_hwsync->hw_sync_header_cnt, *p);
            p_hwsync->hw_sync_header[p_hwsync->hw_sync_header_cnt++] = *p++;
            remain--;
            if (p_hwsync->hw_sync_header_cnt == HW_SYNC_VERSION_SIZE ) {
                if (!hwsync_header_valid(&p_hwsync->hw_sync_header[0])) {
                    //ALOGE("!!!!!!hwsync header out of sync! Resync.should not happen????");
                    p_hwsync->hw_sync_state = HW_SYNC_STATE_HEADER;
                    memmove(p_hwsync->hw_sync_header, p_hwsync->hw_sync_header + 1, HW_SYNC_VERSION_SIZE - 1);
                    p_hwsync->hw_sync_header_cnt--;
                    continue;
                }
                p_hwsync->version_num = p_hwsync->hw_sync_header[3];
                if (p_hwsync->version_num == 1 || p_hwsync->version_num == 2) {
                } else  {
                    ALOGI("invalid hwsync version num %zu",p_hwsync->version_num);
                }
            }
            if ((p_hwsync->version_num  == 1 && p_hwsync->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V1 ) ||
                (p_hwsync->version_num  == 2 && p_hwsync->hw_sync_header_cnt == v2_hwsync_header )) {
                uint64_t pts;
                /**/

                if (p_hwsync->version_num  == 2 && p_hwsync->hw_sync_header_cnt == HW_AVSYNC_HEADER_SIZE_V2 ) {
                    v2_hwsync_header = hwsync_header_get_offset(&p_hwsync->hw_sync_header[0]);
                    if (v2_hwsync_header > HW_AVSYNC_MAX_HEADER_SIZE) {
                        ALOGE("buffer overwrite, check the header size %zu \n",v2_hwsync_header);
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
                p_hwsync->hw_sync_state = HW_SYNC_STATE_BODY;
                p_hwsync->hw_sync_body_cnt = hwsync_header_get_size(&p_hwsync->hw_sync_header[0]);
                if (p_hwsync->hw_sync_frame_size && p_hwsync->hw_sync_body_cnt) {
                    if (p_hwsync->hw_sync_frame_size != p_hwsync->hw_sync_body_cnt) {
                        p_hwsync->bvariable_frame_size = 1;
                        if (adev->debug_flag > 8)
                            ALOGI("old frame size=%d new=%d", p_hwsync->hw_sync_frame_size, p_hwsync->hw_sync_body_cnt);
                    }
                }
                p_hwsync->hw_sync_frame_size = p_hwsync->hw_sync_body_cnt;
                p_hwsync->body_align_cnt = 0; //  alisan zz
                p_hwsync->hw_sync_header_cnt = 0; //8.1
                pts = hwsync_header_get_pts(&p_hwsync->hw_sync_header[0]);
                if (pts == HWSYNC_PTS_EOS) {
                    p_hwsync->eos = true;
                    continue;
                }
                pts = pts * 90 / 1000000;
                time_diff = get_pts_gap(pts, p_hwsync->last_apts_from_header) / 90;
                if (adev->debug_flag > 8) {
                    ALOGV("pts 0x%"PRIx64",frame len %u\n", pts, p_hwsync->hw_sync_body_cnt);
                    ALOGV("last pts 0x%"PRIx64",diff 0x%"PRIx64" ms\n", p_hwsync->last_apts_from_header, time_diff);
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
                *cur_pts = pts;
                pts_found = 1;
                //ALOGI("get header body_cnt = %d, pts = %lld", out->hw_sync_body_cnt, pts);

            }
            continue;
        } else if (p_hwsync->hw_sync_state == HW_SYNC_STATE_BODY) {
            int m = (p_hwsync->hw_sync_body_cnt < remain) ? p_hwsync->hw_sync_body_cnt : remain;
            // process m bytes body with an empty fragment for alignment
            if (m  > 0) {
                //ret = pcm_write(out->pcm, p, m - align);
#if 0
                FILE *fp1 = fopen("/data/hwsync_body.pcm", "a+");
                if (fp1) {
                    int flen = fwrite((char *)p, 1, m, fp1);
                    //ALOGD("flen = %d---outlen=%d ", flen, out_frames * frame_size);
                    fclose(fp1);
                } else {
                    ALOGE("could not open file:/data/hwsync_body.pcm");
                }
#endif
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
                        *cur_pts = p_hwsync->last_apts_from_header;
                    }
                    if (debug_enable > 8) {
                        ALOGV("we found the frame total body,yeah\n");
                    }
                    break;//continue;
                }
            }
        }
    }
    return in_bytes - remain;
}

#ifdef USE_MSYNC
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
#endif

int aml_audio_hwsync_set_first_pts(audio_hwsync_t *p_hwsync, uint64_t pts)
{
    uint32_t pts32;
    char tempbuf[128];
    int vframe_ready_cnt = 0;
    // LINUX change
    // int delay_count = 0;

    if (p_hwsync == NULL) {
        return -1;
    }

    if (pts > 0xffffffff) {
        ALOGE("APTS exeed the 32bit range!");
        return -1;
    }

    pts32 = (uint32_t)pts;
    p_hwsync->first_apts_flag = true;
    p_hwsync->first_apts = pts;

    // LINUX change
#if 0
    while (delay_count < 10) {
        vframe_ready_cnt = get_sysfs_int("/sys/class/video/vframe_ready_cnt");
        ALOGV("/sys/class/video/vframe_ready_cnt is %d", vframe_ready_cnt);
        if (vframe_ready_cnt < 2) {
            usleep(10000);
            delay_count++;
            continue;
        }
        break;
    }
    ALOGI("wait video ready cnt =%d", delay_count);
#endif

#ifndef USE_MSYNC
    if (aml_hwsync_set_tsync_start_pts(pts32) < 0)
        return -EINVAL;
#else
    if (p_hwsync->aout->msync_session) {
       if (av_sync_audio_start(p_hwsync->aout->msync_session, pts32, 0,
             aml_audio_hwsync_msync_callback, p_hwsync->aout) == AV_SYNC_ASTART_ASYNC) {
           pthread_mutex_lock(&p_hwsync->aout->msync_mutex);
           while (!p_hwsync->aout->msync_start) {
               pthread_cond_wait(&p_hwsync->aout->msync_cond, &p_hwsync->aout->msync_mutex);
           }
           pthread_mutex_unlock(&p_hwsync->aout->msync_mutex);
       }
    }
#endif
    p_hwsync->aout->tsync_status = TSYNC_STATUS_RUNNING;
    return 0;
}

static inline uint32_t abs32(int32_t a)
{
    return ((a) < 0 ? -(a) : (a));
}

#define pts_gap(a, b)   abs32(((int)(a) - (int)(b)))
#define pts_bigger(a, b) ((int)(a) > (int)(b))

/*
@offset :ms12 real costed offset
@p_adjust_ms: a/v adjust ms.if return a minus,means
 audio slow,need skip,need slow.return a plus value,means audio quick,need insert zero.
*/
int aml_audio_hwsync_audio_process(audio_hwsync_t *p_hwsync, size_t offset, int *p_adjust_ms)
{
    uint32_t apts = 0;
    int ret = 0;
    uint pcr = 0;
    uint gap = 0;
    int gap_ms = 0;
    int debug_enable = 0;
    char tempbuf[32] = {0};
    struct aml_audio_device *adev = NULL;
    int32_t latency_frames = 0;
    struct audio_stream_out *stream = NULL;
    uint32_t latency_pts = 0;
    struct aml_stream_out  *out;
    int b_raw = 0;
#ifdef DIAG_LOG
    uint32_t a2a_pts;
#endif

    if (p_adjust_ms) *p_adjust_ms = 0;

    // add protection to avoid NULL pointer.
    if (p_hwsync == NULL) {
        ALOGE("%s,p_hwsync == NULL", __func__);
        return 0;
    }

    out = p_hwsync->aout;

    if (p_hwsync->aout == NULL) {
        ALOGE("%s,p_hwsync->aout == NULL", __func__);
        return  0;
    } else {
        adev = p_hwsync->aout->dev;
        if (adev == NULL) {
            ALOGE("%s,adev == NULL", __func__);
        } else {
            debug_enable = (adev->debug_flag > 8);
        }
    }

    /*when it is ddp 2ch/heaac we need use different latency control*/
    if (audio_is_linear_pcm(out->hal_internal_format)) {
        b_raw = 0;
    }else {
        b_raw = 1;
    }

    ret = aml_audio_hwsync_lookup_apts(p_hwsync, offset, &apts);

#ifdef DIAG_LOG
    a2a_pts = apts;
#endif
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
        }
    }

    /*get MS12 pipe line delay + alsa delay*/
    stream = (struct audio_stream_out *)p_hwsync->aout;
    if (stream) {
        if (adev && adev->continuous_audio_mode && (eDolbyMS12Lib == adev->dolby_lib_type)) {
            /*we need get the correct ms12 out pcm */
            latency_frames = out_get_ms12_latency_frames(stream);
            //ALOGI("latency_frames =%d", latency_frames);
            latency_frames += aml_audio_get_ms12_tunnel_latency_offset(b_raw)*48; // add 60ms delay for ms12, 32ms pts offset, other is ms12 delay
#ifdef DIAG_LOG
            a2a_pts -= latency_frames / 48 * 90;
#endif

#ifdef AUDIO_CAP
            if (adev->cap_buffer) {
                latency_frames += adev->cap_delay * 48;
            }
#endif
            if ((adev->ms12.is_dolby_atmos && adev->ms12_main1_dolby_dummy == false) || adev->atoms_lock_flag) {
                int tunnel = 1;
                int atmos_tunning_frame = aml_audio_get_ms12_atmos_latency_offset(tunnel) * 48;
                latency_frames += atmos_tunning_frame;
            }
            if (adev->active_outport == OUTPORT_HDMI_ARC) {
                int arc_latency_ms = 0;
                arc_latency_ms = aml_audio_get_hdmi_latency_offset(adev->sink_format);
                /*the latency is minus, so we minus it*/
                latency_frames -= arc_latency_ms * 48;
            }
        } else {
            latency_frames = out_get_latency_frames(stream);
        }

        if (latency_frames < 0) {
            latency_frames = 0;
        }

        latency_pts = latency_frames / 48 * 90;
    }


    if (ret) {
        ALOGE("%s lookup failed", __func__);
        return 0;
    }

#ifdef DIAG_LOG
    adev->a2a_pts = a2a_pts;
#endif

#ifndef USE_MSYNC
    if (p_hwsync->first_apts_flag == false && offset > 0) {
#else
    if (p_hwsync->first_apts_flag == false) {
#endif
        ALOGI("%s apts = 0x%x (%d ms) latency=0x%x (%d ms)", __FUNCTION__, apts, apts / 90, latency_pts, latency_pts/90);
        ALOGI("%s aml_audio_hwsync_set_first_pts = 0x%x (%d ms)", __FUNCTION__, apts - latency_pts, (apts - latency_pts)/90);
        aml_audio_hwsync_set_first_pts(p_hwsync, apts - latency_pts);
    } else  if (p_hwsync->first_apts_flag) {
        struct aml_audio_device *adev = p_hwsync->aout->dev;
        uint32_t apts_save = apts;
        apts -= latency_pts;

        if (p_hwsync->eos && (eDolbyMS12Lib == adev->dolby_lib_type) && continous_mode(adev)) {
            return 0;
        }

#ifndef USE_MSYNC
        ret = aml_audio_hwsync_get_pcr(p_hwsync, &pcr);
        if (ret == 0) {
            gap = pts_gap(pcr, apts);
            gap_ms = gap / 90;
            if (debug_enable) {
                ALOGI("%s pcr 0x%x,apts 0x%x,gap 0x%x,gap duration %d ms, offset %d, apts_lookup=%d, latency=%d",
                      __func__, pcr, apts, gap, gap_ms, offset, apts_save / 90, latency_pts / 90);
            }

            if (gap > APTS_DISCONTINUE_THRESHOLD_MIN && gap < APTS_DISCONTINUE_THRESHOLD_MAX) {
                if (pts_bigger(apts, pcr)) {
                    /*during video stop, pcr has been reset by video
                      we need ignore such pcr value*/
                    if ((pcr != 0) && p_adjust_ms) {
                        *p_adjust_ms = gap_ms;
                        ALOGE("%s *p_adjust_ms %d\n", __func__, *p_adjust_ms);
                    } else {
                        ALOGE("pcr has been reset\n");
                    }
                } else {
                    if (adev->continuous_audio_mode &&
                        eDolbyMS12Lib == adev->dolby_lib_type &&
                        (adev->ms12.underrun_cnt != dolby_ms12_get_main_underrun())) {
                        /* when MS12 main pipeline gets underrun, the output latency does not reflect the real
                         * latency since muting frame can be inserted by MS12 when continuous output mode is
                         * enabled, so only reset APTS when PCR exceeds last checkin PTS in such case.
                         */
                        gap = pts_gap(pcr, apts_save);
                        if (pts_bigger(pcr, apts_save)) {
                            sprintf(tempbuf, "0x%x", apts_save);
                            sysfs_set_sysfs_str(TSYNC_APTS, tempbuf);
                            ALOGI("tsync -> reset pcrscr 0x%x -> 0x%x", pcr, apts_save);
                        }
                    } else {
                        ALOGI("tsync -> reset pcrscr 0x%x -> 0x%x, %s big,diff %"PRIx64" ms",
                              pcr, apts, apts > pcr ? "apts" : "pcr", get_pts_gap(apts, pcr) / 90);
                        sprintf(tempbuf, "0x%x", apts);
                        int ret_val = sysfs_set_sysfs_str(TSYNC_APTS, tempbuf);
                        if (ret_val == -1) {
                            ALOGE("unable to open file %s,err: %s", TSYNC_APTS, strerror(errno));
                        }
                    }
                }
            } else if (gap > APTS_DISCONTINUE_THRESHOLD_MAX) {
                ALOGE("%s apts exceed the adjust range,need check apts 0x%x,pcr 0x%x",
                      __func__, apts, pcr);
            }
        }
#else /* USE_MSYNC */
        if (out->msync_session) {
            struct audio_policy policy;

            av_sync_audio_render(out->msync_session, apts, &policy);

            if (debug_enable) {
                uint32_t pcr;
                av_sync_get_clock(out->msync_session, &pcr);
                gap = get_pts_gap(pcr, apts);
                gap_ms = gap / 90;
                ALOGI("%s pcr 0x%x, apts 0x%x, gap %d ms, offset %d, apts_lookup=%d, latency=%d",
                    __func__, pcr, apts, gap, gap_ms, offset, apts_save / 90, latency_pts / 90);
            }

            /* TODO: handles policy for non-amaster */
        }
#endif /* USE_MSYNC */
    }

    return ret;
}
int aml_audio_hwsync_checkin_apts(audio_hwsync_t *p_hwsync, size_t offset, unsigned apts)
{
    int i = 0;
    int ret = -1;
    struct aml_audio_device *adev;
    int debug_enable;
    apts_tab_t *pts_tab = NULL;
    if (!p_hwsync) {
        ALOGE("%s null point", __func__);
        return -1;
    }

    adev = p_hwsync->aout->dev;
    debug_enable = (adev->debug_flag > 8);
    if (debug_enable) {
        ALOGI("++ %s checkin ,offset %zu,apts 0x%x", __func__, offset, apts);
    }
    pthread_mutex_lock(&p_hwsync->lock);
    pts_tab = p_hwsync->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (!pts_tab[i].valid) {
            pts_tab[i].pts = apts;
            pts_tab[i].offset = offset;
            pts_tab[i].valid = 1;
            if (debug_enable) {
                ALOGI("%s checkin done,offset %zu,apts 0x%x", __func__, offset, apts);
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
int aml_audio_hwsync_lookup_apts(audio_hwsync_t *p_hwsync, size_t offset, unsigned *p_apts)
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
    uint32_t latency_pts = 0;
    apts_tab_t *pts_tab = NULL;
    uint32_t nearest_pts = 0;
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
        debug_enable = (adev->debug_flag > 8);
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
                    ALOGI("%s first flag %d,pts checkout done,offset %zu,align %zu,pts 0x%x",
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
            if (debug_enable)
                ALOGI("find nearest pts 0x%x offset %zu align %zu", *p_apts, nearest_offset, align);
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
            ALOGI("data offset =%d pts offset =%d diff =%d pts=0x%x pts diff =%d", offset, nearest_offset, offset - nearest_offset, *p_apts, pts_diff);
        }
    }
    pthread_mutex_unlock(&p_hwsync->lock);
    return ret;
}
