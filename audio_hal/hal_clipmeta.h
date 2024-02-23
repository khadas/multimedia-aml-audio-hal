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

#ifndef _HAL_CLIPMETA_H_
#define _HAL_CLIPMETA_H_

#include <stdbool.h>
#include <pthread.h>
#include <hardware/audio.h>

typedef struct dlb_buffer
{
    /** Channel count */
    unsigned    nchannel;

    /** Distance, in units of the chosen data type, between consecutive samples
     *  of the same channel. For DLB_BUFFER_OCTET_PACKED, this is in units of
     *  unsigned char. For interleaved audio data, set this equal to nchannel.
     *  For separate channel buffers, set this to 1.
     */
    unsigned long   nstride;

    /** One of the DLB_BUFFER_* codes */
    int         data_type;

    /** Array of pointers to data, one for each channel. */
    void** ppdata;
} dlb_buffer_t;

typedef struct abuffer_info {
    int sample_rate;   /** the decoded data samplerate*/
    int sample_size; /**the decoded sample bytes*/
    int output_samples;
    dlb_buffer_t *outputbuffer;
} abuffer_info_t;

#define MAX_CLIP_META 500

typedef struct  clip_tbl {
    int valid;
    unsigned long long offset;
    uint64_t clip_front;
    uint64_t clip_back;
} clip_tbl_t;

typedef struct  clip_meta {
    clip_tbl_t clip_tbl[MAX_CLIP_META];
    pthread_mutex_t mutex;
} clip_meta_t;

clip_meta_t *hal_clip_meta_init(void);
void hal_clip_meta_release(clip_meta_t *clip_meta);
void hal_clip_meta_cleanup(clip_meta_t *clip_meta);
void hal_set_clip_info(clip_meta_t *clip_meta, unsigned long long offset, uint64_t clip_front, uint64_t clip_back);
int hal_get_clip_tbl_by_offset(clip_meta_t *clip_meta, unsigned long long offset, clip_tbl_t *clip_tbl_out);
int hal_clip_data_by_samples(char *adata, int adata_size, int bpf, uint32_t clip_samples_front, uint32_t clip_samples_back);
void hal_clip_buf_by_meta(abuffer_info_t *abuff, uint64_t clip_front, uint64_t clip_back);
void hal_abuffer_clip_process(struct audio_stream_out *stream, abuffer_info_t *abuff, unsigned long long offset);

#endif

