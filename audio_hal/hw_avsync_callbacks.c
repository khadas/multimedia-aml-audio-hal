
#define LOG_TAG "audio_hw_hal_hwsync"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <inttypes.h>
#include "hw_avsync_callbacks.h"
#include "audio_hwsync.h"
#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "aml_malloc_debug.h"

#define PTS_90K 90000LL
enum hwsync_status pcm_check_hwsync_status(uint32_t apts_gap)
{
    enum hwsync_status sync_status;

    if (apts_gap < APTS_DISCONTINUE_THRESHOLD_MIN)
        sync_status = CONTINUATION;
    else if (apts_gap > APTS_DISCONTINUE_THRESHOLD)
        sync_status = RESYNC;
    else
        sync_status = ADJUSTMENT;

    return sync_status;
}

enum hwsync_status pcm_check_hwsync_status1(uint32_t pcr, uint32_t apts)
{
    uint32_t apts_gap = get_pts_gap(pcr, apts);
    enum hwsync_status sync_status;

    if (apts >= pcr) {
        if (apts_gap < APTS_DISCONTINUE_THRESHOLD_MIN_35MS)
            sync_status = CONTINUATION;
        else if (apts_gap > APTS_DISCONTINUE_THRESHOLD)
            sync_status = RESYNC;
        else
            sync_status = ADJUSTMENT;
    } else {
        if (apts_gap < APTS_DISCONTINUE_THRESHOLD_MIN)
            sync_status = CONTINUATION;
        else if (apts_gap > APTS_DISCONTINUE_THRESHOLD)
            sync_status = RESYNC;
        else
            sync_status = ADJUSTMENT;
    }
    return sync_status;
}

int on_meta_data_cbk(void *cookie,
        uint64_t offset, struct hw_avsync_header *header, int *delay_ms)
{
    struct aml_stream_out *out = cookie;
    struct meta_data_list *mdata_list = NULL;
    struct listnode *item;
    uint32_t pts32 = 0;
    uint64_t pts = 0;
    uint64_t aligned_offset = 0;
    uint32_t frame_size = 0;
    uint32_t sample_rate = 48000;
    uint64_t pts_delta = 0;
    int ret = 0;
    uint32_t pcr = 0;
    int pcr_pts_gap = 0;
#if 0
    int32_t tunning_latency = aml_audio_get_hwsync_latency_offset(false);

    if (!cookie || !header) {
        ALOGE("NULL pointer");
        return -EINVAL;
    }
    ALOGV("%s(), pout %p", __func__, out);

    frame_size = audio_stream_out_frame_size(&out->stream);
    //sample_rate = out->audioCfg.sample_rate;
    if (out->audioCfg.sample_rate != sample_rate)
        offset = (offset * 1000) /(1000 * sample_rate / out->audioCfg.sample_rate);

    pthread_mutex_lock(&out->mdata_lock);
    if (!list_empty(&out->mdata_list)) {
        item = list_head(&out->mdata_list);
        mdata_list = node_to_item(item, struct meta_data_list, list);
        if (!mdata_list) {
            ALOGE("%s(), fatal err, no meta data!", __func__);
            ret = -EINVAL;
            goto err_lock;
        }
        header->frame_size = mdata_list->mdata.frame_size;
        header->pts = mdata_list->mdata.pts;
        aligned_offset = mdata_list->mdata.payload_offset;
        if (out->debug_stream) {
            ALOGV("%s(), offset %" PRId64 ", checkout payload offset %" PRId64 "",
                        __func__, offset, mdata_list->mdata.payload_offset);
            ALOGV("%s(), frame_size %d, pts %" PRId64 "ms",
                        __func__, header->frame_size, header->pts/PTS_90K);
        }
    }
    ALOGV("offset =%" PRId64 " aligned_offset=%" PRId64 " frame size=%d samplerate=%d", offset, aligned_offset,frame_size,sample_rate);
    if (offset >= aligned_offset && mdata_list) {
        pts = header->pts;
        pts_delta = (offset - aligned_offset) * PTS_90K/(frame_size * sample_rate);
        pts += pts_delta;
        out->last_pts = pts;
        out->last_payload_offset = offset;
        list_remove(&mdata_list->list);
        aml_audio_free(mdata_list);
        ALOGV("head pts =%" PRId64 " delta =%" PRId64 " pts =%" PRId64 " ",header->pts, pts_delta, pts);
    } else if (offset > out->last_payload_offset) {
        pts_delta = (offset - out->last_payload_offset) * PTS_90K/(frame_size * sample_rate);
        pts = out->last_pts + pts_delta;
        ALOGV("last pts=%" PRId64 " delat=%" PRId64 " pts=%" PRId64 " ", out->last_pts, pts_delta, pts);
    } else {
        ret = -EINVAL;
        goto err_lock;
    }

    pts32 = (uint32_t)pts;//(pts / 1000000 * 90);
    pthread_mutex_unlock(&out->mdata_lock);

    /*if stream is already paused, we don't need to av sync, it may cause pcr reset*/
    if (out->pause_status) {
        ALOGW("%s(), write in pause status", __func__);
        if (out->hwsync && out->hwsync->use_mediasync) {
            if(out->first_pts_set == true)
                out->first_pts_set = false;
        }
        return -EINVAL;
    }

    if (out->hwsync && out->hwsync->use_mediasync) {
        if (!out->first_pts_set) {
            int32_t latency = 0;
            int delay_count = 0;
            hwsync_header_construct(header);
            latency = (int32_t)out_get_outport_latency((struct audio_stream_out *)out) * 90;
            latency += tunning_latency * 90;
            ALOGD("%s(), set tsync start pts %d, latency %d, last position %" PRId64 "",
                __func__, pts32, latency, out->last_frames_position);
            if (latency < 0) {
                pts32 += abs(latency);
            } else {
                if (pts32 < latency) {
                    ALOGI("pts32 = %d latency=%d", pts32/90, latency);
                    return 0;
                }
                pts32 -= latency;
            }

             /*if the pts is zero, to avoid video pcr not set issue, we just set it as 1ms*/
            if (pts32 == 0) {
                pts32 = 1 * 90;
            }

            //ALOGI("%s =============== can drop============", __FUNCTION__);
            aml_hwsync_wait_video_start(out->hwsync);
            aml_hwsync_wait_video_drop(out->hwsync, pts32);
            aml_audio_hwsync_set_first_pts(out->hwsync, pts32);

            out->first_pts_set = true;
        } else {
            enum hwsync_status sync_status = CONTINUATION;
            struct hw_avsync_header_extractor *hwsync_extractor;
            struct aml_audio_device *adev = out->dev;
            uint32_t pcr = 0;
            uint32_t apts_gap;
            // adjust pts based on latency which is only the outport latency
            int32_t latency = (int32_t)out_get_outport_latency((struct audio_stream_out *)out) * 90;
            latency += tunning_latency * 90;
            // check PTS discontinue, which may happen when audio track switching
            // discontinue means PTS calculated based on first_apts and frame_write_sum
            // does not match the timestamp of next audio samples
            if (latency < 0) {
                pts32 += abs(latency);
            } else {
                if (pts32 > latency) {
                    pts32 -= latency;
                } else {
                    pts32 = 0;
                }
            }

            hwsync_extractor = out->hwsync_extractor;
        }

        /*if the pts is zero, to avoid video pcr not set issue, we just set it as 1ms*/
        if (pts32 == 0) {
            pts32 = 1 * 90;
        }

        ret = aml_hwsync_get_tsync_pts(out->hwsync, &pcr);
        pcr_pts_gap = ((int)(pts32 - pcr)) / 90;
        if (abs(pcr_pts_gap) > 50) {
            ALOGI("%s pcr =%d pts =%d diff =%d", __func__, pcr/90, pts32/90, pcr_pts_gap);
        }
        return 0;
    }


    /*old sync method*/
    if (!out->first_pts_set) {
        int32_t latency = 0;
        hwsync_header_construct(header);
        latency = (int32_t)out_get_outport_latency((struct audio_stream_out *)out) * 90;
        latency += tunning_latency * 90;
        ALOGD("%s(), set tsync start pts %d, latency %d, last position %" PRId64 "",
            __func__, pts32, latency, out->last_frames_position);
        if (latency < 0) {
            pts32 += abs(latency);
        } else {
            if (pts32 < latency) {
                ALOGI("pts32 = %d latency=%d", pts32/90, latency);
                return 0;
            }
            pts32 -= latency;
        }
        aml_audio_hwsync_set_first_pts(out->hwsync, pts32);
        out->first_pts_set = true;
        //*delay_ms = 40;
    } else {
        if (out->msync_session && (pts32 != HWSYNC_PTS_NA)) {
            struct audio_policy policy;
            uint32_t latency = out_get_outport_latency((struct audio_stream_out *)out) * 90;
            uint32_t apts32 = (pts32 - latency) & 0xffffffff;
            av_sync_audio_render(out->msync_session, apts32, &policy);
            /* TODO: for non-amaster mode, handle sync policy on audio side */
        }
    }

    return 0;
err_lock:
    pthread_mutex_unlock(&out->mdata_lock);
#endif
    return ret;
}

