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
#define TRUEHD_OUTPUT_SAMPLE_RATE (48000)
#define MAT_OUTPUT_SAMPLE_RATE (192000)
#define SAMPLE_NUMS_IN_ONE_BLOCK (256)
#define DDP_FRAME_DURATION(sample_nums, sample_rate) ((sample_nums) / (sample_rate))

#define MAIN_INPUT_STREAM 1
#define ASSOC_INPUT_STREAM 0
#define SYSTEM_INPUT_STREAM 0
#define RESERVED_LENGTH 25

#define AUDIO_2MAIN_MIXER_NODE           0xFF000001UL
#define AUDIO_2MAIN_MIXER_NODE_PRIMARY   0xFF000002UL
#define AUDIO_2MAIN_MIXER_NODE_SECONDARY 0xFF000003UL
#define AUDIO_2MAIN_MIXER_NODE_SYSTEM    0xFF000004UL

#define AUDIO_SYSTEM_MIXER_NODE          0xFF000010UL
#define AUDIO_AD_MIXER_NODE              0xFF000020UL


#define MIXER_PRIMARY_INPUT   0
#define MIXER_SECONDARY_INPUT 1
#define MIXER_SYSTEM_INPUT    2

#define MS12_ALSA_LOW_LIMIT_FRAME        (16 *48)   // 16 ms
#define MS12_ALSA_DEFAULT_LIMIT_FRAME    1024       // 21.3 ms

/**
 *  @brief Supported channel modes, independent on module or codec.
 */
typedef enum {
    AML_DOLBY_ACMOD_INVALID    = -2,  /**< Invalid channel mode. */
    AML_DOLBY_ACMOD_RAW        = -1,  /**< Raw channel mode. (Object audio.) */
    AML_DOLBY_ACMOD_ONEPLUSONE =  0,  /**< Channel mode is dual mono. */
    AML_DOLBY_ACMOD_MONO       =  1,  /**< Channel mode is mono   (C). */
    AML_DOLBY_ACMOD_STEREO     =  2,  /**< Channel mode is stereo (L, R). */
    AML_DOLBY_ACMOD_3_0        =  3,  /**< Channel mode is 3/0    (L, R, C). */
    AML_DOLBY_ACMOD_2_1        =  4,  /**< Channel mode is 2/1    (L, R, Ls). */
    AML_DOLBY_ACMOD_3_1        =  5,  /**< Channel mode is 3/1    (L, R, C, Ls). */
    AML_DOLBY_ACMOD_2_2        =  6,  /**< Channel mode is 2/2    (L, R, Ls, Rs). */
    AML_DOLBY_ACMOD_3_2        =  7,  /**< Channel mode is 5.x    (L, R, C, Ls, Rs). */
    AML_DOLBY_ACMOD_3_4        = 21,  /**< Channel mode is 7.x    (L, R, C, Ls, Rs, Lrs, Rrs). */
    AML_DOLBY_ACMOD_3_2_2      = 28   /**< Channel mode is 5.x.2  (L, R, C, Ls, Rs, Ltm, Rtm). */
} AML_DOLBY_ACMOD;


typedef enum {
    MS12_COMPRESSOR_CLIPPING_PROTECTION = 0,
    MS12_COMPRESSOR_STANDARD_FILM       = 1,
    MS12_COMPRESSOR_LIGHT_FILM          = 2,
    MS12_COMPRESSOR_STANDARD_MUSIC      = 3,
    MS12_COMPRESSOR_LIGHT_MUSIC         = 4,
    MS12_COMPRESSOR_SPEECH              = 5
} MS12_COMPRESSOR_PROFILES;

typedef enum {
    AML_DOLBY_INPUT_MAIN,
    AML_DOLBY_INPUT_SYSTEM,
    AML_DOLBY_INPUT_APP,
    AML_DOLBY_INPUT_AD,
} AML_DOLBY_INPUT;

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
 *     abuffer: data buffer address
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_main_process(
    struct audio_stream_out *stream,
    struct audio_buffer *abuffer,
    size_t *use_size);

/*
 *@brief dolby ad data process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     abuffer: data buffer address
 */
int ms12_ad_process (
    struct audio_stream_out *stream,
    struct audio_buffer *abuffer);

/*
 *@brief set ms12 fade and pan parameter
 * input parameters
 *     struct dolby_ms12_desc *ms12: ms12 pointer
 *     int fade_byte
 *     int gain_byte_center
 *     int gain_byte_front
 *     int gain_byte_surround
 *     int pan_byte
 */
void set_ms12_fade_pan
    (struct dolby_ms12_desc *ms12
    , int fade_byte
    , int gain_byte_center
    , int gain_byte_front
    , int gain_byte_surround
    , int pan_byte
    );

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
    MC_LPCM  = 2,
};

typedef struct aml_ms12_dec_info {
    int output_sr ;   /** the decoded data samplerate*/
    int output_ch ;   /** the decoded data channels*/
    int output_bitwidth; /**the decoded sample bit width*/
    int data_type;
    enum MS12_PCM_TYPE pcm_type;
    unsigned int main_apts_high32b;
    unsigned int main_apts_low32b;
    unsigned int main1_apts_high32b;
    unsigned int main1_apts_low32b;
    int acmod;       /**dolby acmod, please refer to AML_DOLBY_ACMOD**/
    int lfeon;       /**whether it has lfe channel, lfe means low frequency effects**/
    int reserved[RESERVED_LENGTH];
} aml_ms12_dec_info_t;


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
 * input parameters
 *     set_non_continuous: disable ms12 continuous mode
 */
int get_dolby_ms12_cleanup(struct dolby_ms12_desc *ms12, bool set_non_continuous);

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
int ms12_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info);

/*
 *@brief dolby ms12 open the main decoder
 */
int dolby_ms12_main_open(struct audio_stream_out *stream);

/*
 *@brief dolby ms12 close the main decoder
 */
int dolby_ms12_main_close(struct audio_stream_out *stream);

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
int dolby_ms12_bypass_process(struct audio_stream_out *stream, const void *buffer, size_t bytes);

/*
 *@brief set dolby ms12 ad mixing enable
 */
void set_ms12_ad_mixing_enable(struct dolby_ms12_desc *ms12, int ad_mixing_enable);

/*
 *@brief set dolby ms12 mixing level
 */
void set_ms12_ad_mixing_level(struct dolby_ms12_desc *ms12, int mixing_level);
/*
 *@brief set dolby ms12 ad volume
 */
void set_ms12_ad_vol(struct dolby_ms12_desc *ms12, int ad_vol);

/*
 *@brief set dolby ms12 system mixing enable
 */
void set_dolby_ms12_runtime_system_mixing_enable(struct dolby_ms12_desc *ms12, int sys_mixing_enable);
bool is_ms12_passthrough(struct audio_stream_out *stream);

void set_ms12_atmos_lock(struct dolby_ms12_desc *ms12, bool is_atmos_lock_on);
void set_ms12_acmod2ch_lock(struct dolby_ms12_desc *ms12, bool is_lock_on);
bool is_ms12_continuous_mode(struct aml_audio_device *adev);
void set_ms12_main_volume(struct dolby_ms12_desc *ms12, float volume);

/*
 *@brief get the platform's capability of DDP-ATMOS.
 */
bool is_platform_supported_ddp_atmos(struct aml_audio_device *adev);

bool is_dolby_ms12_main_stream(struct audio_stream_out *stream);
bool is_support_ms12_reset(struct audio_stream_out *stream);
bool is_bypass_dolbyms12(struct audio_stream_out *stream);
bool is_dolbyms12_dap_enable(struct aml_stream_out *aml_out);
/*
 *@brief init the ms12 hwsync module to save pts info
 */
int dolby_ms12_hwsync_init(void);

/*
 *@brief release the ms12 hwsync module
 */
int dolby_ms12_hwsync_release(void);

/*
 *@brief check in the audio offset with pts
 */
int dolby_ms12_hwsync_checkin_pts(int offset, int apts);

/*
 *@brief dolby ms12 insert one frame, it is 32ms
 */
int dolby_ms12_output_insert_oneframe(struct audio_stream_out *stream);

/*
 *@brief get how many bytes consumed by main decoder
 */
uint64_t dolby_ms12_get_main_bytes_consumed(struct audio_stream_out *stream);
/*
 *@brief get how many pcm frames generated by main decoder
 */
uint64_t dolby_ms12_get_main_pcm_generated(struct audio_stream_out *stream);

/*
 *@brief get ms12 continuous reset status
 */
bool is_need_reset_ms12_continuous(struct audio_stream_out *stream);

bool is_ms12_output_compatible(struct audio_stream_out *stream, audio_format_t new_sink_format, audio_format_t new_optical_format);

/*
 *@brief dynamically set dolby ms12 drc parameters
 */
void dynamic_set_dolby_ms12_drc_parameters(struct dolby_ms12_desc *ms12, unsigned int mode_control);

/*
 *@brief get buffer latency about ms12
 */
unsigned int get_ms12_buffer_latency(struct aml_stream_out *out);

/*
 *@brief get audio postprocessing add dolby ms12 dap or not
 * return
 *   true if DAP is in MS12 pipline;
 *   false if DAP is not in MS12 pipline;
 */
bool is_audio_postprocessing_add_dolbyms12_dap(struct aml_audio_device *adev);

int bitstream_output(void *buffer, void *priv_data, size_t size);

int spdif_bitstream_output(void *buffer, void *priv_data, size_t size);

int dap_pcm_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info);

int stereo_pcm_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info);

int mat_bitstream_output(void *buffer, void *priv_data, size_t size);
/*
 *@brief set ms12 dap postgain
 */
void set_ms12_dap_postgain(struct dolby_ms12_desc *ms12, int postgain);
void set_ms12_ac4_presentation_group_index(struct dolby_ms12_desc *ms12, int index);
/*
 *@brief set ms12 main audio mute or non mute
 * input parameters
 * struct dolby_ms12_desc *ms12: ms12 pointer
 * bool b_mute: 1 mute , 0 unmute
 */
void set_ms12_main_audio_mute(struct dolby_ms12_desc *ms12, bool b_mute, unsigned int duration);

int dolby_ms12_encoder_reconfig(struct dolby_ms12_desc *ms12);
int ms12_scaletempo(void *priv_data, void *info);

void set_ms12_encoder_chmod_locking(struct dolby_ms12_desc *ms12, bool is_lock_on);
void set_ms12_set_compressor_profile(struct dolby_ms12_desc *ms12, int profile);
audio_format_t ms12_get_audio_hal_format(audio_format_t hal_format);
void set_ms12_alsa_limit_frame(struct dolby_ms12_desc *ms12, int limit_frame);
void set_ms12_scheduler_sleep(struct dolby_ms12_desc *ms12, bool enable_sleep);

#endif //end of _AUDIO_HW_MS12_H_
