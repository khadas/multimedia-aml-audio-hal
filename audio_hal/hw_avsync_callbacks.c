
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
    return ret;
}

