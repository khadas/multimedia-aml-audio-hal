/*
 * Copyright (C) 2018 Amlogic Corporation.
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

#define LOG_TAG "aml_channel_delay"

#include "aml_audio_delay.h"





#define ONE_FRAME_MAX_MS   100
#define ALIGN(size, align) ((size + align - 1) & (~(align - 1)))

typedef struct delay_buf {
    unsigned char * buf_start;
    int buf_size;
    int wp;
    int rp;  // not used
    int delay_rp;
} delay_buf_t;

typedef struct delay_handle {
    int delay_time;
    int delay_max;
    delay_buf_t delay_buffer;
} delay_handle_t;


int aml_audiodelay_init(struct audio_hw_device *dev)
{
    int delay_max = 0;
    int buf_size = 0;
    unsigned char * buf_start = NULL;
    delay_handle_t * delay_handle = NULL;
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;

    delay_max =  adev->delay_max;

    delay_handle = calloc(1, sizeof(delay_handle_t));

    if (delay_handle == NULL) {
        return -1;
    }
    adev->delay_handle = delay_handle;

    /*calculate the max size for 192K 8ch 32 bit, ONE_FRAME_MAX_MS is used for one frame buffer*/
    buf_size = 192 * 8 * 4 * (delay_max + ONE_FRAME_MAX_MS);

    buf_start = calloc(1 , buf_size);
    if (buf_start == NULL) {
        ALOGE("malloc delay buffer failed for size: 0x%x\n", buf_size);
        return -1;
    }
    delay_handle->delay_buffer.buf_start = buf_start;
    delay_handle->delay_buffer.buf_size  = buf_size;
    delay_handle->delay_buffer.delay_rp  = 0;
    delay_handle->delay_buffer.wp        = 0;
    delay_handle->delay_buffer.rp        = 0;

    delay_handle->delay_max              = delay_max;
    delay_handle->delay_time             = 0;


    adev->delay_handle = (void *)delay_handle;


    return 0;
}
int aml_audiodelay_close(struct audio_hw_device *dev)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    delay_handle_t * delay_handle = NULL;

    delay_handle = adev->delay_handle;

    if (delay_handle) {
        if (delay_handle->delay_buffer.buf_start != NULL) {
            free(delay_handle->delay_buffer.buf_start);
        }

        free(delay_handle);

    }

    adev->delay_handle = NULL;

    return 0;
}


int aml_audiodelay_process(struct audio_hw_device *dev, void * in_data, int size, audio_format_t  format)
{
    delay_handle_t * delay_handle = NULL;
    int delay_time = 0;
    int delay_size = 0;
    int sample_rate = 0;
    int ch = 0;
    int bitwidth = 0;
    int one_sec_size = 0;
    int delay_buf_size = 0;
    int avail_buf_size = 0;
    int avail_data_size = 0;
    int left = 0;
    const unsigned char *tmp_buffer = in_data;
    unsigned char * buf_start = NULL;
    int wp = 0;
    int delay_rp = 0;
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;

    delay_handle = adev->delay_handle;
    if (delay_handle == NULL) {
        return -1;
    }
    if (format == AUDIO_FORMAT_E_AC3) {
        one_sec_size = 192 * 1000 * 4;// * abs(adev->delay_ms);
    } else if (format == AUDIO_FORMAT_AC3) {
        one_sec_size = 48 * 1000 * 4;// * abs(adev->delay_ms);
    } else {
        one_sec_size = 48 * 1000 * 32;// * abs(adev->delay_ms);
    }
    if (one_sec_size == 0) {
        return -1;
    }



    delay_time = delay_handle->delay_time;
    buf_start  = delay_handle->delay_buffer.buf_start;
    delay_buf_size = delay_handle->delay_buffer.buf_size;

    if (adev->delay_time > adev->delay_max) {
        adev->delay_time = adev->delay_max;
    }

    /*delay changed*/
    if (adev->delay_time < 0) {
        if (size > delay_size) {
            tmp_buffer = &tmp_buffer[delay_size];
            size = size - delay_size;
            adev->delay_time = 0;
        } else {
            if (format == AUDIO_FORMAT_E_AC3) {
                adev->delay_time = size / (192 * 4) - abs(adev->delay_time);
            } else if (format == AUDIO_FORMAT_AC3) {
                adev->delay_time = size / (48 * 4) - abs(adev->delay_time);
            } else {
                adev->delay_time = size / (48 * 32) - abs(adev->delay_time);
            }
            return -1;
        }
    } else if (delay_time != adev->delay_time) {
        memset(buf_start, 0, delay_buf_size);
        delay_time  = adev->delay_time;
        delay_handle->delay_time  = delay_time;
        delay_size = (one_sec_size * delay_time) / 1000;
        /*align to frame size*/
        delay_size = ALIGN(delay_size, 16);//ch * bitwidth

        delay_handle->delay_buffer.rp        = 0;
        delay_handle->delay_buffer.wp        = 0;
        delay_handle->delay_buffer.delay_rp  = (delay_buf_size - delay_size) % delay_buf_size;
    }
    /*we don't need to delay, just send the data back*/
    if (delay_time == 0) {
        return 0;
    }

    /*write data to delay buffer*/
    wp = delay_handle->delay_buffer.wp;
    avail_buf_size = delay_buf_size - wp;
    if (avail_buf_size >= size) {
        memcpy(buf_start + wp, in_data, size);
        wp += size;
    } else {
        left = size - avail_buf_size;
        memcpy(buf_start + wp, in_data, avail_buf_size);
        memcpy(buf_start, (char *)in_data + avail_buf_size, left);
        wp = left;
    }

    delay_handle->delay_buffer.wp = wp % delay_buf_size;

    /*get the delayed data*/
    delay_rp = delay_handle->delay_buffer.delay_rp;
    avail_data_size = delay_buf_size - delay_rp;
    if (avail_data_size >= size) {
        memcpy(in_data, buf_start + delay_rp, size);
        delay_rp += size;
    } else {
        left = size - avail_data_size;
        memcpy(in_data, buf_start + delay_rp, avail_data_size);
        memcpy((char *)in_data + avail_data_size, buf_start, left);
        delay_rp = left;
    }
    delay_handle->delay_buffer.delay_rp = delay_rp % delay_buf_size;
    return 0;
}

