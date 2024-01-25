/*
 * Copyright (C) 2021 Amlogic Corporation.
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



#define LOG_TAG "audio_hal_mediasync"
#define LOG_NDEBUG 0
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
#include "aml_audio_spdifout.h"
#include "audio_hw_ms12_v2.h"
#include "audio_hw.h"
#include "alsa_config_parameters.h"
#include "audio_hwsync.h"
#include "aml_esmode_sync.h"

#define DD_MUTE_FRAME_SIZE 1536
#define EAC3_IEC61937_FRAME_SIZE 24576
#define MS12_MAT_RAW_LENGTH                 (0x0f7e)
extern int aml_audio_ms12_process_wrapper(struct audio_stream_out *stream, const void *write_buf, size_t write_bytes);
#define SYNC_BUF_SIZE (DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT * EAC3_MULTIPLIER)

const unsigned int hwm_mute_dd_frame[] = {
    0x5d9c770b, 0xf0432014, 0xf3010713, 0x2020dc62, 0x4842020, 0x57100404, 0xf97c3e1f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0xf97c75fe, 0x9fcfe7f3,
    0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0x3e5f9dff, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0x48149ff2, 0x2091,
    0x361e0000, 0x78bc6ddb, 0xbbbbe3f1, 0xb8, 0x0, 0x0, 0x0, 0x77770700, 0x361e8f77, 0x359f6fdb, 0xd65a6bad, 0x5a6badb5, 0x6badb5d6, 0xa0b5d65a, 0x1e000000, 0xbc6ddb36,
    0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35, 0xa6b5d6, 0x0, 0xb66de301, 0x1e8fc7db, 0x80bbbb3b, 0x0, 0x0,
    0x0, 0x0, 0x78777777, 0xb66de3f1, 0xd65af3f9, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x5a6b, 0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0,
    0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0x605a, 0x1e000000, 0xbc6ddb36, 0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35,
    0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0xa0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0, 0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xa6b5d65a, 0x0,
    0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0, 0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0x5a6bad, 0xe3010000, 0xc7dbb66d,
    0xbb3b1e8f, 0x80bb, 0x0, 0x0, 0x0, 0x77770000, 0xe3f17877, 0xf3f9b66d, 0xadb5d65a, 0x605a6b, 0x0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0,
    0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xa0b5, 0xdb361e00, 0xf178bc6d, 0xb8bbbbe3, 0x0, 0x0, 0x0, 0x0,
    0x77777707, 0xdb361e8f, 0xad359f6f, 0xb5d65a6b, 0x10200a6, 0x0, 0xdbb6f100, 0x8fc7e36d, 0xc0dddd1d, 0x0, 0x0, 0x0, 0x0, 0xbcbbbb3b, 0xdbb6f178, 0x6badf97c,
    0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xadb5, 0xb6f10000, 0xc7e36ddb, 0xdddd1d8f, 0xc0, 0x0, 0x0, 0x0, 0xbbbb3b00, 0xb6f178bc, 0xadf97cdb, 0xb5d65a6b, 0x4deb00ad
};

const unsigned int hwm_mute_ddp_frame[] = {
    0x7f01770b, 0x20e06734, 0x2004, 0x8084500, 0x404046c, 0x1010104, 0xe7630001, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xce7f9fcf, 0x7c3e9faf,
    0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xf37f9fcf, 0x9fcfe7ab, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x53dee7f3, 0xf0e9,
    0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d,
    0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0,
    0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0,
    0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a,
    0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0,
    0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db,
    0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0,
    0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000,
    0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5,
    0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x40, 0x7f227c55,
};

/*this mat mute data is 20ms*/
static const unsigned int hwms12_muted_mat_raw[MS12_MAT_RAW_LENGTH / 4 + 1] = {
    0x4009e07,  0x3010184,   0x858085, 0xd903c422, 0x47021181,  0x8030680,  0x1089c11,    0x1091f, 0x85800104, 0x47021183,    0x10c80,  0x8c24341,       0x2f,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0, 0x1183c300,  0xc804702, 0x43410101,   0x2f08c2,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0, 0xc3000000,
    0x5efc1c5, 0x1182db03,  0x6804702, 0x9c110803,  0x91f0108,  0x1040001, 0x11838580,  0xc804702, 0x43410001,   0x2f08c2,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0, 0xc3000000, 0x47021183,  0x1010c80,  0x8c24341,       0x2f,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0, 0xc2c5c300, 0xc4c5c0c5,     0x8cba,
};


bool aml_hwsynces_insertpcm(struct audio_stream_out *stream, audio_format_t format, int time_ms, bool is_ms12)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    //struct aml_audio_patch *patch = adev->audio_patch;
    int insert_size = 0, times = 0;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    size_t out_buf_size = SYNC_BUF_SIZE;
    static char out_buf[SYNC_BUF_SIZE];
    int t1 = 0;
    int ret = 0;
    ALOGI("insert time_ms=%d ms, is_ms12=%d\n", time_ms, is_ms12);
    insert_size =  192 * time_ms;

    memset(out_buf, 0, out_buf_size);
    if (insert_size <= out_buf_size) {
        if (!is_ms12) {
            aml_hw_mixer_mixing(&adev->hw_mixer, out_buf, insert_size, format);
            if (audio_hal_data_processing(stream, out_buf, insert_size, &output_buffer,
                &output_buffer_bytes, format) == 0) {
                if (adev->useSubMix) {
                    out_write_direct_pcm(stream, output_buffer, output_buffer_bytes);
                } else {
                    hw_write(stream, output_buffer, output_buffer_bytes, format);
                }
            }
        } else {
            ret = aml_audio_ms12_process_wrapper(stream, out_buf, insert_size);
        }
        return true;
    }
    if (out_buf_size != 0)
        t1 = insert_size / out_buf_size;
    else  {
        ALOGI("fatal error out_buf_size is 0\n");
        return false;
    }
    ALOGI("set t1=%d\n", t1);
    for (int i = 0; i < t1; i++) {
        if (!is_ms12) {
            aml_hw_mixer_mixing(&adev->hw_mixer, out_buf, out_buf_size, format);
            if (audio_hal_data_processing(stream, out_buf, insert_size, &output_buffer,
                &output_buffer_bytes, format) == 0) {
                hw_write(stream, output_buffer, output_buffer_bytes, format);
            }
        } else {
            ret = aml_audio_ms12_process_wrapper(stream, out_buf, out_buf_size);
        }
    }
    return true;
}


bool aml_audio_spdif_insertpcm_es(struct audio_stream_out *stream,  void **spdifout_handle, int time_ms)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    //struct aml_audio_patch *patch = adev->audio_patch;
    int insert_size = 0;
    int t1 = 0;
    size_t out_buf_size = SYNC_BUF_SIZE;
    static char out_buf[SYNC_BUF_SIZE];
    insert_size =  192 * time_ms;

    if (insert_size <=  out_buf_size) {
        memset(out_buf, 0, out_buf_size);
        aml_audio_spdifout_processs(*spdifout_handle, out_buf, insert_size);
        return true;
    }

    if (out_buf_size != 0) {
        t1 = insert_size / out_buf_size;
    } else  {
        ALOGI("fatal error out_buf_size is 0\n");
        return false;
    }

    ALOGI("t1=%d\n", t1);

    for (int i = 0; i < t1; i++) {
        memset(out_buf, 0, out_buf_size);
        aml_audio_spdifout_processs(*spdifout_handle, out_buf, out_buf_size);
    }
    return true;
}



bool aml_hwsynces_spdif_insertraw(struct audio_stream_out *stream,  void **spdifout_handle, int time_ms, int is_packed)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    //struct aml_audio_patch *patch = adev->audio_patch;
    static unsigned char buffer[EAC3_IEC61937_FRAME_SIZE];
    int t1 = 0;
    int size = 0;
    t1 = time_ms / 32;

    memset(buffer, 0, sizeof(buffer));

    if (is_packed) {
        char *raw_buf = NULL;
        int raw_size = 0;
        raw_buf = aml_audio_get_muteframe(AUDIO_FORMAT_AC3,&raw_size,0);
        memcpy(buffer, raw_buf, raw_size);
        size = raw_size;
        ALOGI("packet dd size = %d\n",  size);
    } else {
        memcpy(buffer, hwm_mute_ddp_frame, sizeof(hwm_mute_ddp_frame));
        size =  sizeof(hwm_mute_ddp_frame);
        ALOGI("non-packet ddp size = %d\n", size);
    }
    for (int i = 0; i < t1; i++)
        aml_audio_spdifout_processs(*spdifout_handle, buffer, size);
    return  true;
}



int mediasync_nonms12_process_insert(struct audio_stream_out *stream,struct mediasync_audio_policy *p_policy)
{
    int insert_time_ms = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;

    aml_dec_t *aml_dec = aml_out->aml_dec;
    dec_data_info_t * raw_in_data = NULL;
    if (aml_dec) {
        raw_in_data  = &aml_dec->raw_in_data;
    } else {
        ALOGI("aml_dec is null, return -1\n");
        return -1;
    }
    insert_time_ms = p_policy->param1/1000;
    ALOGI("before insert :%d\n", insert_time_ms);
    do {

        aml_hwsynces_insertpcm(stream, AUDIO_FORMAT_PCM_16_BIT, 32, false);

        if (audio_is_linear_pcm(aml_dec->format) && raw_in_data->data_ch > 2) {
            aml_audio_spdif_insertpcm_es(stream, &aml_out->spdifout_handle, 32);
        }

        if (aml_out->optical_format != AUDIO_FORMAT_PCM_16_BIT) {

            if (aml_dec->format == AUDIO_FORMAT_E_AC3 || aml_dec->format == AUDIO_FORMAT_AC3) {
                if (adev->dual_spdif_support) {
                    /*output raw ddp to hdmi*/
                    if (aml_dec->format == AUDIO_FORMAT_E_AC3 &&
                        aml_out->optical_format == AUDIO_FORMAT_E_AC3) {

                        aml_hwsynces_spdif_insertraw(stream,  &aml_out->spdifout_handle,
                                                32, 0);//insert non-IEC packet
                    }

                    /*output dd data to spdif*/
                    aml_hwsynces_spdif_insertraw(stream,  &aml_out->spdifout2_handle,
                                            32, 1);
                } else {

                    aml_hwsynces_spdif_insertraw(stream,  &aml_out->spdifout_handle,
                                            32, 0);//insert non-IEC packet
                }
            }
        }

        insert_time_ms -= 32;

    } while (insert_time_ms  > 0);

    ALOGI("after insert time\n");
    return 0;
}

int mediasync_get_policy(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct mediasync_audio_policy m_audiopolicy;
    int debug_flag = aml_audio_property_get_int("audio.media.sync.util.debug", debug_flag);
    memset(&m_audiopolicy, 0, sizeof(m_audiopolicy));

    pthread_mutex_lock(&(aml_out->avsync_ctx->lock));
    audio_mediasync_t *p_mediasync = aml_out->avsync_ctx->mediasync_ctx;
    pthread_mutex_unlock(&(aml_out->avsync_ctx->lock));

    do {
        if (true == aml_out->will_pause || true == aml_out->pause_status) {
            break;
        }

        int ret = mediasync_wrap_AudioProcess(p_mediasync->handle, p_mediasync->out_start_apts, p_mediasync->cur_outapts, MEDIASYNC_UNIT_PTS, &m_audiopolicy);
        if (!ret) {
            AM_LOGE("mediasync_wrap_AudioProcess fail.");
            break;
        }

        if (debug_flag || m_audiopolicy.audiopolicy != MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
            ALOGI("m_audiopolicy=%d=%s, param1=%u, param2=%u, org_pts=0x%llx, cur_pts=0x%llx",
                m_audiopolicy.audiopolicy, mediasyncAudiopolicyType2Str(m_audiopolicy.audiopolicy),
                m_audiopolicy.param1, m_audiopolicy.param2,
                p_mediasync->out_start_apts, p_mediasync->cur_outapts);
        }

        if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD) {
            if (m_audiopolicy.param1 == -1) {
                usleep(15000);
            } else if (1000000 < m_audiopolicy.param1) {
                AM_LOGE("Invalid hold parameter, m_audiopolicy.param1:%d, change sleep to 1s now!", m_audiopolicy.param1);
                usleep(1000000);
            } else {
                usleep(m_audiopolicy.param1);
            }
        }

        if ((true == aml_out->fast_quit) || adev->ms12_to_be_cleanup) {
            AM_LOGI("fast_quit, break now.");
            break;
        }
    } while (aml_out->avsync_ctx && m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD);

    p_mediasync->apolicy.audiopolicy= (audio_policy)m_audiopolicy.audiopolicy;
    p_mediasync->apolicy.param1 = m_audiopolicy.param1;
    p_mediasync->apolicy.param2 = m_audiopolicy.param2;
    return;
}

int aml_process_resample(struct audio_stream_out *stream,
                                struct mediasync_audio_policy *p_policy,
                                bool *speed_enabled)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    float speed = 0.0f;
    int ret = -1;
    if (p_policy->param2 != 0)
        speed = ((float)(p_policy->param1)) / p_policy->param2;
    else
        ALOGI("Warning speed error\n");

    ALOGI("new speed=%f,  output_speed=%f\n", speed, aml_out->output_speed);

    if (speed != 1.0f) {
        *speed_enabled = true;

        if (speed != aml_out->output_speed) {
            ALOGE("aml_audio_set_output_speed set speed :%f --> %f.\n",
                aml_out->output_speed, speed);
        }

    } else
        *speed_enabled = false;

    aml_out->output_speed = speed;

    return 0;
}

sync_process_res mediasync_nonms12_process(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    aml_dec_t *aml_dec = aml_out->aml_dec;
    sync_process_res ret = ESSYNC_AUDIO_OUTPUT;

    struct mediasync_audio_policy *async_policy = &(aml_out->avsync_ctx->mediasync_ctx->apolicy);

    if (aml_out->alsa_status_changed) {
        AM_LOGI("aml_out->alsa_running_status %d", aml_out->alsa_running_status);
        mediasync_wrap_setParameter(aml_out->avsync_ctx->mediasync_ctx->handle, MEDIASYNC_KEY_ALSAREADY, &aml_out->alsa_running_status);
        aml_out->alsa_status_changed = false;
    }

    mediasync_get_policy(stream);
    if (true == aml_out->fast_quit) {
        return ESSYNC_AUDIO_EXIT;
    }

    switch (async_policy->audiopolicy)
    {
        case MEDIASYNC_AUDIO_DROP_PCM:
            aml_out->avsync_ctx->mediasync_ctx->cur_outapts = aml_dec->out_frame_pts;
            ret = ESSYNC_AUDIO_DROP;
            break;
        case MEDIASYNC_AUDIO_INSERT:
            mediasync_nonms12_process_insert(stream, &async_policy);
            break;
        case DTVSYNC_AUDIO_ADJUST_CLOCK:
            //aml_dtvsync_ms12_adjust_clock(stream_out, async_policy->param1);
            adev->underrun_mute_flag = false;
            break;
        case MEDIASYNC_AUDIO_RESAMPLE:
            //aml_hwsynces_process_resample(stream, &m_audiopolicy, speed_enabled);
            break;
        case MEDIASYNC_AUDIO_MUTE:
            adev->underrun_mute_flag = true;
            break;
        case MEDIASYNC_AUDIO_NORMAL_OUTPUT:
            adev->underrun_mute_flag = false;
            break;
        default :
            AM_LOGE("unknown policy:%d error!", async_policy->audiopolicy);
            break;
    }

    return ret;
}

