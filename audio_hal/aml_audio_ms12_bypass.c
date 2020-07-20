/*
 * Copyright (C) 2020 Amlogic Corporation.
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
#define LOG_TAG "audio_ms12_bypass"

#include <cutils/log.h>
#include <inttypes.h>
#include "audio_hw.h"
#include "aml_audio_ms12_bypass.h"

#define BYPASS_MS12_MAX_FRAME_SIZE (32768)
#define DDP_FRAME_BLK_NUM          (6)


struct bypass_frame_item {
    struct listnode list;
    uint32_t frame_size;
    void *   frame_buf;
    uint32_t frame_cnt;
    uint64_t offset_start;
    uint64_t offset_end;
    uint32_t numblks;
    struct bypass_frame_info info;
};

struct aml_ms12_bypass_handle {
    void *  buf;
    uint32_t buf_size;
    uint64_t data_offset;
    struct listnode frame_list;
    pthread_mutex_t list_lock;
};

static void delete_bypass_frame(struct bypass_frame_item * frame)
{
    if (frame) {
        if (frame->frame_buf) {
            free(frame->frame_buf);
        }
        free(frame);
    }
    return;
}

static struct bypass_frame_item * new_bypass_frame(const void *buffer, int32_t numBytes, struct bypass_frame_info * data_info)
{
    struct bypass_frame_item *frame = NULL;
    /*malloc frame item*/
    frame = (struct bypass_frame_item *)calloc(1, sizeof(struct bypass_frame_item));
    if (frame == NULL) {
        goto exit;
    }
    memcpy(&frame->info, data_info, sizeof(struct bypass_frame_info));

    /*malloc buffer*/
    frame->frame_buf = calloc(1, numBytes);
    memcpy(frame->frame_buf, buffer, numBytes);
    frame->frame_size = numBytes;

    frame->frame_cnt  = 1;

    return frame;
exit:
    delete_bypass_frame(frame);

    ALOGE("%s failed", __FUNCTION__);
    return NULL;
}

static struct bypass_frame_item * modify_bypass_frame(struct bypass_frame_item * frame, const void *buffer, int32_t numBytes)
{
    uint32_t new_frame_size = frame->frame_size + numBytes;
    frame->frame_buf = realloc(frame->frame_buf, new_frame_size);
    if (frame->frame_buf == NULL) {
        ALOGE("realloc size =%d failed", new_frame_size);
        return NULL;
    }
    memcpy((char *)frame->frame_buf + frame->frame_size, buffer, numBytes);
    frame->frame_size = new_frame_size;
    frame->frame_cnt++;

    return frame;
}

int aml_ms12_bypass_open(void **pphandle)
{
    struct aml_ms12_bypass_handle *bypass_hanlde = NULL;

    bypass_hanlde = (struct aml_ms12_bypass_handle *)calloc(1, sizeof(struct aml_ms12_bypass_handle));
    if (bypass_hanlde == NULL) {
        ALOGE("%s handle error", __func__);
        goto error;
    }

    bypass_hanlde->buf_size  = BYPASS_MS12_MAX_FRAME_SIZE;
    bypass_hanlde->buf  = calloc(1, BYPASS_MS12_MAX_FRAME_SIZE);
    if (bypass_hanlde->buf == NULL) {
        ALOGE("%s data buffer error", __func__);
        goto error;
    }
    bypass_hanlde->data_offset = 0;
    list_init(&bypass_hanlde->frame_list);
    pthread_mutex_init(&bypass_hanlde->list_lock, NULL);
    *pphandle = bypass_hanlde;
    ALOGI("%s exit =%p", __func__, bypass_hanlde);
    return 0;
error:
    if (bypass_hanlde) {
        free(bypass_hanlde);
    }
    *pphandle = NULL;
    ALOGE("%s error", __func__);
    return -1;
}


int aml_ms12_bypass_close(void *phandle)
{
    struct aml_ms12_bypass_handle *bypass_hanlde = (struct aml_ms12_bypass_handle *)phandle;
    struct bypass_frame_item *frame_item = NULL;
    struct listnode *item;

    if (bypass_hanlde) {
        pthread_mutex_lock(&bypass_hanlde->list_lock);
        while (!list_empty(&bypass_hanlde->frame_list)) {
            item = list_head(&bypass_hanlde->frame_list);
            frame_item = node_to_item(item, struct bypass_frame_item, list);
            list_remove(item);
            delete_bypass_frame(frame_item);
        }
        pthread_mutex_unlock(&bypass_hanlde->list_lock);

        if (bypass_hanlde->buf) {
            free(bypass_hanlde->buf);
        }
        free(bypass_hanlde);
    }
    ALOGI("%s exit", __func__);
    return 0;
}

int aml_ms12_bypass_reset(void *phandle)
{
    struct aml_ms12_bypass_handle *bypass_hanlde = (struct aml_ms12_bypass_handle *)phandle;
    struct bypass_frame_item *frame_item = NULL;
    struct listnode *item = NULL;
    uint32_t frame_no = 0;

    if (bypass_hanlde) {
        pthread_mutex_lock(&bypass_hanlde->list_lock);
        while (!list_empty(&bypass_hanlde->frame_list)) {
            item = list_head(&bypass_hanlde->frame_list);
            frame_item = node_to_item(item, struct bypass_frame_item, list);
            list_remove(item);
            delete_bypass_frame(frame_item);
            frame_no++;
        }
        pthread_mutex_unlock(&bypass_hanlde->list_lock);
        bypass_hanlde->data_offset = 0;
    }
    ALOGI("%s exit release frame number=%d", __FUNCTION__, frame_no);
    return 0;
}

int aml_ms12_bypass_checkin_data(void *phandle, const void *buffer, int32_t numBytes, struct bypass_frame_info * data_info)
{
    int ret = 0;
    struct bypass_frame_item * new_frame = NULL;
    struct bypass_frame_item * last_frame = NULL;
    struct listnode *item = NULL;
    bool need_new_frame = true;
    struct aml_ms12_bypass_handle *bypass_hanlde = (struct aml_ms12_bypass_handle *)phandle;
    if ((bypass_hanlde == NULL) ||
        (buffer == NULL)        ||
        (numBytes == 0)         ||
        (data_info == NULL)) {
        ALOGE("%s Invalid parameter", __FUNCTION__);
        return -1;
    }
    ALOGV("frame info rate=%d dependency=%d numblks=%d", data_info->samplerate, data_info->dependency_frame, data_info->numblks);
    pthread_mutex_lock(&bypass_hanlde->list_lock);
    if (!data_info->dependency_frame) {
        if (!list_empty(&bypass_hanlde->frame_list)) {
            item      = list_tail(&bypass_hanlde->frame_list);
            last_frame = node_to_item(item, struct bypass_frame_item, list);
            /*add this buffer to last frame*/
            if (last_frame->numblks < DDP_FRAME_BLK_NUM) {
                modify_bypass_frame(last_frame, buffer, numBytes);
                last_frame->offset_end   = bypass_hanlde->data_offset + numBytes;
                last_frame->numblks     += data_info->numblks;
                bypass_hanlde->data_offset += numBytes;
                need_new_frame = false;
            }
        }
        if (need_new_frame)
        {
            new_frame = new_bypass_frame(buffer, numBytes, data_info);
            if (new_frame) {
                new_frame->offset_start = bypass_hanlde->data_offset;
                new_frame->offset_end   = bypass_hanlde->data_offset + numBytes;
                new_frame->numblks      = data_info->numblks;
                list_add_tail(&bypass_hanlde->frame_list, &new_frame->list);
                bypass_hanlde->data_offset += numBytes;
            } else {
                ret = -1;
            }
        }
    } else {
        if (!list_empty(&bypass_hanlde->frame_list)) {
            item      = list_tail(&bypass_hanlde->frame_list);
            new_frame = node_to_item(item, struct bypass_frame_item, list);
            modify_bypass_frame(new_frame, buffer, numBytes);
            new_frame->offset_end   = bypass_hanlde->data_offset + numBytes;
            new_frame->numblks      = data_info->numblks;
            bypass_hanlde->data_offset += numBytes;
        }
    }
    pthread_mutex_unlock(&bypass_hanlde->list_lock);

    if (new_frame) {
        ALOGV("check in bypass frame start=%lld end=%lld size=%d depedency=%d", new_frame->offset_start, new_frame->offset_end, numBytes, new_frame->info.dependency_frame);
    }
    return ret;
}


int aml_ms12_bypass_checkout_data(void *phandle, void **output_buf, int32_t *out_size, uint64_t offset, struct bypass_frame_info *frame_info)
{
    struct aml_ms12_bypass_handle *bypass_hanlde = (struct aml_ms12_bypass_handle *)phandle;
    struct bypass_frame_item *frame_item = NULL;
    struct listnode *item = NULL;
    uint32_t frame_size = 0;
    bool find_frame = false;
    if (bypass_hanlde == NULL) {
        return -1;
    }
    ALOGV("check out bypass data =%lld", offset);
    pthread_mutex_lock(&bypass_hanlde->list_lock);
    list_for_each(item, &bypass_hanlde->frame_list) {
        frame_item = node_to_item(item, struct bypass_frame_item, list);
        if ((frame_item->offset_start <= offset) && (offset <= frame_item->offset_end)) {
            /*find the offset frame*/
            frame_size = frame_item->frame_size;
            ALOGV("offset=%lld frame size=%d start=%lld end=%lld frame dependency=%d cnt=%d numblks=%d", offset, frame_size, frame_item->offset_start, frame_item->offset_end, frame_item->info.dependency_frame, frame_item->frame_cnt, frame_item->numblks);
            if (frame_size > bypass_hanlde->buf_size) {
                bypass_hanlde->buf = realloc(bypass_hanlde->buf, frame_size);
                if (bypass_hanlde->buf == NULL) {
                    ALOGE("%s realloc buf failed =%d", __FUNCTION__, frame_size);
                    goto error;
                }
                bypass_hanlde->buf_size = frame_size;
            }
            memcpy(bypass_hanlde->buf, frame_item->frame_buf, frame_size);
            memcpy(frame_info, &frame_item->info, sizeof(struct bypass_frame_info));

            *output_buf = bypass_hanlde->buf;
            *out_size   = frame_size;
            find_frame = true;
            list_remove(&frame_item->list);
            delete_bypass_frame(frame_item);
            break;
        }
    }

    if (find_frame) {
        pthread_mutex_unlock(&bypass_hanlde->list_lock);
        return 0;
    }
error:
    pthread_mutex_unlock(&bypass_hanlde->list_lock);
    *output_buf = NULL;
    *out_size   = 0;
    return -1;
}


