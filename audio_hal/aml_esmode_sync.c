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



#define LOG_TAG "aml_hwsynces"
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
#include "audio_media_sync_util.h"

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



int aml_hwmediasync_nonms12_process_insert(struct audio_stream_out *stream,struct mediasync_audio_policy *p_policy)
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

int aml_hwsynces_ms12_process_insert(void *priv_data, int insert_time_ms,
                                    aml_ms12_dec_info_t *ms12_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct bitstream_out_desc *bitstream_out;
    unsigned char buffer[EAC3_IEC61937_FRAME_SIZE];
    audio_format_t output_format = (ms12_info) ? ms12_info->data_type : AUDIO_FORMAT_PCM_16_BIT;
    int t1 = 0;
    int i = 0;
    int insert_ms = 0;
    int size = 0;
    bool is_mat = false;
    size_t out_buf_size = SYNC_BUF_SIZE;
    static char out_buf[SYNC_BUF_SIZE];

    for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {
        bitstream_out = &ms12->bitstream_out[i];
        if ((bitstream_out->audio_format == AUDIO_FORMAT_AC3) ||
            (bitstream_out->audio_format == AUDIO_FORMAT_E_AC3)) {
            t1 = insert_time_ms / 32;
            break;
        } else if (bitstream_out->audio_format == AUDIO_FORMAT_MAT) {
            t1 = insert_time_ms / 20;
            is_mat = true;
        }
    }

    if (is_mat)
        insert_ms = 20;
    else
        insert_ms = 32;

    ALOGI("inset_time_ms=%d, insert_ms=%d, t1=%d, is_mat=%d\n",
            insert_time_ms, insert_ms, t1, is_mat);

    memset(out_buf, 0, out_buf_size);

    do {

        if (audio_is_linear_pcm(output_format)) {

            if (ms12_info->pcm_type == DAP_LPCM) {
                dap_pcm_output(out_buf, priv_data, 192*insert_ms, ms12_info);
            } else {
                stereo_pcm_output(out_buf, priv_data, 192*insert_ms, ms12_info);
            }
        } else {
            if (is_mat) {
                size = sizeof(hwms12_muted_mat_raw);
                memcpy(buffer, hwms12_muted_mat_raw, size);
                mat_bitstream_output(buffer, priv_data, size);
            } else {
                for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {

                    bitstream_out = &ms12->bitstream_out[i];

                    if (bitstream_out->spdifout_handle != NULL) {
                        if (bitstream_out->audio_format == AUDIO_FORMAT_E_AC3) {
                            size = sizeof(hwm_mute_ddp_frame);
                            memcpy(buffer,  hwm_mute_ddp_frame, size);
                            bitstream_output(buffer, priv_data, size);
                        } else if (bitstream_out->audio_format == AUDIO_FORMAT_AC3) {
                            size = sizeof(hwm_mute_dd_frame);
                            memcpy(buffer,  hwm_mute_dd_frame, size);
                            spdif_bitstream_output(buffer, priv_data, size);
                        }
                    }
                }
            }
        }
        insert_time_ms -= insert_ms;
    } while(insert_time_ms > 0);

    return 0;
}

sync_process_res aml_hwsynces_ms12_process_policy1(void *priv_data, aml_ms12_dec_info_t *ms12_info, struct aml_stream_out *aml_out_write)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;//ms12out
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    struct aml_audio_device *adev = aml_out->dev;
    struct mediasync_audio_policy *async_policy = NULL;
    struct renderpts_item *frame_item = NULL;
    struct listnode *item = NULL;

    pthread_mutex_lock(&adev->ms12.p_pts_list->list_lock);
    if (!list_empty(&adev->ms12.p_pts_list->frame_list)) {
        item = list_head(&adev->ms12.p_pts_list->frame_list);
        frame_item = node_to_item(item, struct renderpts_item, list);
        list_remove(item);
    }
    pthread_mutex_unlock(&adev->ms12.p_pts_list->list_lock);

    if (NULL != frame_item)
    {
        async_policy = &(aml_out_write->hwsync->es_mediasync.apolicy);
        do {
            mediasync_wrap_AudioProcess(aml_out_write->hwsync->es_mediasync.mediasync, frame_item->out_start_apts, frame_item->cur_outapts, MEDIASYNC_UNIT_PTS, async_policy);
            if (adev->debug_flag > 0)
                ALOGI("ms12_process_policy_14=%d=%s, param1=%u, param2=%u, ori=0x%llx,curout=0x%llx \n",
                    async_policy->audiopolicy, mediasyncAudiopolicyType2Str(async_policy->audiopolicy),
                    async_policy->param1, async_policy->param2,
                    frame_item->out_start_apts, frame_item->cur_outapts);
            if (async_policy->audiopolicy == MEDIASYNC_AUDIO_HOLD) {
                if (async_policy->param1 > 15000 || async_policy->param1 < 1) {
                    async_policy->param1 = 15000;
                }
                usleep(async_policy->param1);
            }
        } while (aml_out_write->hwsync && (aml_out_write->pause_status == false) && async_policy->audiopolicy == MEDIASYNC_AUDIO_HOLD);

        aml_audio_free(frame_item);
        if (async_policy->audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {

            return ESSYNC_AUDIO_DROP;

        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_INSERT) {

            aml_hwsynces_ms12_process_insert(priv_data, async_policy->param1/1000, ms12_info);

        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_ADJUST_CLOCK) {


        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_RESAMPLE) {


        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_MUTE) {

            adev->underrun_mute_flag = true;

        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
            adev->underrun_mute_flag = false;
        }
        async_policy->audiopolicy = MEDIASYNC_AUDIO_UNKNOWN;
    }
    return ESSYNC_AUDIO_OUTPUT;
}


void aml_hwsynces_ms12_get_policy_continue(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct mediasync_audio_policy m_audiopolicy;
    memset(&m_audiopolicy, 0, sizeof(m_audiopolicy));
    pthread_mutex_lock(&aml_out->hwsync->lock);
    audio_hwsync_mediasync_t *p_esmediasync = &(aml_out->hwsync->es_mediasync);
    pthread_mutex_unlock(&aml_out->hwsync->lock);
    mediasync_wrap_AudioProcess(p_esmediasync->mediasync, p_esmediasync->out_start_apts, p_esmediasync->cur_outapts, MEDIASYNC_UNIT_PTS, &m_audiopolicy);
    if (adev->debug_flag > 0) {
        static int64_t beforepts = 0;
        static int64_t beforepts1 = 0;
        ALOGI("%s, do ms12_1 get es m_audiopolicy=%d=%s, param1=%u, param2=%u, cur_pts=0x%llx,cur_outpts=0x%llx,diff_cur_pts=%lld,diff_cur_outpts=%lld \n", __func__,
            m_audiopolicy.audiopolicy, mediasyncAudiopolicyType2Str(m_audiopolicy.audiopolicy),
            m_audiopolicy.param1, m_audiopolicy.param2,
            p_esmediasync->out_start_apts, p_esmediasync->cur_outapts, p_esmediasync->out_start_apts - beforepts1, p_esmediasync->cur_outapts - beforepts);
       beforepts = p_esmediasync->cur_outapts;
       beforepts1 = p_esmediasync->out_start_apts;
    }
    p_esmediasync->apolicy.audiopolicy= (audio_policy)m_audiopolicy.audiopolicy;
    p_esmediasync->apolicy.param1 = m_audiopolicy.param1;
    p_esmediasync->apolicy.param2 = m_audiopolicy.param2;
}

sync_process_res aml_hwsynces_ms12_process_policy_continue(void *priv_data, aml_ms12_dec_info_t *ms12_info, struct aml_stream_out *aml_out_write)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;//ms12out
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    struct aml_audio_device *adev = aml_out->dev;
    struct mediasync_a_policy *async_policy = NULL;
    audio_mediasync_util_t* media_sync_util = aml_audio_get_mediasync_util_handle();

    pthread_mutex_lock(&aml_out_write->hwsync->lock);
    async_policy = &(aml_out_write->hwsync->es_mediasync.apolicy);
    pthread_mutex_unlock(&aml_out_write->hwsync->lock);

    if (media_sync_util->media_sync_debug || async_policy->audiopolicy != MEDIASYNC_AUDIO_NORMAL_OUTPUT)
        ALOGI("[%s:%d]es cur policy:%d(%s),last policy:%d(%s), prm1:%d, prm2:%d\n", __FUNCTION__, __LINE__,
                async_policy->audiopolicy, mediasyncAudiopolicyType2Str(async_policy->audiopolicy),
                async_policy->last_audiopolicy, mediasyncAudiopolicyType2Str(async_policy->last_audiopolicy),
                async_policy->param1, async_policy->param2);

    if (async_policy->audiopolicy == MEDIASYNC_AUDIO_DROP_PCM && async_policy->last_audiopolicy != MEDIASYNC_AUDIO_DROP_PCM) {
        set_dolby_ms12_runtime_sync(&(adev->ms12), -1);//set drop policy to ms12
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_INSERT && async_policy->last_audiopolicy != MEDIASYNC_AUDIO_INSERT) {
        clock_gettime(CLOCK_MONOTONIC, &media_sync_util->start_insert_time);
        media_sync_util->insert_time_ms = async_policy->param1/1000;
        set_dolby_ms12_runtime_sync(&(adev->ms12), 1);//set insert policy to ms12
        //aml_dtvsync_ms12_process_insert(priv_data, async_policy->param1/1000, ms12_info);
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_HOLD && async_policy->last_audiopolicy != MEDIASYNC_AUDIO_HOLD) {
        set_dolby_ms12_runtime_sync(&(adev->ms12), 1);//set insert policy to ms12
    }else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_NORMAL_OUTPUT && async_policy->last_audiopolicy != MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
        set_dolby_ms12_runtime_sync(&(adev->ms12), 0);//set normal output policy to ms12
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_ADJUST_CLOCK) {
        aml_dtvsync_ms12_adjust_clock(stream_out, async_policy->param1);
        adev->underrun_mute_flag = false;
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_RESAMPLE) {
        //aml_dtvsync_ms12_process_resample(stream, async_policy);
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_MUTE) {
        adev->underrun_mute_flag = true;
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
        adev->underrun_mute_flag = false;
    }

    async_policy->last_audiopolicy = async_policy->audiopolicy;

    return ESSYNC_AUDIO_OUTPUT;
}

void aml_hwsynces_ms12_get_policy(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct mediasync_audio_policy m_audiopolicy;
    int debug_flag = aml_audio_property_get_int("audio.media.sync.util.debug", debug_flag);
    memset(&m_audiopolicy, 0, sizeof(m_audiopolicy));

    if (!(MEDIA_SYNC_ESMODE(aml_out)))
    {
        return;
    }

    pthread_mutex_lock(&aml_out->hwsync->lock);
    audio_hwsync_mediasync_t *p_esmediasync = &(aml_out->hwsync->es_mediasync);
    //p_esmediasync->out_start_apts = aml_out->hwsync->es_mediasync.in_apts;
    pthread_mutex_unlock(&aml_out->hwsync->lock);

    do {
        if (true == aml_out->will_pause || true == aml_out->pause_status) {
            return;
        }

        int ret = mediasync_wrap_AudioProcess(p_esmediasync->mediasync, p_esmediasync->out_start_apts, p_esmediasync->cur_outapts, MEDIASYNC_UNIT_PTS, &m_audiopolicy);
        if (!ret) {
            AM_LOGE("aml_dtvsync_audioprocess fail.");
            return;
        }

        if (debug_flag || m_audiopolicy.audiopolicy != MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
            ALOGI("ts m_audiopolicy=%d=%s, param1=%u, param2=%u, org_pts=0x%llx, cur_pts=0x%llx",
                m_audiopolicy.audiopolicy, mediasyncAudiopolicyType2Str(m_audiopolicy.audiopolicy),
                m_audiopolicy.param1, m_audiopolicy.param2,
                p_esmediasync->out_start_apts, p_esmediasync->cur_outapts);
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
    } while (aml_out->hwsync && m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD);

    p_esmediasync->apolicy.audiopolicy= (audio_policy)m_audiopolicy.audiopolicy;
    p_esmediasync->apolicy.param1 = m_audiopolicy.param1;
    p_esmediasync->apolicy.param2 = m_audiopolicy.param2;
    return;
}

sync_process_res aml_hwsynces_ms12_process_policy(void *priv_data, aml_ms12_dec_info_t *ms12_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;//ms12out
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    struct aml_audio_device *adev = aml_out->dev;
    struct mediasync_a_policy *async_policy = NULL;

    if (!(MEDIA_SYNC_ESMODE(aml_out)))
    {
        ALOGE("aml_out->hwsync:%p, aml_out->hw_sync_mode:%d, aml_out->avsync_type:%d error!", aml_out->hwsync, aml_out->hw_sync_mode, aml_out->avsync_type);
        return ESSYNC_AUDIO_OUTPUT;
    }

    pthread_mutex_lock(&aml_out->hwsync->lock);
    async_policy = &(aml_out->hwsync->es_mediasync.apolicy);
    pthread_mutex_unlock(&aml_out->hwsync->lock);

    if (adev->debug_flag)
        ALOGI("[%s:%d] es cur_policy:%d, prm1:%d, prm2:%d", __func__, __LINE__, async_policy->audiopolicy, async_policy->param1, async_policy->param2);

    if (async_policy->audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {
        return ESSYNC_AUDIO_DROP;
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_INSERT) {
        aml_hwsynces_ms12_process_insert(priv_data, async_policy->param1/1000, ms12_info);
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_ADJUST_CLOCK) {
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_RESAMPLE) {
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_MUTE) {
        adev->underrun_mute_flag = true;
    } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
        adev->underrun_mute_flag = false;
    }

    if (async_policy) {
        async_policy->audiopolicy = DTVSYNC_AUDIO_UNKNOWN;
    }

    return ESSYNC_AUDIO_OUTPUT;
}

int aml_audio_get_cur_ms12_latencyes(struct audio_stream_out *stream) {

    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    //struct aml_audio_patch *patch = adev->audio_patch;
    //aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    int ms12_latencyms = 0;

    uint64_t inputnode_consumed = dolby_ms12_get_main_bytes_consumed(stream);
    uint64_t frames_generated = dolby_ms12_get_main_pcm_generated(stream);
    if (is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)) {
        //if (demux_info->dual_decoder_support)
        //    ms12_latencyms = (frames_generated - ms12->master_pcm_frames) / 48;
        //else
            ms12_latencyms = (ms12->ms12_main_input_size - inputnode_consumed) / aml_out->ddp_frame_size * 32 + (frames_generated - ms12->master_pcm_frames) / 48;
    } else {
        ms12_latencyms = ((ms12->ms12_main_input_size - inputnode_consumed ) / 4 + frames_generated - ms12->master_pcm_frames) / 48;
    }
    if (adev->debug_flag)
        ALOGI("ms12_latencyms %d  ms12_main_input_size %lld inputnode_consumed %lld frames_generated %lld master_pcm_frames %lld,audio format %d\n",
        ms12_latencyms, ms12->ms12_main_input_size, inputnode_consumed,frames_generated, ms12->master_pcm_frames,aml_out->hal_internal_format);
    return ms12_latencyms;

}

int aml_hwsynces_process_resample(struct audio_stream_out *stream,
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


sync_process_res  aml_hwmediasync_nonms12_process(struct audio_stream_out *stream, int duration, bool *speed_enabled)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    aml_dec_t *aml_dec = aml_out->aml_dec;
    float speed = 0.0f;
    struct mediasync_audio_policy m_audiopolicy;
    memset(&m_audiopolicy, 0, sizeof(m_audiopolicy));

    if (!(MEDIA_SYNC_ESMODE(aml_out)))
    {
        ALOGE("aml_out->hwsync:%p, aml_out->hw_sync_mode:%d, aml_out->avsync_type:%d error!", aml_out->hwsync, aml_out->hw_sync_mode, aml_out->avsync_type);
        return ESSYNC_AUDIO_OUTPUT;
    }

    if (aml_out->hwsync->es_mediasync.duration == 0 ||
        (duration > 0 && duration < aml_out->hwsync->es_mediasync.duration)) {

        ALOGI("set duration from: %d to:%d \n", aml_out->hwsync->es_mediasync.duration, duration);
        aml_out->hwsync->es_mediasync.duration = duration;
    }

    do {
        if (aml_out->hwsync && aml_out->hwsync->es_mediasync.mediasync) {
            if (aml_out->tsync_status == TSYNC_STATUS_STOP) {
                break;
            }
            mediasync_wrap_AudioProcess(aml_out->hwsync->es_mediasync.mediasync, aml_dec->out_frame_pts, aml_out->hwsync->es_mediasync.cur_outapts, MEDIASYNC_UNIT_PTS, &m_audiopolicy);
            if (adev->debug_flag > 0 || MEDIASYNC_AUDIO_NORMAL_OUTPUT != m_audiopolicy.audiopolicy)
                ALOGI("es m_audiopolicy=%d=%s, param1=%u, param2=%u, out_pts=0x%llx,cur=0x%llx \n",
                    m_audiopolicy.audiopolicy, mediasyncAudiopolicyType2Str(m_audiopolicy.audiopolicy),
                    m_audiopolicy.param1, m_audiopolicy.param2,
                    aml_dec->out_frame_pts, aml_out->hwsync->es_mediasync.cur_outapts);

            if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD) {
                if (m_audiopolicy.param1 == -1) {
                    usleep(15000);
                } else {
                    usleep(m_audiopolicy.param1);
                }
            }
        }

    } while (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD);

    if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {
        aml_out->hwsync->es_mediasync.cur_outapts = aml_dec->out_frame_pts;
        return ESSYNC_AUDIO_DROP;
    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_INSERT) {

        aml_hwmediasync_nonms12_process_insert(stream,  &m_audiopolicy);

    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_ADJUST_CLOCK) {

    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_RESAMPLE) {

        aml_hwsynces_process_resample(stream, &m_audiopolicy, speed_enabled);
    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_MUTE) {
        adev->underrun_mute_flag = true;
    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_NORMAL_OUTPUT) {
        adev->underrun_mute_flag = false;
    }

    return ESSYNC_AUDIO_OUTPUT;
}

