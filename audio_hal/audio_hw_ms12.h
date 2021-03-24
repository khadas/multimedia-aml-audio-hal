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

#ifndef _AUDIO_HW_MS12_H_
#define _AUDIO_HW_MS12_H_

#include <tinyalsa/asoundlib.h>
#include <system/audio.h>
#include <stdbool.h>
#include <aml_audio_ms12.h>

#include "audio_hw.h"

#define DDP_OUTPUT_SAMPLE_RATE (48000)
#define SAMPLE_NUMS_IN_ONE_BLOCK (256)
#define DDP_FRAME_DURATION(sample_nums, sample_rate) ((sample_nums) / (sample_rate))

#define MAIN_INPUT_STREAM 1
#define ASSOC_INPUT_STREAM 0
#define SYSTEM_INPUT_STREAM 0
#define RESERVED_LENGTH 31

/*
 *@brief get dolby ms12 prepared
 */
int get_the_dolby_ms12_prepared(
    struct aml_stream_out *aml_out
    , audio_format_t input_format
    , audio_channel_mask_t input_channel_mask
    , int input_sample_rate);

/*
 *@brief get the data of direct thread
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_main_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *used_size);


/*
 *@brief dolby ms12 system process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_system_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *used_size);

enum MS12_PCM_TYPE {
    NORMAL_LPCM = 0,
    DAP_LPCM = 1,
};

typedef struct aml_dec_info {
    int output_sr ;   /** the decoded data samplerate*/
    int output_ch ;   /** the decoded data channels*/
    int output_bitwidth; /**the decoded sample bit width*/
    int data_type;
    enum MS12_PCM_TYPE pcm_type;
    int reserved[RESERVED_LENGTH];
} aml_dec_info_t;


/*
 *@brief dolby ms12 app process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_app_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *used_size);

/*
 *@brief get dolby ms12 cleanup
 */
int get_dolby_ms12_cleanup(struct dolby_ms12_desc *ms12);

/*
 *@brief set dolby ms12 primary gain
 */
enum {
    GAIN_SHAPE_LINEAR   = 0,
    GAIN_SHAPE_CUBE_IN  = 1,
    GAIN_SHAPE_CUBE_OUT = 2,
    GAIN_SHAPE_MAX      = 3
};
int set_dolby_ms12_primary_input_db_gain(struct dolby_ms12_desc *ms12,
        int db_gain, int duration, int shape);

/*
 *@brief set system app mixing status
 * if normal pcm stream status is STANDBY, set mixing off(-xs 0)
 * if normal pcm stream status is active, set mixing on(-xs 1)
 */
int set_system_app_mixing_status(struct aml_stream_out *aml_out, int stream_status);

/*
 *@brief an callback for dolby ms12 output
 */
int ms12_output(void *buffer, void *priv_data, size_t size, aml_dec_info_t *ms12_info);

/*
 *@brief dolby ms12 flush the main related buffer
 */
int dolby_ms12_main_flush(struct audio_stream_out *stream);

/*
 *@brief dolby ms12 flush the app related buffer
 */
void dolby_ms12_app_flush();

/*
 *@brief dolby ms12 enable to debug by the property 'vendor.audio.dolbyms12.debug'
 */
void dolby_ms12_enable_debug();

/*
 *@brief check that dolby ms12 output ddp atmos(5.1.2)/ddp(5.1) is suitable or not.
 */
bool is_ms12_out_ddp_5_1_suitable(bool is_ddp_atmos);

/*
 *@brief bypass iec61937 audio data
 */
int dolby_ms12_bypass_process();

/*
 *@brief set dolby ms12 ad mixing enable
 */
void set_ms12_ad_mixing_enable(struct dolby_ms12_desc *ms12, int ad_mixing_enable);

/*
 *@brief set dolby ms12 mixing level
 */
void set_ms12_ad_mixing_level(struct dolby_ms12_desc *ms12, int mixing_level);

/*
 *@brief set dolby ms12 pause
 */
void set_dolby_ms12_runtime_pause(struct dolby_ms12_desc *ms12, int is_pause);

/*
 *@brief set dolby ms12 system mixing enable
 */
void set_dolby_ms12_runtime_system_mixing_enable(struct dolby_ms12_desc *ms12, int sys_mixing_enable);
bool is_bypass_ms12(struct audio_stream_out *stream);

void set_ms12_atmos_lock(struct dolby_ms12_desc *ms12, bool is_atmos_lock_on);

/*
 *@brief get the platform's capability of DDP-ATMOS.
 */
bool is_platform_supported_ddp_atmos(bool atmos_supported, enum OUT_PORT current_out_port);


#endif //end of _AUDIO_HW_MS12_H_
