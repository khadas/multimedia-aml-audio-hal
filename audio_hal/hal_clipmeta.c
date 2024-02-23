/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#include <string.h>
#include <stdlib.h>

#define LOG_TAG "audio_hw_primary"
#include <cutils/log.h>
#include <time.h>

#include "hal_clipmeta.h"
#include "audio_hw_utils.h"
#include "aml_android_utils.h"

/*|->..............org data buffer.............<-|*
 *|->clip_front<-|..remain buffer..|->clip_back<-|*/


int hal_clip_data_by_samples(char *adata, int adata_size, int bpf, uint32_t clip_samples_front, uint32_t clip_samples_back)
{
    char *p_sfract_start = NULL;
    char *p_sfract_out   = adata;
    int remain_size      = adata_size;
    int remain_samples   = adata_size / bpf;
    int front_size = 0;
    int back_size  = 0;
    int total_size = adata_size;

    //Skip any samples that need skipping
    if (0 != clip_samples_front)
    {
        front_size  = clip_samples_front * bpf;
        remain_size = total_size - front_size;
        remain_samples -= clip_samples_front;
        p_sfract_start = p_sfract_out + front_size;
        memmove(p_sfract_out, p_sfract_start, remain_size);
    }

    if (0 != clip_samples_back)
    {
        back_size   = clip_samples_back * bpf;
        remain_size = (remain_size > back_size) ? (remain_size - back_size) : 0;
        remain_samples = (remain_samples > clip_samples_back) ? (remain_samples - clip_samples_back) : 0;
        memset(p_sfract_out + remain_size, 0, (total_size - front_size - remain_size));
    }

    return remain_samples;
}

void hal_clip_buf_by_meta(abuffer_info_t *abuff, uint64_t clip_front, uint64_t clip_back)
{
    if ((NULL == abuff) || ((0 == clip_front) && (0 == clip_back)))
    {
        ALOGE("[%s,%d], abuff:%p, clip_front:%lld, clip_back:%lld", __func__, __LINE__, abuff, clip_front, clip_back);
        return;
    }

    if ((NULL == abuff->outputbuffer) || (0 == abuff->output_samples))
    {
        ALOGE("[%s,%d], outputbuffer:%p, output_samples:%d", __func__, __LINE__, abuff->outputbuffer, abuff->output_samples);
        return;
    }

    int remain_samples = 0;
    int org_samples = abuff->output_samples;
    int sample_rate = abuff->sample_rate;
    int sample_size = abuff->sample_size;
    int total_size  = abuff->output_samples * sample_size;
    dlb_buffer_t *pwritebuff = (dlb_buffer_t*)abuff->outputbuffer;
    int nchannel             = pwritebuff->nchannel;
    uint32_t discarded_samples_from_front = clip_front * sample_rate / 1000000000;   //clip_front (ns)
    uint32_t discarded_samples_from_end   = clip_back * sample_rate / 1000000000;

    ALOGI("[%s:%d] IN, front_ns(%lld), back_ns(%lld), total_size:%d, output_samples:%d, sample_size:%d, sample_rate:%d", __FUNCTION__, __LINE__,
        clip_front, clip_back, total_size, abuff->output_samples, abuff->sample_size, sample_rate);

    for (int ch = 0; ch < nchannel; ch++)
    {
        char *p_sfract_out = (char*)pwritebuff->ppdata[ch];
        remain_samples = hal_clip_data_by_samples(p_sfract_out, total_size, sample_size, discarded_samples_from_front, discarded_samples_from_end);
    }

    //update output samples
    abuff->output_samples = remain_samples;

    ALOGI("[%s:%d] OUT, front(%lld ns, %d), back(%lld ns, %d), total_size:%d, org_samples:%d, remain_samples:%d, sample_rate:%d", __FUNCTION__, __LINE__,
        clip_front, discarded_samples_from_front, clip_back, discarded_samples_from_end,
        total_size, org_samples, remain_samples, sample_rate);

    return;
}

clip_meta_t *hal_clip_meta_init(void)
{
    clip_meta_t *clip_meta = aml_audio_malloc(sizeof(clip_meta_t));
    if (NULL == clip_meta)
    {
        ALOGE("[%s:%d], size:%d, malloc fail!!!", __func__, __LINE__, sizeof(clip_meta_t));
        return NULL;
    }

    memset(clip_meta->clip_tbl, 0, (sizeof(clip_tbl_t) * MAX_CLIP_META));

    pthread_mutex_init(&clip_meta->mutex, NULL);
    ALOGI("[%s:%d], init done", __func__, __LINE__);
    return clip_meta;
}

void hal_clip_meta_release(clip_meta_t *clip_meta)
{
    if (NULL == clip_meta)
    {
        ALOGE("[%s:%d], already free!!", __func__, __LINE__);
        return;
    }

    aml_audio_free(clip_meta);
    ALOGI("[%s:%d], release done", __func__, __LINE__);

    return;
}

void hal_clip_meta_cleanup(clip_meta_t *clip_meta)
{
    if (NULL == clip_meta)
    {
        return;
    }

    pthread_mutex_lock(&(clip_meta->mutex));
    memset(clip_meta->clip_tbl, 0, (sizeof(clip_tbl_t) * MAX_CLIP_META));
    pthread_mutex_unlock(&(clip_meta->mutex));
    ALOGI("[%s:%d], cleanup done", __func__, __LINE__);

    return;
}

void hal_set_clip_info(clip_meta_t *clip_meta, unsigned long long offset, uint64_t clip_front, uint64_t clip_back)
{
    int ret = 0;
    int i = 0;
    clip_meta_t *clip_meta_tmp = clip_meta;

    if (NULL == clip_meta)
    {
        ALOGE("[%s:%d], clip_meta NULL!!", __func__, __LINE__);
        return;
    }
    clip_tbl_t *clip_tbl = clip_meta->clip_tbl;

    pthread_mutex_lock(&(clip_meta->mutex));
    for (i = 0; i < MAX_CLIP_META; i++)
    {
        if (0 == clip_tbl[i].valid)
        {
            clip_tbl[i].offset     = offset;
            clip_tbl[i].clip_front = clip_front;
            clip_tbl[i].clip_back  = clip_back;
            clip_tbl[i].valid = 1;
            break;
        }
    }
    pthread_mutex_unlock(&(clip_meta->mutex));

    if (MAX_CLIP_META == i)
    {
        ALOGE("[%s:%d], clip tbl overflow!!", __func__, __LINE__);
    }

    ALOGI("[%s:%d], i:%d, offset:%lld, clip_front:%lld, clip_back:%lld", __func__, __LINE__, i, offset, clip_front, clip_back);
    return;
}

int hal_get_clip_tbl_by_offset(clip_meta_t *clip_meta, unsigned long long offset, clip_tbl_t *clip_tbl_out)
{
    int ret = 0;
    int i   = 0;
    unsigned long long min_offset = offset;
    int match_i = MAX_CLIP_META;
    clip_meta_t *clip_meta_tmp = clip_meta;
    if (NULL == clip_meta)
    {
        ALOGI("[%s:%d], clip_meta NULL ", __func__, __LINE__);
        return -1;
    }

    clip_tbl_t *clip_tbl = clip_meta->clip_tbl;
    pthread_mutex_lock(&(clip_meta->mutex));
    for (i = 0; i < MAX_CLIP_META; i++)
    {
        if (clip_tbl[i].valid) {
            //AM_LOGI("GX, i:%d, offset:%lld, clip_tbl[i].offset:%lld", i, offset, clip_tbl[i].offset);
        }
        if ((clip_tbl[i].valid) && (min_offset >= clip_tbl[i].offset))
        {
            min_offset = clip_tbl[i].offset;
            match_i = i;
            //break;
        }
    }

    if (MAX_CLIP_META != match_i)
    {
        clip_tbl[match_i].valid  = 0;
        clip_tbl_out->clip_front = clip_tbl[match_i].clip_front;
        clip_tbl_out->clip_back  = clip_tbl[match_i].clip_back;
        ALOGI("[%s:%d]match_i:%d, offset:%lld, clip_tbl[%d].offset:%lld, clip_front:%lld, clip_back:%lld", __func__, __LINE__,
                match_i, offset, match_i, clip_tbl[match_i].offset, clip_tbl_out->clip_front, clip_tbl_out->clip_back);
    }
    else
    {
        clip_tbl_out->clip_front = 0;
        clip_tbl_out->clip_back  = 0;
        ret = -1;
    }

    pthread_mutex_unlock(&(clip_meta->mutex));
    return ret;
}

void hal_abuffer_clip_process(struct audio_stream_out *stream, abuffer_info_t *abuff, unsigned long long offset)
{
    int ret = 0;
    uint64_t consume_payload = 0;
    clip_tbl_t clip_tbl_out;
    memset(&clip_tbl_out, 0, sizeof(clip_tbl_t));
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    clip_meta_t *clip_meta = aml_out->clip_meta;

    /*get decoded frame and its pts*/
    consume_payload = dolby_ms12_get_main_bytes_consumed(stream);

    /*main pcm is resampled out of ms12, so the payload size is changed*/
    if (audio_is_linear_pcm(aml_out->hal_internal_format) && aml_out->hal_rate != 48000) {
        consume_payload = consume_payload * aml_out->hal_rate / 48000;
    }

    AM_LOGI("consume_payload:%lld", consume_payload);

    ret = hal_get_clip_tbl_by_offset(clip_meta, consume_payload, &clip_tbl_out);
    if (0 != ret)
    {
        return;
    }

    hal_clip_buf_by_meta(abuff, clip_tbl_out.clip_front, clip_tbl_out.clip_back);
    return;
}

