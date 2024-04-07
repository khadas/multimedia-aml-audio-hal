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

#ifndef _AML_DTSX_DEC_API_H_
#define _AML_DTSX_DEC_API_H_

#include <hardware/audio.h>
#include "aml_ringbuffer.h"
#include "aml_audio_types_def.h"
#include "aml_dec_api.h"
#include "aml_audio_ease.h"

#define DCA_BITS(x) (1 << x)

struct dtsx_syncword_info {
    unsigned int syncword;
    int syncword_pos;
    int check_pos;
};

struct dtsx_frame_info {
    unsigned int syncword;
    int syncword_pos;
    int check_pos;
    bool is_little_endian;
    bool is_p2_codec;
    int iec61937_data_type;
    int size;
};

/** \brief Decoder output bus IDs */
typedef enum
{
    DTS_DSP2_OUTPUTBUS0 = 0,      /**< Multi-channel Bus 1 (for feeding the Virtual-X / Headphone-X processor) */
    DTS_DSP2_OUTPUTBUS1 = 1,      /**< Multi-channel Bus 2 (for feeding transcoder) */
    DTS_DSP2_OUTPUTBUS2 = 2,      /**< Stereo downmix Bus (downmix from Multi-channel Bus 1) */
    DTS_DSP2_OUTPUTBUS_MAX = 3    /**< This must be the last entry in this enum. Reserved for internal use only. */
} dtsx_output_bus_e;

///< Keep the members of dtsx_stream_info_t same as dtsxp1_bitstream_info_t in dtsx_aml_wrapper.h which is in dtx lib
typedef struct dtsx_stream_info_s
{
    unsigned int channel_num;         /**< Total number of channels of backwards compatible layout */
    unsigned int channel_mask;        /**< Channel mask of backwards compatible layout. If SpeakerActivitymask from Asset descriptor not available, m_nChannelMask will be zero*/
    unsigned int representation_type; /**< Representation type, #DTS_REPTYPE */
    unsigned int sample_rate;         /**< Maximum sampling rate of the bitstream */
    unsigned int stream_type;         /**< Indicates stream type through which we can distinguish legacy streams,DTS-X streams etc */
    unsigned int is_has_heights;      /* Indicates height channel(s) present or not */
    unsigned int is_drc_metadata_present; /* Value 1 Indicates presence of DRC metadata in stream */
    unsigned int is_dialognorm_metadata_present; /* Indicates Dialog Normalization parameters are present or not present in stream */
    unsigned int is_type1CC_flag_present; /* This 1-bit flag when TRUE it indicates that content is created according to Type1 certified process*/
    int reserved_padding[11];         /**< The reserved size of dtsxp1_bitstream_info_t is 80 bytes and needs to be filled to the corresponding size. */
} dtsx_stream_info_t;

///< Keep the members of dtsxParameterType_t same as dtsxParameterType_t in dtsx_aml_wrapper.h which is in dtx lib
typedef enum  {
    /* Get Type1 Certified Content processing mode value.
       0 - OFF mode. Type1-CC processing turned off even if Type1-CC flag is present in bitstream.
       1 - AUTO mode. The processing is controlled by the Type1-CC flag in the bitstream.
    */
    DTSX_TYPE1CC_MODE     = 0,
    /* Get the Type1-CC processing state.
       Type1-CC process state is get updated based on Type1-CC process mode (which is configured
       by user) and Type1-CC flag decoded from bit-stream.
    */
    DTSX_TYPE1CC_STATE    = 1,
    /* Get Type1-CC flag value from input bit-stream.
       0 - Type1-CC flag is not present in stream.
       1 - Type1-CC flag is present in stream.
    */
    DTSX_TYPE1CC_FLAG     = 2,
    /* Gets the status of parma(The latest upmixing and spatial remapping technology from DTS, a.k.a Neural:X).
       0 - Bypassed.
       1 - Running.
    */
    DTSX_PARMA_STATUS     = 3,    // Get the status (true: running; false: bypassed) of parma.

    /* Gets the bitstream info which stored in #dtsStreamInfo
    */
    DTSX_BITSTREAM_INFO   = 4,

    DTSX_PARAM_TYPE_MAX,
} dtsxParameterType_t;

typedef enum {
    DTSX_INITED = DCA_BITS(0),   // dca decoder init or not.
    DTSX_PROCESS_HALF_FRAME = DCA_BITS(1),
} DTSX_DECODER_STATUS;

typedef struct dtsx_dec_outbuf_s {
    bool          buf_is_full;
    char*         buf;
    unsigned int  buf_size;
    unsigned int  data_len;
} dtsx_dec_outbuf_t;

#define DTSX_PARAM_STRING_LEN     256
#define DTSX_PARAM_COUNT_MAX      64
#define DTSX_SINK_SUPPORTS_CA     0          /**< HDMI / SPDIF Sink Device Supports DTS. Must be supported by any sink */
#define DTSX_SINK_SUPPORTS_MA     1          /**< HDMI Sink Device Supports DTS-HD MA */
#define DTSX_SINK_SUPPORTS_P1     2          /**< HDMI Sink Device Supports DTS-X Profile 1 */
#define DTSX_SINK_SUPPORTS_P2     4          /**< HDMI Sink Device Supports DTS-X Profile 2 */

typedef struct dtsx_dec_s {
    ///< Control
    aml_dec_t aml_dec;
    void *p_dtsx_dec_inst;
    void *p_dtsx_pp_inst;

    ///< Information
    int status;
    int remain_size;
    int half_frame_remain_size;
    int half_frame_used_size;
    unsigned int outlen_pcm;
    unsigned int outlen_raw;
    int stream_type;    ///< enum audio_hal_format
    bool is_headphone_x;
    struct pcm_info pcm_out_info;
    struct dtsx_frame_info frame_info;   ///< for frame parsing

    ///< Parameter
    char *init_argv[DTSX_PARAM_COUNT_MAX];
    int init_argc;
    bool is_dtscd;
    bool is_iec61937;
    unsigned char *inbuf;
    unsigned int inbuf_size;
    unsigned char *dec_pcm_out_buf;
    unsigned int dec_pcm_out_size;
    unsigned char *a_dtsx_pp_output[3];
    unsigned int a_dtsx_pp_output_size[3];
    aml_dec_control_type_t digital_raw;
    int is_hdmi_output;
    ring_buffer_t input_ring_buf;
    dtsx_dec_outbuf_t dec_buf[2];//after core#1,we will queue data to do fade out, so we need two buffer(dtsx_dec_outbuf_t)
    bool need_fade_in;
    bool need_fade_out;

    pthread_t dts_postprocess_threadID;
    bool dts_postprocess_thread_exit;
    pthread_mutex_t stream_lock;
    void *stream;
    aml_audio_ease_t * ease_handle;
} dtsx_dec_t;

int dtsx_decoder_init_patch(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config);
int dtsx_decoder_release_patch(aml_dec_t *aml_dec);
int dtsx_decoder_process_patch(aml_dec_t *aml_dec, struct audio_buffer *abuffer);

///< internal api, for VirtualX.

/**
* @brief Get dca decoder output channel(internal use).
* @param None
* @return [success]: 0 decoder not init.
*         [success]: 1 ~ 8, output channel number
*            [fail]: -1 get output channel fail.
*/
int dtsx_get_out_ch_internal(void);
/**
* @brief Set dca decoder output channel(internal use).
* @param ch_num: The num of channels you want the decoder to output
*              0: Default setting, decoder configs output channel automatically.
*          1 ~ 8: The decoder outputs the specified number of channels
*                (At present, @ch_num only supports 2-ch and 6-ch).
* @return [success]: 0
*            [fail]: -1 set output channel fail.
*/
int dtsx_set_out_ch_internal(int ch_num);

int dtsx_set_postprocess_dynamic_parameter(char *cmd);

// drc
int dtsx_spk_drc_enable(bool enable);
int dtsx_transcoder_drc_enable(bool enable);
int dtsx_hp_drc_enable(bool enable);

int dtsx_spk_drc_profile(int profile);
int dtsx_transcoder_drc_profile(int profile);
int dtsx_hp_drc_profile(int profile);

int dtsx_spk_drc_default_curve(int default_curve);
int dtsx_transcoder_drc_default_curve(int default_curve);
int dtsx_hp_drc_default_curve(int default_curve);

int dtsx_spk_drc_cut_value(int cut_value);
int dtsx_transcoder_drc_cut_value(int cut_value);
int dtsx_hp_drc_cut_value(int cut_value);

int dtsx_spk_drc_boost_value(int boost_value);
int dtsx_transcoder_drc_boost_value(int boost_value);
int dtsx_hp_drc_boost_value(int boost_value);

// loudness
int dtsx_spk_loudness_enable(bool enable);
int dtsx_transcoder_loudness_enable(bool enable);
int dtsx_hp_loudness_enable(bool enable);

int dtsx_spk_loudness_target(int target);
int dtsx_transcoder_loudness_target(int target);
int dtsx_hp_loudness_target(int target);

void dtsx_set_sink_dev_type(int sink_dev_type);

extern aml_dec_func_t aml_dtsx_func;

#endif
