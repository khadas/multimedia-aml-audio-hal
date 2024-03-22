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

#define LOG_TAG "aml_audio_dtsx_dec"
//#define LOG_NDEBUG 0

#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sound/asound.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include "audio_hw.h"
#include "aml_dtsx_dec_api.h"
#ifdef DTS_VX_V4_ENABLE
#include "Virtualx_v4.h"
#else
#include "Virtualx.h"
#endif
#include "aml_hw_mixer.h"
#include "aml_audio_nonms12_render.h"

#define DTSX_LIB_PATH       "/vendor/lib/libHwAudio_dtsx.so"
#define DTSX_LIB64_PATH     "/vendor/lib64/libHwAudio_dtsx.so"

#define MAX_DTS_FRAME_LENGTH 32768
#define READ_PERIOD_LENGTH 2048 * 4
#define DTS_TYPE_I     0xB
#define DTS_TYPE_II    0xC
#define DTS_TYPE_III   0xD
#define DTS_TYPE_IV    0x11

#define IEC61937_HEADER_LENGTH  8
#define IEC_DTS_HD_APPEND_LENGTH 12
#define IEC61937_PA_OFFSET  0
#define IEC61937_PA_SIZE    2
#define IEC61937_PB_OFFSET  2
#define IEC61937_PB_SIZE    2
#define IEC61937_PC_OFFSET  4
#define IEC61937_PC_SIZE    2
#define IEC61937_PD_OFFSET  6
#define IEC61937_PD_SIZE    2

#define AML_DCA_SW_CORE_16M                         0x7ffe8001
#define AML_DCA_SW_CORE_14M                         0x1fffe800
#define AML_DCA_SW_CORE_24M                         0xfe80007f
#define AML_DCA_SW_CORE_16                          0xfe7f0180
#define AML_DCA_SW_CORE_14                          0xff1f00e8
#define AML_DCA_SW_CORE_24                          0x80fe7f01
#define AML_DCA_SW_SUBSTREAM_M                      0x64582025
#define AML_DCA_SW_SUBSTREAM                        0x58642520
#define AML_DTSX_SYNCWORD_STANDALONE_METADATA       0x40411bf2
#define AML_DTSX_SYNCWORD_STANDALONE_METADATA_M     0x4140f21b
#define AML_DTSX_SYNCWORD_FTOC_NONSYNC              0x71C442E8
#define AML_DTSX_SYNCWORD_FTOC_NONSYNC_M            0xC471E842
#define AML_DTSX_PROP_DEBUG_FLAG                    "vendor.media.audio.dtsdebug"
#define AML_DTSX_PROP_DUMP_INPUT_RAW                "vendor.media.audio.dtsdump.input.raw"
#define AML_DTSX_PROP_DUMP_DECODE_PCM               "vendor.media.audio.dtsdump.decode.pcm"
#define AML_DTSX_PROP_DUMP_MIXED_PCM                "vendor.media.audio.dtsdump.mixed.pcm"
#define AML_DTSX_PROP_DUMP_OUTPUT_PCM               "vendor.media.audio.dtsdump.output.pcm"
#define AML_DTSX_PROP_DUMP_OUTPUT_RAW               "vendor.media.audio.dtsdump.output.raw"
#define AML_DTSX_DUMP_FILE_DIR                      "/tmp/audio_dump/"

#define AML_DTSX_FTOC_SYNC(__cur_syncword) \
(__cur_syncword == AML_DTSX_SYNCWORD_STANDALONE_METADATA || __cur_syncword == AML_DTSX_SYNCWORD_STANDALONE_METADATA_M\
|| __cur_syncword == AML_DTSX_SYNCWORD_FTOC_NONSYNC || __cur_syncword == AML_DTSX_SYNCWORD_FTOC_NONSYNC_M)

#define DCA_CHECK_NULL_STR(x)     (NULL == x ? #x : "")
#define DCA_CHECK_STATUS(status, state_code)       (status & state_code)
#define DCA_ADD_STATUS(status, state_code)       (status |= state_code)
#define DCA_CLEAR_STATUS(status, state_code)       (status &= ~state_code)

/** \brief The type of sink device that will receive transcoded bitstream */
#define DTS_SINK_SUPPORTS_CA                0          /**< HDMI / SPDIF Sink Device Supports DTS.  Must be supported by any sink */
#define DTS_SINK_SUPPORTS_MA                1          /**< HDMI Sink Device Supports DTS-HD MA */
#define DTS_SINK_SUPPORTS_P1                2          /**< HDMI Sink Device Supports DTS-X Profile 1 */
#define DTS_SINK_SUPPORTS_P2                4          /**< HDMI Sink Device Supports DTS-X Profile 2 */

/** \brief Sink device capability */
#define DTS_SINK_MA_CAPABLE(x)              (( x & DTS_SINK_SUPPORTS_MA ) || ( x & DTS_SINK_SUPPORTS_P1 ) ? 1 : 0)
#define DTS_SINK_P1_CAPABLE(x)              (( x & DTS_SINK_SUPPORTS_P1 ) ? 1 : 0)
#define DTS_SINK_P2_CAPABLE(x)              (( x & DTS_SINK_SUPPORTS_P2 ) ? 1 : 0)

#define DTSX_DRC_CUT_VALUE_MAX              100
#define DTSX_DRC_CUT_VALUE_MIN              0
#define DTSX_DRC_BOOST_VALUE_MAX            100
#define DTSX_DRC_BOOST_VALUE_MIN            0
#define DTSX_LOUDNESS_TARGET_VALUE_MAX      -10
#define DTSX_LOUDNESS_TARGET_VALUE_MIN      -60

#define DTSX_CORE2_OUTPUT_TYPE_DEFAULT      -1
#define DTSX_CORE2_OUTPUT_TYPE_ATUO         0
#define DTSX_CORE2_OUTPUT_TYPE_STEREO       2

enum
{
    EXITING_STATUS = -1001,
    NO_ENOUGH_DATA = -1002,
};

enum AML_DTSX_OUTPUT_TYPE
{
    DTSX_OUTPUT_SPK = 0,
    DTSX_OUTPUT_ARC = 1,
    DTSX_OUTPUT_HP  = 2,
    DTSX_OUTPUT_MAX = 3
} DTSX_OUTPUT_TYPE_E;

///< From dtshd_dec_api_common.h. It belongs to the pub header, so it won't change.
enum AML_DTSX_STRMTYPE_MASK
{
    DTSXDECSTRMTYPE_UNKNOWN              = 0x00000000,
    DTSXDECSTRMTYPE_DTS_LEGACY           = 0x00000001,
    DTSXDECSTRMTYPE_DTS_ES_MATRIX        = 0x00000002,
    DTSXDECSTRMTYPE_DTS_ES_DISCRETE      = 0x00000004,
    DTSXDECSTRMTYPE_DTS_9624             = 0x00000008,
    DTSXDECSTRMTYPE_DTS_ES_8CH_DISCRETE  = 0x00000010,
    DTSXDECSTRMTYPE_DTS_HIRES            = 0x00000020,
    DTSXDECSTRMTYPE_DTS_MA               = 0x00000040,
    DTSXDECSTRMTYPE_DTS_LBR              = 0x00000080,
    DTSXDECSTRMTYPE_DTS_LOSSLESS         = 0x00000100,
    DTSXDECSTRMTYPE_DTS_UHD              = 0x00000200,
    DTSXDECSTRMTYPE_DTS_UHD_MA           = 0x00000400,
    DTSXDECSTRMTYPE_DTS_UHD_LOSSLESS     = 0x00000800,
    DTSXDECSTRMTYPE_DTS_UHD_GAME         = 0x00001000,

    ///< amlogic add
    DTSXDECSTRMTYPE_DTS_HEADPHONE        = 0x10000000    ///< for headphoneX, fake streamtype
    ///< amlogic add end
} DTSXDECSTRMTYPE;

enum AML_DTSX_DRC_PROFILE
{
    DTSX2_DEC_DRC_PROFILE_LOW = 0,                            /**< DRC Low profile setting */
    DTSX2_DEC_DRC_PROFILE_MEDIUM = 1,                         /**< DRC Medium profile setting  */
    DTSX2_DEC_DRC_PROFILE_HIGH = 2,                           /**< DRC High profile setting */
    DTSX2_DEC_DRC_PROFILE_MAX = 3                             /**< This must always be the last entry on the list. Reserved for internal use only */
} DTSX_DRC_PROFILE_E;

enum AML_DTSX_DRC_DEFAULT_CURVE
{
    DTSX_DEC_DRC_DEFAULT_CURVE_NO_COMPRESSION = 0,            /**< No DRC Default Compression Curve */
    DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_FILM_STANDARD,          /**< DRC Default Compression Curve for Legacy Film Standard */
    DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_FILM_LIGHT,             /**< DRC Default Compression Curve for Legacy Film light */
    DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_MUSIC_STANDARD,         /**< DRC Default Compression Curve for Legacy Music Standard */
    DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_MUSIC_LIGHT,            /**< DRC Default Compression Curve for Legacy Music light */
    DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_SPEECH,                 /**< DRC Default Compression Curve for Legacy speech  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON1,                       /**< DRC Default Compression Curve for DTS:X Profile2 common1  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON2,                       /**< DRC Default Compression Curve for DTS:X Profile2 common2  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON3,                       /**< DRC Default Compression Curve for DTS:X Profile2 common3  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON4,                       /**< DRC Default Compression Curve for DTS:X Profile2 common4  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON5,                       /**< DRC Default Compression Curve for DTS:X Profile2 common5  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON6,                       /**< DRC Default Compression Curve for DTS:X Profile2 common6  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON7,                       /**< DRC Default Compression Curve for DTS:X Profile2 common7  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON8,                       /**< DRC Default Compression Curve for DTS:X Profile2 common8  */
    DTSX_DEC_DRC_DEFAULT_CURVE_COMMON9,                       /**< DRC Default Compression Curve for DTS:X Profile2 common9  */
    DTSX_DEC_DRC_DEFAULT_CURVE_CUSTOM,                        /**< DRC Default Compression Curve for DTS:X Profile2 custom   */
    DTSX_DEC_DRC_DEFAULT_CURVE_PREDEFINED_MAX = DTSX_DEC_DRC_DEFAULT_CURVE_CUSTOM, /**< DRC Default Compression Curve for Pre-defined curve  */
    DTSX_DEC_DRC_DEFAULT_CURVE_MAX
} DTSX_DRC_DEFAULT_CURVE_E;

typedef enum AML_DTSX_EASE_TYPE
{
    DTSX_EASE_IN = 0,
    DTSX_EASE_OUT = 1,
} DTSX_EASE_TYPE_E;

typedef struct dtsx_debug_s {
    bool debug_flag;
    FILE* fp_pcm;
    FILE* fp_input_raw;
    FILE* fp_dec_in_raw;
    FILE* fp_decode_pcm;
    FILE* fp_mixed_pcm;
    FILE* fp_output_raw;
} dtsx_debug_t;

typedef struct dtsx_config_params_s {
    int core1_dec_out;
    int core2_spkr_out;// core2 bus0 output speaker mask, this setting will only take effect when auto_config_out_for_vx is set to -1
    int auto_config_out_for_vx;//// Auto config output channel for Virtual X: -1:default without vx. 0:auto config output according to DecMode. 2:always downmix to stereo.
    int dec_sink_dev_type;
    int pp_sink_dev_type;
    bool passthrough;
    int limiter_type[DTSX_OUTPUT_MAX];
    bool drc_enable[DTSX_OUTPUT_MAX];
    int drc_profile[DTSX_OUTPUT_MAX];          // DTSX_DRC_PROFILE_E
    int drc_default_curve[DTSX_OUTPUT_MAX];    // DTSX_DRC_DEFAULT_CURVE_E
    int drc_cut_value[DTSX_OUTPUT_MAX];        // rang: 0 ~ 100
    int drc_boost_value[DTSX_OUTPUT_MAX];      // rang: 0 ~ 100
    bool loudness_enable[DTSX_OUTPUT_MAX];
    int loudness_target[DTSX_OUTPUT_MAX];      // rang: -60 ~ -10
    bool neuralx_up_mix;
    bool neox_down_mix;
} dtsx_config_params_t;

static dtsx_debug_t _dtsx_debug = {0};
static dtsx_dec_t *_dtsx_dec = NULL;
static dtsx_config_params_t _dtsx_config_params = {
    .core1_dec_out = 2123,
    .core2_spkr_out = 2,
    .auto_config_out_for_vx = DTSX_CORE2_OUTPUT_TYPE_DEFAULT,
    .dec_sink_dev_type = DTSX_SINK_SUPPORTS_CA,
    .pp_sink_dev_type = DTSX_SINK_SUPPORTS_CA,
    .passthrough = 0,
    /* \brief Limiter mode
    typedef enum
    {
        DTSX2_DEC_LIMITER_MODE_HYBRID_UNLINKED = 0,         < Enable hybrid limiting in un-linked mode (Highest MIPS)
        DTSX2_DEC_LIMITER_MODE_HYBRID_LINKED,               < Enable hybrid limiting in linked mode (Medium MIPS)
        DTSX2_DEC_LIMITER_MODE_HARD_CLIPPER,                < Enable hard-clipper limiting on all channels except LFE (Slightly higher than lowest MIPS)
        DTSX2_DEC_LIMITER_MODE_HARD_CLIPPER_ALL,            < Enable hard-clipper limiting on all channels including LFE (Lowest MIPS)
        DTSX2_DEC_LIMITER_MODE_MAX                         < This must always be the last entry on the list. Reserved for internal use onl
    } dtsx2DecLimiterMode;   */
    .limiter_type[DTSX_OUTPUT_SPK] = 0,
    .limiter_type[DTSX_OUTPUT_ARC] = 0,
    .limiter_type[DTSX_OUTPUT_HP] = 1,
    .drc_enable[DTSX_OUTPUT_SPK] = 1,
    .drc_enable[DTSX_OUTPUT_ARC] = 1,
    .drc_enable[DTSX_OUTPUT_HP] = 1,
    .drc_profile[DTSX_OUTPUT_SPK] = DTSX2_DEC_DRC_PROFILE_LOW,
    .drc_profile[DTSX_OUTPUT_ARC] = DTSX2_DEC_DRC_PROFILE_LOW,
    .drc_profile[DTSX_OUTPUT_HP] =  DTSX2_DEC_DRC_PROFILE_LOW,
    .drc_default_curve[DTSX_OUTPUT_SPK] = DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_FILM_LIGHT,
    .drc_default_curve[DTSX_OUTPUT_ARC] = DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_FILM_LIGHT,
    .drc_default_curve[DTSX_OUTPUT_HP] =  DTSX_DEC_DRC_DEFAULT_CURVE_LEGACY_FILM_LIGHT,
    .drc_cut_value[DTSX_OUTPUT_SPK] = 100,
    .drc_cut_value[DTSX_OUTPUT_ARC] = 100,
    .drc_cut_value[DTSX_OUTPUT_HP] = 100,
    .drc_boost_value[DTSX_OUTPUT_SPK] = 100,
    .drc_boost_value[DTSX_OUTPUT_ARC] = 100,
    .drc_boost_value[DTSX_OUTPUT_HP] = 100,
    .loudness_enable[DTSX_OUTPUT_SPK] = 1,
    .loudness_enable[DTSX_OUTPUT_ARC] = 0,
    .loudness_enable[DTSX_OUTPUT_HP] = 1,
    .loudness_target[DTSX_OUTPUT_SPK] = -20,
    .loudness_target[DTSX_OUTPUT_ARC] = -31,
    .loudness_target[DTSX_OUTPUT_HP] = -20,
    .neuralx_up_mix = 0,
    .neox_down_mix = 1
};

///static struct pcm_info pcm_out_info;
/*dts decoder lib function*/
static int (*_aml_dts_decoder_init)(void **ppDtsInstance, unsigned int init_argc, const char *init_argv[]);
static int (*_aml_dts_decoder_process)(void *pDtsInstance, const unsigned char *in_buf, unsigned int in_size, unsigned char **, unsigned int *);
static int (*_aml_dts_decoder_deinit)(void *pDtsInstance);
static int (*_aml_dts_decoder_get_output_info)(void *, int, int *, int *, int *);
static int (*_aml_dts_decoder_get_parameter)(void *pDtsInstance, dtsxParameterType_t nParamType, void *pValue);

static int (*_aml_dts_postprocess_init)(void **ppDtsPPInstance, unsigned int init_argc, const char *init_argv[]);
static int (*_aml_dts_postprocess_deinit)(void *pDtsPPInstance);
static int (*_aml_dts_postprocess_proc)(void *ppDtsPPInstance, const unsigned char *in_buf, unsigned int in_size, unsigned char **, unsigned int *);
static int (*_aml_dts_metadata_update)(void *pDtsInstance, void *pDtsPPInstance);
static int (*_aml_dts_postprocess_get_out_info)(void *, int , int *, int *, int *);
static int (*_aml_dts_postprocess_dynamic_parameter_set)(void *pDtsPPInstance, unsigned int init_argc, const char *init_argv[]);

static void *_DtsxDecoderLibHandler = NULL;
static int _dtsx_syncword_scan(unsigned char *read_pointer, unsigned int *pTemp0);
static int _dtsx_frame_scan(dtsx_dec_t *dts_dec);
static int _dtsx_pcm_output(dtsx_dec_t *dtsx_dec);
static int _dts_raw_output(dtsx_dec_t *dtsx_dec);
static int _dts_stream_type_mapping(unsigned int stream_type);
static int _dtsx_dualcore_init(dtsx_dec_t *p_dtsx_dec);
static int _get_dtsx_function_symbol(void);
static void _unload_dtsx_function_symbol();

static void _dtsx_ease_conifg(aml_dec_t *aml_dec, DTSX_EASE_TYPE_E ease_type, int duration, int ch, int sr) {
    /*start ease in the audio*/
    ease_setting_t ease_setting;
    dtsx_dec_t *dtsx_dec = (dtsx_dec_t *)aml_dec;
    ALOGI("[%s:%d] do ease %s, duration(%d), ch(%d), sr(%d)", __func__, __LINE__, ease_type?"OUT":"IN", duration, ch, sr);

    dtsx_dec->ease_handle->data_format.format = AUDIO_FORMAT_PCM_16_BIT;
    dtsx_dec->ease_handle->data_format.ch = ch;
    dtsx_dec->ease_handle->data_format.sr = sr;
    dtsx_dec->ease_handle->ease_type = EaseInCubic;
    ease_setting.duration = duration;
    if (ease_type == DTSX_EASE_IN) {
        ease_setting.start_volume = 0.0;
        ease_setting.target_volume = 1.0;
    } else if (ease_type == DTSX_EASE_OUT) {
        ease_setting.start_volume = 1.0;
        ease_setting.target_volume = 0.0;
    }
    aml_audio_ease_config(dtsx_dec->ease_handle, &ease_setting);

    return 0;
}

static void endian16_convert(void *buf, int size)
{
    int i;
    unsigned short *p = (unsigned short *)buf;
    for (i = 0; i < size / 2; i++, p++) {
      *p = ((*p & 0xff) << 8) | ((*p) >> 8);
    }
}

static int _dtsx_iec61937_check_type(unsigned char *buf, int size)
{
    int i = -1;
    uint32_t *tmp_pc;
    uint32_t pc = 0;

    if (size < 8) {
        return i;
    }
    for (i = 0; i < (size - 4); i++) {
        if ((buf[i + 0] == 0x72 && buf[i + 1] == 0xf8 && buf[i + 2] == 0x1f && buf[i + 3] == 0x4e)
            || (buf[i + 0] == 0xf8 && buf[i + 1] == 0x72 && buf[i + 2] == 0x4e && buf[i + 3] == 0x1f)) {
            tmp_pc = (uint32_t*)(buf + i + 4);
            pc = *tmp_pc;
            return (pc & 0x1f);
        }
    }
    return -1;
}

static int _dtsx_syncword_scan(unsigned char *read_pointer, unsigned int *pTemp0)
{
    unsigned int ui32Temp0 = 0;
    unsigned int ui32Temp1 = 0;

    ui32Temp0  = read_pointer[0];
    ui32Temp0 <<= 8;
    ui32Temp0 |= read_pointer[1];
    ui32Temp0 <<= 8;
    ui32Temp0 |= read_pointer[2];
    ui32Temp0 <<= 8;
    ui32Temp0 |= read_pointer[3];

    ui32Temp1  = read_pointer[4];
    ui32Temp1 <<= 8;
    ui32Temp1 |= read_pointer[5];
    ui32Temp1 <<= 8;
    ui32Temp1 |= read_pointer[6];
    ui32Temp1 <<= 8;
    ui32Temp1 |= read_pointer[7];

    /* 16-bit bit core stream*/
    if (ui32Temp0 == AML_DCA_SW_CORE_16  || ui32Temp0 == AML_DCA_SW_CORE_14 ||
        ui32Temp0 == AML_DCA_SW_CORE_16M || ui32Temp0 == AML_DCA_SW_CORE_14M ||
        ui32Temp0 == AML_DCA_SW_SUBSTREAM|| ui32Temp0 ==AML_DCA_SW_SUBSTREAM_M ||
        ui32Temp0 == AML_DTSX_SYNCWORD_STANDALONE_METADATA || ui32Temp0 == AML_DTSX_SYNCWORD_STANDALONE_METADATA_M ||
        ui32Temp0 == AML_DTSX_SYNCWORD_FTOC_NONSYNC || ui32Temp0 == AML_DTSX_SYNCWORD_FTOC_NONSYNC_M)
    {
        *pTemp0 = ui32Temp0;
        return 1;
    }

    if ((ui32Temp0 & 0xffffff00) == (AML_DCA_SW_CORE_24 & 0xffffff00) &&
       ((ui32Temp1 >> 16) & 0xFF)== (AML_DCA_SW_CORE_24 & 0xFF)) {
        *pTemp0 = ui32Temp0;
        return 1;
    }

    return 0;
}

static int _dtsx_frame_scan(dtsx_dec_t *dts_dec)
{
    int frame_size = 0;
    int unuse_size = 0;
    unsigned char *read_pointer = NULL;
    struct ring_buffer* input_rbuffer = &dts_dec->input_ring_buf;
    struct dtsx_frame_info* frame_info = &dts_dec->frame_info;

    read_pointer = input_rbuffer->start_addr + frame_info->check_pos;
    if (read_pointer <= input_rbuffer->wr) {
        unuse_size = input_rbuffer->wr - read_pointer;
    } else {
        unuse_size = input_rbuffer->size + input_rbuffer->wr - read_pointer;
    }

    ALOGV("remain :%d, unuse_size:%d, is_dts:%d, is_iec61937:%d",
        dts_dec->remain_size, unuse_size, dts_dec->is_dtscd, dts_dec->is_iec61937);
    if (dts_dec->is_dtscd) {
        frame_size = dts_dec->remain_size;
    } else if (dts_dec->is_iec61937) {
        int drop_size = 0;
        if (!frame_info->syncword || (frame_info->size <= 0)) {
            bool syncword_flag = false;

            //DTS_SYNCWORD_IEC61937 : 0xF8724E1F
            while (!syncword_flag && (unuse_size > IEC61937_HEADER_LENGTH)) {
                if (read_pointer[0] == 0x72 && read_pointer[1] == 0xf8
                    && read_pointer[2] == 0x1f && read_pointer[3] == 0x4e) {
                    syncword_flag = true;
                    frame_info->iec61937_data_type = read_pointer[4] & 0x1f;
                } else if (read_pointer[0] == 0xf8 && read_pointer[1] == 0x72
                           && read_pointer[2] == 0x4e && read_pointer[3] == 0x1f) {
                    syncword_flag = true;
                    frame_info->is_little_endian = true;
                    frame_info->iec61937_data_type = read_pointer[5] & 0x1f;
                }

                if (syncword_flag) {
                    if ((frame_info->iec61937_data_type == DTS_TYPE_I)
                        || (frame_info->iec61937_data_type == DTS_TYPE_II)
                        || (frame_info->iec61937_data_type == DTS_TYPE_III)
                        || (frame_info->iec61937_data_type == DTS_TYPE_IV)) {
                        frame_info->syncword = 0xF8724E1F;
                    } else {
                        syncword_flag = false;
                    }
                }

                if (!syncword_flag) {
                    read_pointer++;
                    unuse_size--;
                    if (read_pointer > (input_rbuffer->start_addr + input_rbuffer->size)) {
                        read_pointer = input_rbuffer->start_addr;
                    }
                }
            }

            //ALOGD("DTS Sync=%d, little endian=%d, dts type=0x%x\n", syncword_flag, frame_info->is_little_endian, frame_info->iec61937_data_type);
            if (syncword_flag) {
                // point to pd
                read_pointer = read_pointer + IEC61937_PD_OFFSET;
                //ALOGD("read_pointer[0]:0x%x read_pointer[1]:0x%x",read_pointer[0],read_pointer[1]);
                if (frame_info->is_little_endian) {
                    frame_size = (read_pointer[1] | read_pointer[0] << 8);
                } else {
                    frame_size = (read_pointer[0] | read_pointer[1] << 8);
                }

                if (frame_info->iec61937_data_type == DTS_TYPE_I ||
                    frame_info->iec61937_data_type == DTS_TYPE_II ||
                    frame_info->iec61937_data_type == DTS_TYPE_III) {
                    // these DTS type use bits length for PD
                    frame_size = frame_size >> 3;
                    // point to the address after pd
                    read_pointer = read_pointer + IEC61937_PD_SIZE;
                } else if ((frame_info->iec61937_data_type == DTS_TYPE_IV)
                            && (unuse_size > IEC_DTS_HD_APPEND_LENGTH + IEC61937_HEADER_LENGTH)) {
                    /*
                    Structure of the IEC Data Burst for DTS Type 4
                     ———————————————————————————————————————————————————————————————————————————————————————————————————————————————————
                    | Filed name  |Pa |Pb |Pc |Pd |Chunk Index 1|Chunk Index 2|Reserved|Status Register|Chunk ID|Chunk Size| audio data |
                    ————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
                    |Size in bytes| 2 | 2 | 2 | 2 |     1       |      1      |    5   |     1         |    2   |     2    |  Chunk Size|
                     ———————————————————————————————————————————————————————————————————————————————————————————————————————————————————

                     Chunk Index and Chunk ID Values
                     ———————————————————————————————————————————————————————
                    |Chunk Index Value|Chunk ID|    DESCRIPTION             |
                     ———————————————————————————————————————————————————————
                    |       0         | 0x0000 |   No chunk index entry     |
                     ———————————————————————————————————————————————————————
                    |       1         | 0xFEFE |DTS Presentation audio chunk|
                     ———————————————————————————————————————————————————————
                    |       5         | 0xFAFA |   DTS Command chunk        |
                     ———————————————————————————————————————————————————————
                    |all other values |        |       Reserved             |
                     ———————————————————————————————————————————————————————

                     PS:Chunk Size is the audio data size
                    */
                    // point to the address after pd
                    read_pointer = read_pointer + IEC61937_PD_SIZE;
                    if (read_pointer[8] == 0xfe && read_pointer[9] == 0xfe && !frame_info->is_little_endian) {
                        frame_size = (read_pointer[10] | read_pointer[11] << 8);
                    } else if (read_pointer[8] == 0xfe && read_pointer[9] == 0xfe && frame_info->is_little_endian) {
                        frame_size = (read_pointer[11] | read_pointer[10] << 8);
                    } else {
                        ALOGE("DTS HD error data\n");
                        frame_size = 0;
                    }
                    //ALOGD("size data=0x%x 0x%x\n",read_pointer[10],read_pointer[11]);
                    // point to the address after 12 bytes header
                    read_pointer = read_pointer + IEC_DTS_HD_APPEND_LENGTH;
                }
            }

            if (read_pointer >= input_rbuffer->rd) {
                drop_size = read_pointer - input_rbuffer->rd;
            } else {
                drop_size = input_rbuffer->size + read_pointer - input_rbuffer->rd;
            }

            if (drop_size > 0) {
                ring_buffer_seek(input_rbuffer, drop_size);
                dts_dec->remain_size -= drop_size;
            }
        } else {
            frame_size = frame_info->size;
        }

        // If framesize is a odd number, (such as 2013(Pd 0x3EE8 >>3 )) and ASPF(a.k.a Audio Sync Word Insertion Flag) is 1.
        // The last byte will be used as a parameter for DSYNC verification and needs to be given to the decoder together.
        // Due to the endian problem, one more bytes need to be passed in, and the one extra byte, the decoder will discard it.
        frame_info->is_p2_codec = false;
        unsigned int syncword = 0;
        if (_dtsx_syncword_scan(read_pointer, &syncword)) {
            if (AML_DTSX_FTOC_SYNC(syncword)) {
                frame_info->is_p2_codec = true;
            }
        }
        if ((frame_size % 2) && (frame_info->is_p2_codec == false)) {
            frame_size += 1;
        }

        //ALOGD("remain:%d, frame:%d, read:%p, rd:%p, drop:%d"
        //    , dts_dec->remain_size, frame_size, read_pointer, input_rbuffer->rd, drop_size);

        frame_info->check_pos = read_pointer - input_rbuffer->start_addr;
        frame_info->size = frame_size;
        if ((dts_dec->remain_size >= frame_size) && (frame_size > 0)) {
            frame_info->syncword = 0;
            frame_info->check_pos += frame_size;
            if (frame_info->check_pos >= input_rbuffer->size) {
                frame_info->check_pos -= input_rbuffer->size;
            }
        } else {
            frame_size = 0;
        }
    } else {
        unsigned int syncword = 0;
        unsigned int check_size = 0;
        int tmp_syncword_pos = -1;
        int first_sync_word_pos = 0;
        while ((frame_size <= 0) && (unuse_size > IEC61937_HEADER_LENGTH)) {
            if (_dtsx_syncword_scan(read_pointer, &syncword)) {
                tmp_syncword_pos = read_pointer - input_rbuffer->start_addr;
                if (!frame_info->syncword) {
                    frame_info->syncword_pos = tmp_syncword_pos;
                    frame_info->syncword = syncword;
                    first_sync_word_pos = tmp_syncword_pos;
                } else {
                    if ((syncword == frame_info->syncword) ||
                        (AML_DTSX_FTOC_SYNC(syncword) && AML_DTSX_FTOC_SYNC(frame_info->syncword))) {
                        if (tmp_syncword_pos >= frame_info->syncword_pos) {
                            frame_size = tmp_syncword_pos - frame_info->syncword_pos;
                        } else {
                            frame_size = input_rbuffer->size + tmp_syncword_pos - frame_info->syncword_pos;
                        }
                        frame_info->syncword_pos = tmp_syncword_pos;
                        frame_info->syncword = syncword;
                    }
                }

                frame_info->is_p2_codec = false;
                if (AML_DTSX_FTOC_SYNC(syncword)) {
                    frame_info->is_p2_codec = true;
                }
                ALOGV("syncword :0x%x, syncword_pos:%d", frame_info->syncword, frame_info->syncword_pos);
            }

            unuse_size--;
            read_pointer++;
            check_size++;
            if (read_pointer > (input_rbuffer->start_addr + input_rbuffer->size)) {
                read_pointer = input_rbuffer->start_addr;
            }
        }

        frame_info->check_pos = read_pointer - input_rbuffer->start_addr;

        ALOGV("check_pos:%d, syncword_pos:%d, read_pointer:%p, check_size:%d",
            frame_info->check_pos, frame_info->syncword_pos, read_pointer, check_size);

        // no valid frame was found beyond size of MAX_DCA_FRAME_LENGTH, maybe it is dirty data, so drop it
        if ((frame_size <= 0) && (check_size >= (MAX_DTS_FRAME_LENGTH - IEC61937_HEADER_LENGTH))) {
            ring_buffer_seek(input_rbuffer, check_size);
            dts_dec->remain_size -= check_size;
            frame_info->syncword_pos = 0;
            frame_info->syncword = 0;
            frame_size = 0;
            ALOGI("no valid frame, drop %d bytes.", check_size);
        } else if ((frame_size > 0) && (first_sync_word_pos > 0)) {
            // drop the dirty data in the beginning.
            ring_buffer_seek(input_rbuffer, first_sync_word_pos);
            dts_dec->remain_size -= first_sync_word_pos;
            ALOGI("drop %d bytes before found the syncword.", first_sync_word_pos);
        } else if ((frame_size <= 0) && (check_size < (MAX_DTS_FRAME_LENGTH - IEC61937_HEADER_LENGTH))) {
            frame_size = 0;
        }
    }

    if (_dtsx_debug.debug_flag) {
        ALOGD("%s remain size:%d, frame size:%d, is dtscd:%d, is iec61937:%d",
            __func__, dts_dec->remain_size, frame_size, dts_dec->is_dtscd, dts_dec->is_iec61937);
    }

    return frame_size;
}

static int _dtsx_pcm_output(dtsx_dec_t *dtsx_dec)
{
    aml_dec_t *aml_dec = (aml_dec_t *)dtsx_dec;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    int nSampleRate =0;
    int nChannel =0;
    int nBitWidth =0;

    int rc = _aml_dts_postprocess_get_out_info(dtsx_dec->p_dtsx_pp_inst, DTS_DSP2_OUTPUTBUS0, &nSampleRate, &nChannel, &nBitWidth);
    if (rc != 0) {
        ALOGE("[%s:%d] _aml_dts_postprocess_get_out_info fail", __func__, __LINE__);
    } else {
        if (_dtsx_debug.debug_flag) {
            ALOGD("[%s:%d] Pcm [BUS-0] output nSampleRate(%d), nChannel(%d), nBitWidth(%d)",
                __func__, __LINE__, nSampleRate, nChannel, nBitWidth);
        }
    }

    /* VX(VirtualX) uses 2CH as input by default.
     * When the output changes from stereo to multi-ch,
     * VX needs at least one frame of decoded information to configure the number of channel correctly.
     * So mute to the first frame to avoid abnormal sound caused by wrong configuration.
     */
    if (dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0] > dec_pcm_data->buf_size) {
        ALOGI("[%s:%d] realloc bus[0] buffer from (%d) to (%u)", __func__, __LINE__,
            dec_pcm_data->buf_size, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0]);
        dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0]);
        if (dec_pcm_data->buf == NULL) {
            ALOGE("[%s:%d] realloc for bus[0] buffer(%u) failed", __func__, __LINE__,
                dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0]);
            return AML_DEC_RETURN_TYPE_FAIL;
        }
        dec_pcm_data->buf_size = dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0];
    }
    memcpy(dec_pcm_data->buf, dtsx_dec->a_dtsx_pp_output[DTS_DSP2_OUTPUTBUS0], dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0]);

    if (_dtsx_debug.fp_pcm) {
        fwrite(dec_pcm_data->buf, 1, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0], _dtsx_debug.fp_pcm);
    }

    dec_pcm_data->data_format = AUDIO_FORMAT_PCM_16_BIT;
    if (nChannel > 0) {
        dec_pcm_data->data_ch = dec_pcm_data->data_pcm_ch = nChannel;
    } else {
        dec_pcm_data->data_ch = dec_pcm_data->data_pcm_ch = 2;
    }

    if (nSampleRate > 0) {
        dec_pcm_data->data_sr = nSampleRate;
    } else {
        dec_pcm_data->data_sr = 48000;
    }
    dec_pcm_data->data_len = dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0];

    return AML_DEC_RETURN_TYPE_OK;
}

static int _dtsx_raw_output(dtsx_dec_t *dtsx_dec)
{
    aml_dec_t *aml_dec = (aml_dec_t *)dtsx_dec;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    unsigned int syncword = 0;
    int nSampleRate =0;
    int nChannel =0;
    int nBitWidth =0;
    int dts_iec61937_type = -1;
    int rc = _aml_dts_postprocess_get_out_info(dtsx_dec->p_dtsx_pp_inst, DTS_DSP2_OUTPUTBUS1, &nSampleRate, &nChannel, &nBitWidth);
    if (rc != 0) {
        ALOGE("[%s:%d] _aml_dts_postprocess_get_out_info fail", __func__, __LINE__);
    } else {
        if (_dtsx_debug.debug_flag) {
            ALOGD("[%s:%d] transcode/passthrough [BUS-1] output nSampleRate(%d), nChannel(%d), nBitWidth(%d)",
                __func__, __LINE__, nSampleRate, nChannel, nBitWidth);
        }
    }

    if (dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1] > dec_raw_data->buf_size) {
        ALOGI("[%s:%d] realloc bus[1] buffer from (%d) to (%u)", __func__, __LINE__,
            dec_raw_data->buf_size, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1]);
        dec_raw_data->buf = aml_audio_realloc(dec_raw_data->buf, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1]);
        if (dec_raw_data->buf == NULL) {
            ALOGE("[%s:%d] realloc for bus[1] buffer(%u) failed", __func__, __LINE__, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1]);
            return AML_DEC_RETURN_TYPE_FAIL;
        }
        dec_raw_data->buf_size = dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1];
    }
    memcpy(dec_raw_data->buf, dtsx_dec->a_dtsx_pp_output[DTS_DSP2_OUTPUTBUS1], dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1]);
    if (_dtsx_debug.fp_output_raw) {
        fwrite(dec_raw_data->buf, 1, dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1], _dtsx_debug.fp_output_raw);
    }

    dec_raw_data->data_format = AUDIO_FORMAT_IEC61937;

    /*we check whether the data is iec61937 or dts raw*/
    dec_raw_data->is_dtscd = _dtsx_syncword_scan(dec_raw_data->buf, &syncword);
    dec_raw_data->data_ch = nChannel;
    dec_raw_data->data_sr = nSampleRate;
    dec_raw_data->data_len = dtsx_dec->a_dtsx_pp_output_size[1];

    // TODO: Check the @nSampleRate and @nChannel from postprocess is correct or not.

    if (dec_raw_data->is_dtscd) {
        dec_raw_data->sub_format = AUDIO_FORMAT_DTS;
        dts_iec61937_type = DTS_TYPE_I;
    } else {
        /* spdif type，set output format 48K 2ch
         * hdmi mode，type4，1xI2S，set output format 192K 2ch
         * hdmi mode，type4，4xI2S，set output format 192K 8ch
         */
        if (nChannel == 8) {// hbr
            dec_raw_data->sub_format = AUDIO_FORMAT_DTS_HD;
            dts_iec61937_type = DTS_TYPE_IV;
        } else {
            dts_iec61937_type = _dtsx_iec61937_check_type(dec_raw_data->buf, dec_raw_data->buf_size);
            if (dts_iec61937_type == DTS_TYPE_IV) {
                dec_raw_data->sub_format = AUDIO_FORMAT_DTS_HD;
            } else {
                dec_raw_data->sub_format = AUDIO_FORMAT_DTS;
            }
        }
    }
    dec_raw_data->data_len = dtsx_dec->a_dtsx_pp_output_size[1];

    if (_dtsx_debug.debug_flag) {
        ALOGD("raw_output: length:%d sr:%d ch:%d dtscd:%d sub_format:0x%x iec_type:0x%x",
            dec_raw_data->data_len,
            dec_raw_data->data_sr,
            dec_raw_data->data_ch,
            dec_raw_data->is_dtscd,
            dec_raw_data->sub_format,
            dts_iec61937_type);
    }
    return 0;
}


static void _unload_dtsx_function_symbol()
{
    _aml_dts_decoder_init = NULL;
    _aml_dts_decoder_process = NULL;
    _aml_dts_decoder_deinit = NULL;
    _aml_dts_decoder_get_output_info = NULL;
    _aml_dts_decoder_get_parameter = NULL;
    _aml_dts_postprocess_init = NULL;
    _aml_dts_postprocess_deinit = NULL;
    _aml_dts_postprocess_proc = NULL;
    _aml_dts_metadata_update = NULL;
    _aml_dts_postprocess_get_out_info = NULL;
    _aml_dts_postprocess_dynamic_parameter_set = NULL;
    if (_DtsxDecoderLibHandler != NULL) {
        dlclose(_DtsxDecoderLibHandler);
        _DtsxDecoderLibHandler = NULL;
    }
}

static int _get_dtsx_function_symbol(void)
{
    ALOGI("[%s:%d] in", __func__, __LINE__);
    _DtsxDecoderLibHandler = dlopen(DTSX_LIB_PATH, RTLD_NOW);
    //open 32bit so failed, here try to open the 64bit dtsx so.
    if (_DtsxDecoderLibHandler == NULL) {
        _DtsxDecoderLibHandler = dlopen(DTSX_LIB64_PATH, RTLD_NOW);
        ALOGI("%s, 64bit lib:%s, _DtsxDecoderLibHandler:%p\n", __FUNCTION__, DTSX_LIB64_PATH, _DtsxDecoderLibHandler);
    }
    if (!_DtsxDecoderLibHandler) {
        ALOGE("%s, failed to open (libHwAudio_dtsx.so), %s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[_DtsxDecoderLibHandler]", __FUNCTION__, __LINE__);
    }

    _aml_dts_decoder_init = (int (*)(void **, unsigned int, const char **))dlsym(_DtsxDecoderLibHandler, "dtsx_decoder_init");
    if (_aml_dts_decoder_init == NULL) {
        ALOGE("%s,can't find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_init:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_decoder_process = (int (*)(void *, const unsigned char *, unsigned int, unsigned char **,unsigned int *))dlsym(_DtsxDecoderLibHandler, "dtsx_decoder_process");
    if (_aml_dts_decoder_process == NULL) {
        ALOGE("%s,can't find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_process:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_decoder_deinit = (int (*)(void *))dlsym(_DtsxDecoderLibHandler, "dtsx_decoder_deinit");
    if (_aml_dts_decoder_deinit == NULL) {
        ALOGE("%s,can't find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_deinit:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_decoder_get_output_info = (int (*)(void *, int, int *, int *, int *))dlsym(_DtsxDecoderLibHandler, "dtsx_decoder_get_out_info");
    if (_aml_dts_decoder_get_output_info == NULL) {
        ALOGE("%s,can not find decoder getinfo function,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_getinfo:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_decoder_get_parameter = (int (*)(void *, dtsxParameterType_t , void *))dlsym(_DtsxDecoderLibHandler, "dtsx_decoder_get_parameter");
    if (_aml_dts_decoder_get_parameter == NULL)  {
        ALOGE("%s,can not find decoder get parameter function,%s\n", __FUNCTION__, dlerror());
        //return -1;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_getparameter:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_postprocess_init = (int (*)(void **, unsigned int, const char **))dlsym(_DtsxDecoderLibHandler, "dtsx_postprocess_init");
    if (_aml_dts_postprocess_init == NULL) {
        ALOGE("%s,can not find decoder getinfo function,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_postprocess_init:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_postprocess_deinit = (int (*)(void *))dlsym(_DtsxDecoderLibHandler, "dtsx_postprocess_deinit");
    if (_aml_dts_postprocess_deinit == NULL) {
        ALOGE("%s,can not find decoder getinfo function,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_postprocess_deinit:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_postprocess_proc = (int (*)(void *, const unsigned char *, unsigned int, unsigned char **,unsigned int *))dlsym(_DtsxDecoderLibHandler, "dtsx_postprocess_proc");
    if (_aml_dts_postprocess_proc == NULL) {
        ALOGE("%s,can not find decoder getinfo function,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_postprocess_proc:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_metadata_update = (int (*)(void *, void *))dlsym(_DtsxDecoderLibHandler, "dtsx_metadata_update");
    if (_aml_dts_metadata_update == NULL) {
        ALOGE("%s,can not find decoder getinfo function,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dts_metadata_update:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_postprocess_get_out_info = (int (*)(void *, int, int *, int *, int *))dlsym(_DtsxDecoderLibHandler, "dtsx_postprocess_get_out_info");
    if (_aml_dts_postprocess_get_out_info == NULL) {
        ALOGE("%s,can not find postprocess getinfo function,%s\n", __FUNCTION__, dlerror());
    } else {
        ALOGV("<%s::%d>--[dts_postprocess_getinfo:]", __FUNCTION__, __LINE__);
    }

    _aml_dts_postprocess_dynamic_parameter_set = (int (*)(void *, int, int *, int *, int *))dlsym(_DtsxDecoderLibHandler, "dtsx_postprocess_dynamic_parameter_set");
    if (_aml_dts_postprocess_dynamic_parameter_set == NULL) {
        ALOGE("%s,can not find postprocess dynamic_parameter_set function,%s\n", __FUNCTION__, dlerror());
    } else {
        ALOGV("<%s::%d>--[dts_postprocess_dynamic_parameter_set:]", __FUNCTION__, __LINE__);
    }

    ALOGI("[%s:%d] out", __func__, __LINE__);
    return 0;
}

static int _dtsx_dualcore_init(dtsx_dec_t *p_dtsx_dec)
{
    aml_dec_t *aml_dec = &p_dtsx_dec->aml_dec;
    struct aml_audio_device *adev = (struct aml_audio_device *)(aml_dec->dev);
    ALOGI("[%s:%d] in", __func__, __LINE__);
    if (!p_dtsx_dec) {
        ALOGE("p_dtsx_dec is NULL");
        goto DTSX_DUALCORE_INIT_FAIL;
    }

    int cmd_count = 0;
    int ret = 0;

    if (_get_dtsx_function_symbol() < 0) {
        goto DTSX_DUALCORE_INIT_FAIL;
    }

    /* Prepare the init argv for core1 decoder */
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_core1_max_spkrout=%d", _dtsx_config_params.core1_dec_out);  // 2123(7.1 8 lanes) is hardcode
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_unalignedsyncword");
    _dtsx_config_params.dec_sink_dev_type = property_get_int32("vendor.media.audio.dtsx.sink.type", _dtsx_config_params.dec_sink_dev_type);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_dec_sinkdevtype=%d", _dtsx_config_params.dec_sink_dev_type);
    _dtsx_config_params.passthrough = property_get_int32("vendor.media.audio.dtsx.passthrough", _dtsx_config_params.passthrough);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_passthrough_enable=%d", _dtsx_config_params.passthrough);

    ret = (_aml_dts_decoder_init)(&p_dtsx_dec->p_dtsx_dec_inst, cmd_count, (const char **)(p_dtsx_dec->init_argv));
    if (ret != 0) {
        ALOGE("_aml_dts_decoder_init fail:%d", ret);
        goto DTSX_DUALCORE_INIT_FAIL;
    }

    /* Prepare the init argv for core2 post processing */
    cmd_count = 0;
    _dtsx_config_params.pp_sink_dev_type = property_get_int32("vendor.media.audio.dtsx.sink.type", _dtsx_config_params.pp_sink_dev_type);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_pp_sinkdevtype=%d", _dtsx_config_params.pp_sink_dev_type);
    _dtsx_config_params.core2_spkr_out = property_get_int32("vendor.media.audio.dtsx.speaker.out", _dtsx_config_params.core2_spkr_out);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_core2_spkrout=%d", _dtsx_config_params.core2_spkr_out);

    if (Check_VX_lib()) {
#ifdef DTS_VX_V4_ENABLE
    if (1 == VirtualX_getparameter(&adev->native_postprocess, VIRTUALX4_PARAM_ENABLE) && adev->vx_enable
            && 0 == VirtualX_getparameter(&adev->native_postprocess, PARAM_TSX_PROCESS_DISCARD_I32)) {
#else
    if (1 == VirtualX_getparameter(&adev->native_postprocess, DTS_PARAM_VX_ENABLE_I32) && adev->vx_enable
            && 0 == VirtualX_getparameter(&adev->native_postprocess, DTS_PARAM_TSX_PROCESS_DISCARD_I32)) {
#endif
            _dtsx_config_params.auto_config_out_for_vx = DTSX_CORE2_OUTPUT_TYPE_ATUO;
        } else {
            _dtsx_config_params.auto_config_out_for_vx = DTSX_CORE2_OUTPUT_TYPE_STEREO;
        }
    } else {
        _dtsx_config_params.auto_config_out_for_vx = DTSX_CORE2_OUTPUT_TYPE_DEFAULT;
    }
    _dtsx_config_params.auto_config_out_for_vx = property_get_int32("vendor.media.audio.dtsx.auto.config.out", _dtsx_config_params.auto_config_out_for_vx);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_config_output_for_vx=%d", _dtsx_config_params.auto_config_out_for_vx);

    // hybrid limiting in linked mode (Medium MIPS)
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_limitertype=%d", _dtsx_config_params.limiter_type[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_limitertype=%d", _dtsx_config_params.limiter_type[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_limitertype=%d", _dtsx_config_params.limiter_type[DTSX_OUTPUT_HP]);

    // Range: [-60, -10]
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_loudnesstarget=%d", _dtsx_config_params.loudness_target[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_loudnesstarget=%d", _dtsx_config_params.loudness_target[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_loudnesstarget=%d", _dtsx_config_params.loudness_target[DTSX_OUTPUT_HP]);

    // Loudness enable
    _dtsx_config_params.loudness_enable[DTSX_OUTPUT_SPK] = property_get_int32("vendor.media.audio.dtsx.loudnessenable", _dtsx_config_params.loudness_enable[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_loudnessenable=%d", _dtsx_config_params.loudness_enable[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_loudnessenable=%d", _dtsx_config_params.loudness_enable[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_loudnessenable=%d", _dtsx_config_params.loudness_enable[DTSX_OUTPUT_HP]);

    // DRC Medium profile setting
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_drcprofile=%d", _dtsx_config_params.drc_profile[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcprofile=%d", _dtsx_config_params.drc_profile[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_drcprofile=%d", _dtsx_config_params.drc_profile[DTSX_OUTPUT_HP]);

    // DRC Default Compression Curve for Legacy Music Standard
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_drcdefaultcurve=%d", _dtsx_config_params.drc_default_curve[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcdefaultcurve=%d", _dtsx_config_params.drc_default_curve[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_drcdefaultcurve=%d", _dtsx_config_params.drc_default_curve[DTSX_OUTPUT_HP]);

    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_drccutvalue=%d", _dtsx_config_params.drc_cut_value[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drccutvalue=%d", _dtsx_config_params.drc_cut_value[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_drccutvalue=%d", _dtsx_config_params.drc_cut_value[DTSX_OUTPUT_HP]);

    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_drcboostvalue=%d", _dtsx_config_params.drc_boost_value[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcboostvalue=%d", _dtsx_config_params.drc_boost_value[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_drcboostvalue=%d", _dtsx_config_params.drc_boost_value[DTSX_OUTPUT_HP]);

    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_spk_drcenable=%d", _dtsx_config_params.drc_enable[DTSX_OUTPUT_SPK]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcenable=%d", _dtsx_config_params.drc_enable[DTSX_OUTPUT_ARC]);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_hp_drcenable=%d", _dtsx_config_params.drc_enable[DTSX_OUTPUT_HP]);

    _dtsx_config_params.neuralx_up_mix = property_get_int32("vendor.media.audio.dtsx.upmixenable", _dtsx_config_params.neuralx_up_mix);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_neuralxupmixenable=%d", _dtsx_config_params.neuralx_up_mix);
    _dtsx_config_params.neox_down_mix = property_get_int32("vendor.media.audio.dtsx.downmixenable", _dtsx_config_params.neox_down_mix);
    snprintf(p_dtsx_dec->init_argv[cmd_count++], DTSX_PARAM_STRING_LEN, "dtsx_neoxdownmixenable=%d", _dtsx_config_params.neox_down_mix);

    ret = (_aml_dts_postprocess_init)(&p_dtsx_dec->p_dtsx_pp_inst, cmd_count, (const char **)(p_dtsx_dec->init_argv));
    if (ret != 0) {
        ALOGE("_aml_dts_decoder_process fail:%d", ret);
        goto DTSX_DUALCORE_INIT_FAIL;
    }

    ALOGI("[%s:%d] out", __func__, __LINE__);
    return 0;

DTSX_DUALCORE_INIT_FAIL:
    if (p_dtsx_dec->p_dtsx_dec_inst) {
        (_aml_dts_decoder_deinit)(p_dtsx_dec->p_dtsx_dec_inst);
    }

    _unload_dtsx_function_symbol();
    return -1;
}

static int _dtsx_stream_type_mapping(unsigned int stream_type)
{
    int dts_type = TYPE_DTS;
    int temp_stream_type;

    temp_stream_type = stream_type & (~DTSXDECSTRMTYPE_DTS_HEADPHONE);
    switch (temp_stream_type) {
        case DTSXDECSTRMTYPE_UNKNOWN:
        case DTSXDECSTRMTYPE_DTS_LEGACY:
        case DTSXDECSTRMTYPE_DTS_ES_MATRIX:
        case DTSXDECSTRMTYPE_DTS_ES_DISCRETE:
        case DTSXDECSTRMTYPE_DTS_9624:
        case DTSXDECSTRMTYPE_DTS_ES_8CH_DISCRETE:
        dts_type = TYPE_DTS;
            break;
        case DTSXDECSTRMTYPE_DTS_HIRES:
        case DTSXDECSTRMTYPE_DTS_MA:
        case DTSXDECSTRMTYPE_DTS_LBR:
        case DTSXDECSTRMTYPE_DTS_LOSSLESS:
            dts_type = TYPE_DTS_HD;
            break;
        case DTSXDECSTRMTYPE_DTS_UHD:
        case DTSXDECSTRMTYPE_DTS_UHD_MA:
        case DTSXDECSTRMTYPE_DTS_UHD_LOSSLESS:
        case DTSXDECSTRMTYPE_DTS_UHD_GAME:
            dts_type = TYPE_DTSX;
            break;
        default:
            dts_type = TYPE_DTS;
            break;
    }

    return dts_type;
}

void *dtsx_postprocess_threadloop(void *data)
{
    aml_dec_t *aml_dec = (aml_dec_t *)data;
    dtsx_dec_t *dtsx_dec = (dtsx_dec_t *)aml_dec;
    struct aml_audio_device *adev = (struct aml_audio_device *)(aml_dec->dev);
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    int in_frames = 0;
    int last_ch = 2;
    dtsx_dec_outbuf_t* out_buf;
    int ret = -1;

    ALOGI("[%s:%d] +<IN>", __func__, __LINE__);
    while (!dtsx_dec->dts_postprocess_thread_exit) {
        pthread_mutex_lock(&dtsx_dec->stream_lock);
        struct aml_stream_out *aml_out = (struct aml_stream_out *)dtsx_dec->stream;
        if (aml_out != NULL && (dtsx_dec->dec_buf[0].buf_is_full || aml_out->pause_status)) {
            //ALOGD("[%s:%d] IN dec0 is_full(%d)", __func__, __LINE__, dtsx_dec->dec_buf[0].buf_is_full);
            // we generally out data that is in dtsx_dec->dec_buf[0]
            out_buf = &dtsx_dec->dec_buf[0];
            // out_pause_new is called
            if (aml_out->pause_status) {
                //do fade out after pause
                if (dtsx_dec->dec_buf[1].buf_is_full && dtsx_dec->need_fade_out) {
                    //if pasue, dtsx_dec->dec_buf[0] will be empty, so we use data that is in dtsx_dec->dec_buf[1] to do fade out
                    if (dtsx_dec->dec_buf[0].buf_is_full) {
                        ALOGE("[%s:%d] the data that is in the dec_buf[0] is not output before pause", __func__, __LINE__);
                    }
                    out_buf = &dtsx_dec->dec_buf[1];
                    // do fade out here
                    if (dtsx_dec->pcm_out_info.channel_num != 0 && dtsx_dec->pcm_out_info.sample_rate != 0) {
                        int duration = out_buf->data_len * 1000 / dtsx_dec->pcm_out_info.channel_num / 2 / dtsx_dec->pcm_out_info.sample_rate;
                        _dtsx_ease_conifg(aml_dec, DTSX_EASE_OUT, duration, dtsx_dec->pcm_out_info.channel_num, dtsx_dec->pcm_out_info.sample_rate);
                        /*if (property_get_bool(AML_DTSX_PROP_DUMP_OUTPUT_PCM, 0)) {
                            aml_audio_dump_audio_bitstreams("/tmp/audio_dump/before_ease_out.pcm", out_buf->buf, out_buf->data_len);
                        }*/
                        if (-1 == aml_audio_ease_process(dtsx_dec->ease_handle, out_buf->buf, out_buf->data_len)) {
                            ALOGE("[%s:%d] aml_audio_ease_process fail", __func__, __LINE__);
                        }
                        /*if (property_get_bool(AML_DTSX_PROP_DUMP_OUTPUT_PCM, 0)) {
                            aml_audio_dump_audio_bitstreams("/tmp/audio_dump/after_ease_out.pcm", out_buf->buf, out_buf->data_len);
                        }*/
                    } else {
                        ALOGE("[%s:%d] Can not do ease out, since ch(%d) or sr(%d) is 0", __func__, __LINE__, dtsx_dec->pcm_out_info.channel_num, dtsx_dec->pcm_out_info.sample_rate);
                    }
                    //set state
                    dtsx_dec->need_fade_out = false;
                } else {// insert 0 data to continue output after pause and fade out is done
                    memset(out_buf->buf, 0x00, out_buf->data_len);
                }

                if (_dtsx_debug.debug_flag) {
                    ALOGD("[%s:%d] stream(%p), pause_status(%d), last_ch(%d), Insert 0 data size(%d)", __func__, __LINE__,
                    aml_out, aml_out->pause_status, last_ch, out_buf->data_len);
                }
            } else if (dtsx_dec->need_fade_in) {// start output data, do fade in
                // do fade in here
                if (dtsx_dec->ease_handle->ease_frames_elapsed != dtsx_dec->ease_handle->ease_frames) {
                    ALOGD("[%s:%d] now do ease in need_ease_frames(%d),ease_frames_elapsed(%d)", __func__, __LINE__,
                        dtsx_dec->ease_handle->ease_frames, dtsx_dec->ease_handle->ease_frames_elapsed);
                    /*if (property_get_bool(AML_DTSX_PROP_DUMP_OUTPUT_PCM, 0)) {
                        aml_audio_dump_audio_bitstreams("/tmp/audio_dump/aa_before_ease_in.pcm", out_buf->buf, out_buf->data_len);
                    }*/
                    if (-1 == aml_audio_ease_process(dtsx_dec->ease_handle, out_buf->buf, out_buf->data_len)) {
                        ALOGE("[%s:%d] aml_audio_ease_process fail", __func__, __LINE__);
                    }
                    /*if (property_get_bool(AML_DTSX_PROP_DUMP_OUTPUT_PCM, 0)) {
                        aml_audio_dump_audio_bitstreams("/tmp/audio_dump/aa_after_ease_in.pcm", out_buf->buf, out_buf->data_len);
                    }*/
                } else {
                    dtsx_dec->need_fade_in = false;
                    ALOGD("[%s:%d] do ease in done", __func__, __LINE__);
                }
            }
            // check if bytes_per_sample is valid
            if (dtsx_dec->pcm_out_info.bytes_per_sample != 2 && !aml_out->pause_status) {
                ALOGE("[%s:%d] bytes_per_sample(%d) is not 2", __func__, __LINE__, dtsx_dec->pcm_out_info.bytes_per_sample);
            }
            // check if channel_num is valid
            if (dtsx_dec->pcm_out_info.channel_num != 0) {
                last_ch = dtsx_dec->pcm_out_info.channel_num;
            } else if (!aml_out->pause_status) {
                ALOGE("[%s:%d] channel_num(%d) is 0", __func__, __LINE__, dtsx_dec->pcm_out_info.channel_num);
            }

            //ALOGD("[%s:%d] MIX before ch(%d),sample_rate(%d),byte(%d),frames(%d)", __func__, __LINE__,
            //dtsx_dec->pcm_out_info.channel_num, dtsx_dec->pcm_out_info.sample_rate, dtsx_dec->outlen_pcm, in_frames);
            if (adev->optical_format != AUDIO_FORMAT_PCM_16_BIT) {//When ARC output not PCM data, we mix sound here
                in_frames = out_buf->data_len / last_ch / 2;
                aml_hw_mixer_mch_in_mixing(&adev->hw_mixer, out_buf->buf, in_frames, AUDIO_FORMAT_PCM_16_BIT, last_ch);
                if (_dtsx_debug.fp_mixed_pcm) {
                    fwrite(out_buf->buf, 1, out_buf->data_len, _dtsx_debug.fp_mixed_pcm);
                }
            }
            //ALOGD("[%s:%d] dec0 is_full(%d) _aml_dts_postprocess_proc data_len(%d)", __func__, __LINE__,
            //dtsx_dec->dec_buf[0].buf_is_full, out_buf->data_len);
            ret = (_aml_dts_postprocess_proc)(dtsx_dec->p_dtsx_pp_inst,
                                              out_buf->buf, out_buf->data_len,
                                              dtsx_dec->a_dtsx_pp_output, dtsx_dec->a_dtsx_pp_output_size);
            if (_dtsx_debug.debug_flag) {
                ALOGD("[%s:%d] Core2 Bus0:(%d) Bus1:(%d) Bus2:(%d)", __func__, __LINE__,
                    dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0],
                    dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1],
                    dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS2]);
            }

            if (ret != 0) {
                ALOGE("[%s:%d] dtsx post process fail:%d", __func__, __LINE__, ret);
            } else {
                if (dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0] > 0) {
                    _dtsx_pcm_output(dtsx_dec);
                }

                if (dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1] > 0) {
                    _dtsx_raw_output(dtsx_dec);
                }
            }

            if ((dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0] > 0) || (dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS1] > 0)) {
                ///< get dts stream type, display audio info banner.
                if (!_aml_dts_decoder_get_parameter) {
                    dtsx_dec->stream_type = -1;
                    dtsx_dec->is_headphone_x = false;
                } else {
                    dtsx_stream_info_t stream_info;
                    memset(&stream_info, 0, sizeof(stream_info));
                    int ret = (*_aml_dts_decoder_get_parameter)(dtsx_dec->p_dtsx_dec_inst, DTSX_BITSTREAM_INFO, &stream_info);
                    if (!ret) {
                        dtsx_dec->stream_type = _dtsx_stream_type_mapping(stream_info.stream_type);
                        dtsx_dec->is_headphone_x = !!(stream_info.stream_type & DTSXDECSTRMTYPE_DTS_HEADPHONE);
                    } else {
                        dtsx_dec->stream_type = -1;
                        dtsx_dec->is_headphone_x = false;
                    }
                    if (_dtsx_debug.debug_flag) {
                        ALOGD("[%s:%d] stream_type:(%d)(0x%x) is_headphone_x:(%d)", __func__, __LINE__,
                            dtsx_dec->stream_type, stream_info.stream_type, dtsx_dec->is_headphone_x);
                    }
                }

                adev->dts_hd.stream_type = dtsx_dec->stream_type;
                //adev->dts_hd.is_headphone_x = dtsx_dec->is_headphone_x;//For the dtsx decoder, there is no need to display headphone
            }

            // output data to alsa
            //ALOGD("[%s:%d] output data to alsa", __func__, __LINE__);
            aml_audio_nonms12_output((struct audio_stream_out *)aml_out);
            //ALOGD("[%s:%d] set dec_output_data_need_process to false", __func__, __LINE__);
            out_buf->buf_is_full = false;
            //ALOGD("[%s:%d] OUT dec0 is_full(%d)", __func__, __LINE__, dtsx_dec->dec_buf[0].buf_is_full);
        }
        pthread_mutex_unlock(&dtsx_dec->stream_lock);
        usleep(1000);
    }

    ALOGI("[%s:%d] -<OUT>", __func__, __LINE__);
}

int dtsx_decoder_init_patch(aml_dec_t **ppaml_dec, aml_dec_config_t *dec_config)
{
    dtsx_dec_t *dtsx_dec = NULL;
    aml_dec_t  *aml_dec = NULL;
    int cmd_count = 0;

    ALOGI("%s enter", __func__);
    dtsx_dec = aml_audio_calloc(1, sizeof(dtsx_dec_t));
    if (dtsx_dec == NULL) {
        ALOGE("%s malloc dts_dec failed\n", __func__);
        return -1;
    }

    aml_dtsx_config_t *dtsx_config = &dec_config->dtsx_config;
    pthread_mutex_init(&dtsx_dec->stream_lock, NULL);
    pthread_mutex_lock(&dtsx_dec->stream_lock);
    dtsx_dec->stream = dtsx_config->stream;
    pthread_mutex_unlock(&dtsx_dec->stream_lock);
    aml_dec = &dtsx_dec->aml_dec;
    aml_dec->dev = dtsx_config->dev;

    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t *raw_in_data  = &aml_dec->raw_in_data;
    _dtsx_config_params.dec_sink_dev_type = dtsx_config->sink_dev_type;
    _dtsx_config_params.pp_sink_dev_type = dtsx_config->sink_dev_type;
    _dtsx_config_params.passthrough = dtsx_config->passthroug_enable;
    dtsx_dec->is_dtscd = dtsx_config->is_dtscd;
    dtsx_dec->is_iec61937 = dtsx_config->is_iec61937;
    dtsx_dec->is_hdmi_output = dtsx_config->is_hdmi_output;
    aml_dec->frame_cnt = 0;
    aml_dec->format = dtsx_config->format;
    dtsx_dec->stream_type = 0;
    dtsx_dec->is_headphone_x = false;
    dtsx_dec->init_argc = 0;
    dtsx_dec->init_argv[0] = aml_audio_malloc(DTSX_PARAM_COUNT_MAX * DTSX_PARAM_STRING_LEN);
    if (dtsx_dec->init_argv[0] == NULL) {
        ALOGE("%s malloc argv memory failed!", __func__);
        goto DTSX_INIT_FAIL;
    } else {
        int i = 0;
        memset(dtsx_dec->init_argv[0], 0, DTSX_PARAM_COUNT_MAX * DTSX_PARAM_STRING_LEN);
        for (i = 1; i < DTSX_PARAM_COUNT_MAX; i++) {
            dtsx_dec->init_argv[i] = dtsx_dec->init_argv[0] + (DTSX_PARAM_STRING_LEN * i);
        }
    }

    if (_dtsx_dualcore_init(dtsx_dec) < 0) {
        ALOGE("dtsx init fail");
        goto DTSX_INIT_FAIL;
    }

    dtsx_dec->status |= DTSX_INITED;
    dtsx_dec->remain_size = 0;
    dtsx_dec->half_frame_remain_size = 0;

    dtsx_dec->frame_info.syncword = 0;
    dtsx_dec->frame_info.syncword_pos = 0;
    dtsx_dec->frame_info.check_pos = 0;
    dtsx_dec->frame_info.is_little_endian = false;
    dtsx_dec->frame_info.iec61937_data_type = 0;
    dtsx_dec->frame_info.size = 0;

    dtsx_dec->inbuf_size = MAX_DTS_FRAME_LENGTH;
    dtsx_dec->inbuf = (unsigned char*) aml_audio_malloc(dtsx_dec->inbuf_size);
    dtsx_dec->dec_pcm_out_size = 4096 * 8 * 4;  // FIXME: hardcode: 4096 samples * 8ch * 32bit
    dtsx_dec->dec_pcm_out_buf = (unsigned char*) aml_audio_malloc(dtsx_dec->dec_pcm_out_size);
    dec_pcm_data->buf_size = MAX_DTS_FRAME_LENGTH * 2;
    dec_pcm_data->buf = (unsigned char *)aml_audio_malloc(dec_pcm_data->buf_size);
    dec_raw_data->buf_size = MAX_DTS_FRAME_LENGTH * 2;
    dec_raw_data->buf = (unsigned char *)aml_audio_malloc(dec_raw_data->buf_size);
    for (int i = 0; i < 2; i++) {
        dtsx_dec->dec_buf[i].buf_size = MAX_DTS_FRAME_LENGTH * 2;
        dtsx_dec->dec_buf[i].buf = (unsigned char *)aml_audio_malloc(dtsx_dec->dec_buf[i].buf_size);
        dtsx_dec->dec_buf[i].buf_is_full = false;
        dtsx_dec->dec_buf[i].data_len = 0;
    }
    if (!dec_pcm_data->buf || !dec_raw_data->buf || !dtsx_dec->inbuf || !dtsx_dec->dec_pcm_out_buf
        || !dtsx_dec->dec_buf[0].buf || !dtsx_dec->dec_buf[1].buf) {
        ALOGE("[%s:%d] malloc memory failed!", __func__, __LINE__);
        goto DTSX_INIT_FAIL;
    }
    memset(dec_pcm_data->buf, 0, dec_pcm_data->buf_size);
    memset(dec_raw_data->buf , 0, dec_raw_data->buf_size);
    memset(dtsx_dec->inbuf, 0, dtsx_dec->inbuf_size);
    memset(dtsx_dec->dec_pcm_out_buf, 0, dtsx_dec->dec_pcm_out_size);
    memset(raw_in_data, 0, sizeof(dec_data_info_t));  ///< no use
    memset(dtsx_dec->dec_buf[0].buf, 0, dtsx_dec->dec_buf[0].buf_size);
    memset(dtsx_dec->dec_buf[1].buf, 0, dtsx_dec->dec_buf[1].buf_size);
    dtsx_dec->need_fade_in = true;
    dtsx_dec->need_fade_out = false;

    if (ring_buffer_init(&dtsx_dec->input_ring_buf, MAX_DTS_FRAME_LENGTH * 4)) {
        ALOGE("%s init ring buffer failed!", __func__);
        goto DTSX_INIT_FAIL;
    }

    if (property_get_bool(AML_DTSX_PROP_DUMP_INPUT_RAW, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdtsx_input_raw.dts", AML_DTSX_DUMP_FILE_DIR);
        _dtsx_debug.fp_input_raw = fopen(name, "ab+");
        if (!_dtsx_debug.fp_input_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }

        memset(name, 0x00, sizeof(name));
        snprintf(name, 64, "%sdtsx_dec_in_raw.dts", AML_DTSX_DUMP_FILE_DIR);
        _dtsx_debug.fp_dec_in_raw = fopen(name, "ab+");
        if (!_dtsx_debug.fp_dec_in_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DTSX_PROP_DUMP_DECODE_PCM, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdtsx_decode_pcm.pcm", AML_DTSX_DUMP_FILE_DIR);
        _dtsx_debug.fp_decode_pcm = fopen(name, "a+");
        if (!_dtsx_debug.fp_decode_pcm) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DTSX_PROP_DUMP_MIXED_PCM, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdtsx_mixed_pcm.pcm", AML_DTSX_DUMP_FILE_DIR);
        _dtsx_debug.fp_mixed_pcm = fopen(name, "a+");
        if (!_dtsx_debug.fp_mixed_pcm) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DTSX_PROP_DUMP_OUTPUT_RAW, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdtsx_output_raw.dts", AML_DTSX_DUMP_FILE_DIR);
        _dtsx_debug.fp_output_raw = fopen(name, "ab+");
        if (!_dtsx_debug.fp_output_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DTSX_PROP_DUMP_OUTPUT_PCM, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdtsx_%d_%dch.pcm", AML_DTSX_DUMP_FILE_DIR, 48000, 2);
        _dtsx_debug.fp_pcm = fopen(name, "ab+");
        if (!_dtsx_debug.fp_pcm) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DTSX_PROP_DEBUG_FLAG, 0)) {
        ALOGD("enable dtsx debug log");
        _dtsx_debug.debug_flag = true;
    } else {
        ALOGD("disable dtsx debug log");
        _dtsx_debug.debug_flag = false;
    }

    dtsx_dec->dts_postprocess_thread_exit  = false;
    int ret = pthread_create(&dtsx_dec->dts_postprocess_threadID, NULL,
                          &dtsx_postprocess_threadloop, aml_dec);
    dtsx_dec->ease_handle = NULL;
    if (-1 == aml_audio_ease_init(&dtsx_dec->ease_handle)) {
        ALOGE("[%s:%d] aml_audio_ease_init failed", __func__, __LINE__);
    }

    *ppaml_dec = aml_dec;
    _dtsx_dec = dtsx_dec;

    if (ret != 0) {
        ALOGE("[%s:%d] Create dts_postprocess_thread failed", __func__, __LINE__);
    } else {
        ALOGI("[%s:%d] success stream(%p)", __func__, __LINE__, dtsx_dec->stream);
    }
    return 0;

DTSX_INIT_FAIL:
    if (dtsx_dec) {
        if (dtsx_dec->inbuf) {
            aml_audio_free(dtsx_dec->inbuf);
            dtsx_dec->inbuf = NULL;
        }
        if (dtsx_dec->dec_pcm_out_buf) {
            aml_audio_free(dtsx_dec->dec_pcm_out_buf);
            dtsx_dec->dec_pcm_out_buf = NULL;
        }
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }
        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
            dec_raw_data->buf = NULL;
        }
        if (dtsx_dec->p_dtsx_dec_inst && _aml_dts_decoder_deinit) {
            (_aml_dts_decoder_deinit)(dtsx_dec->p_dtsx_dec_inst);
            dtsx_dec->p_dtsx_dec_inst = NULL;
        }
        if (dtsx_dec->p_dtsx_pp_inst && _aml_dts_postprocess_deinit) {
            (_aml_dts_postprocess_deinit)(dtsx_dec->p_dtsx_pp_inst);
            dtsx_dec->p_dtsx_pp_inst = NULL;
        }
        ring_buffer_release(&dtsx_dec->input_ring_buf);
        aml_audio_free(dtsx_dec);
        aml_dec = NULL;
        *ppaml_dec = NULL;
    }
    _unload_dtsx_function_symbol();
    ALOGE("%s failed", __func__);
    return -1;
}

int dtsx_decoder_release_patch(aml_dec_t *aml_dec)
{
    dtsx_dec_t *dtsx_dec = (dtsx_dec_t *)aml_dec;
    struct aml_audio_device *adev = (struct aml_audio_device *)(aml_dec->dev);
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    int ret = -1;
    ALOGI("%s enter", __func__);

    dtsx_dec->dts_postprocess_thread_exit = 1;
    ret = pthread_join(dtsx_dec->dts_postprocess_threadID, NULL);
    if (ret != 0) {
        ALOGW("[%s:%d] Pthread join failed", __func__, __LINE__);
    }
    pthread_mutex_lock(&dtsx_dec->stream_lock);
    dtsx_dec->stream = NULL;
    pthread_mutex_unlock(&dtsx_dec->stream_lock);

    if (dtsx_dec->p_dtsx_dec_inst && _aml_dts_decoder_deinit) {
        (_aml_dts_decoder_deinit)(dtsx_dec->p_dtsx_dec_inst);
        dtsx_dec->p_dtsx_dec_inst = NULL;
    }
    if (dtsx_dec->p_dtsx_pp_inst && _aml_dts_postprocess_deinit) {
        (_aml_dts_postprocess_deinit)(dtsx_dec->p_dtsx_pp_inst);
        dtsx_dec->p_dtsx_pp_inst = NULL;
    }
    _unload_dtsx_function_symbol();

    if (dtsx_dec->ease_handle) {
        aml_audio_ease_close(dtsx_dec->ease_handle);
    }

    if (dtsx_dec) {
        if (dtsx_dec->inbuf) {
            aml_audio_free(dtsx_dec->inbuf);
            dtsx_dec->inbuf = NULL;
        }
        if (dtsx_dec->dec_pcm_out_buf) {
            aml_audio_free(dtsx_dec->dec_pcm_out_buf);
            dtsx_dec->dec_pcm_out_buf = NULL;
        }
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }
        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
            dec_raw_data->buf = NULL;
        }
        ring_buffer_release(&dtsx_dec->input_ring_buf);

        if (_dtsx_debug.fp_input_raw) {
            fclose(_dtsx_debug.fp_input_raw);
            _dtsx_debug.fp_input_raw = NULL;
        }

        if (_dtsx_debug.fp_dec_in_raw) {
            fclose(_dtsx_debug.fp_dec_in_raw);
            _dtsx_debug.fp_dec_in_raw = NULL;
        }

        if (_dtsx_debug.fp_decode_pcm) {
            fclose(_dtsx_debug.fp_decode_pcm);
            _dtsx_debug.fp_decode_pcm = NULL;
        }

        if (_dtsx_debug.fp_mixed_pcm) {
            fclose(_dtsx_debug.fp_mixed_pcm);
            _dtsx_debug.fp_mixed_pcm = NULL;
        }

        if (_dtsx_debug.fp_output_raw) {
            fclose(_dtsx_debug.fp_output_raw);
            _dtsx_debug.fp_output_raw = NULL;
        }

        if (_dtsx_debug.fp_pcm) {
            fclose(_dtsx_debug.fp_pcm);
            _dtsx_debug.fp_pcm = NULL;
        }

        adev = (struct aml_audio_device *)(aml_dec->dev);
        adev->dts_hd.stream_type = 0;
        adev->dts_hd.is_headphone_x = false;
        aml_dec->frame_cnt = 0;

        pthread_mutex_destroy(&dtsx_dec->stream_lock);
        aml_audio_free(dtsx_dec);
        dtsx_dec = NULL;
        _dtsx_dec = NULL;
    }
    ALOGI("%s exit", __func__);
    return 1;
}

static int _dtsx_decoder_copy_data_to_dec_buf(dtsx_dec_outbuf_t* dec_buf, unsigned char *buffer, int bytes)
{
    if (dec_buf->buf_size < bytes) {
        dec_buf->buf = aml_audio_realloc(dec_buf->buf, bytes);
        if (dec_buf->buf  == NULL) {
            ALOGE("[%s:%d] realloc for decode buffer(%u) failed", __func__, __LINE__, bytes);
            return -1;
        }
        dec_buf->buf_size = bytes;
    }
    memcpy(dec_buf->buf, buffer, bytes);
    dec_buf->data_len = bytes;
    dec_buf->buf_is_full = true;
    return 0;
}

int dtsx_decoder_process_patch(aml_dec_t *aml_dec, struct audio_buffer *abuffer)
{
    dtsx_dec_t *dtsx_dec = NULL;
    int bytes = abuffer->size;
    char * buffer = abuffer->buffer;
    struct aml_audio_device *adev = NULL;
    struct ring_buffer *input_rbuffer = NULL;
    dec_data_info_t *dec_pcm_data = NULL;
    dec_data_info_t *dec_raw_data = NULL;
    int frame_size = 0;
    int ret = 0, bits_per_sample = 0;
    unsigned char *dec_pcm_out = NULL;

    if (!aml_dec || !buffer) {
        ALOGE("[%s:%d] Invalid parameter: %s %s", __func__, __LINE__, DCA_CHECK_NULL_STR(aml_dec), DCA_CHECK_NULL_STR(buffer));
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    dtsx_dec = (dtsx_dec_t *)aml_dec;
    input_rbuffer = &dtsx_dec->input_ring_buf;
    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_raw_data = &aml_dec->dec_raw_data;
    dtsx_dec->stream_type = TYPE_DTS;
    dtsx_dec->is_headphone_x = false;

    adev = (struct aml_audio_device *)(aml_dec->dev);
    if (!adev) {
        ALOGE("[%s:%d] Invalid parameter %s", __func__, __LINE__, DCA_CHECK_NULL_STR(adev));
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    if (bytes > 0) {
        if (_dtsx_debug.fp_input_raw) {
            fwrite(buffer, 1, bytes, _dtsx_debug.fp_input_raw);
        }

        if (get_buffer_write_space(input_rbuffer) >= bytes) {
            ring_buffer_write(input_rbuffer, buffer, bytes, 0);
            dtsx_dec->remain_size += bytes;
            if (_dtsx_debug.debug_flag) {
                ALOGD("[%s:%d] remain:%d input data size:%d" , __func__, __LINE__, dtsx_dec->remain_size, bytes);
            }
        } else {
            ALOGE("[%s:%d] ring buffer haven`t enough space, lost data size:%d", __func__, __LINE__, bytes);
            ring_buffer_reset(input_rbuffer);
            dtsx_dec->remain_size = 0;
            return AML_DEC_RETURN_TYPE_FAIL;
        }
    }

    {
        frame_size = _dtsx_frame_scan(dtsx_dec);
        if (frame_size > 0) {
            if (dtsx_dec->is_iec61937 && frame_size % 2) { // P2 odd frame size
                ring_buffer_read(input_rbuffer, dtsx_dec->inbuf, frame_size + 1);
                dtsx_dec->remain_size -= (frame_size + 1);
                if (!dtsx_dec->frame_info.is_little_endian) {
                    //for odd frame size and big endian, the last audio byte need to be come from high address
                    dtsx_dec->inbuf[frame_size - 1] = dtsx_dec->inbuf[frame_size];
                }
            } else {
                ring_buffer_read(input_rbuffer, dtsx_dec->inbuf, frame_size);
                dtsx_dec->remain_size -= frame_size;
                if (frame_size % 2 && !dtsx_dec->frame_info.is_p2_codec) {
                    dtsx_dec->inbuf[frame_size] = 0;
                    frame_size = frame_size + 1;
                }
            }
            if (dtsx_dec->is_iec61937 && !dtsx_dec->frame_info.is_little_endian) {
                endian16_convert(dtsx_dec->inbuf, frame_size);
            }
        }
    }

    if (frame_size > 0) {
        if (_dtsx_debug.fp_dec_in_raw) {
            fwrite(dtsx_dec->inbuf, 1, frame_size, _dtsx_debug.fp_dec_in_raw);
        }
        dtsx_dec->outlen_pcm = 0;
        ret = (_aml_dts_decoder_process)(dtsx_dec->p_dtsx_dec_inst,
                                         dtsx_dec->inbuf,
                                         frame_size,
                                         dtsx_dec->a_dtsx_pp_output,
                                         &dtsx_dec->outlen_pcm);
        if ((ret != 0) || (dtsx_dec->outlen_pcm == 0)) {
            ALOGW("[%s:%d] dtsx decode fail:%d, outlen_pcm:%d", __func__, __LINE__, ret, dtsx_dec->outlen_pcm);
            return AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN;
        }

        ret = (_aml_dts_decoder_get_output_info)(dtsx_dec->p_dtsx_dec_inst, 0,
                                         &dtsx_dec->pcm_out_info.sample_rate,
                                         &dtsx_dec->pcm_out_info.channel_num,
                                         &bits_per_sample);
        if (ret != 0) {
            ALOGW("[%s:%d] dtsx decode fail:%d", __func__, __LINE__, ret);
            return AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN;
        }

        dtsx_dec->pcm_out_info.bytes_per_sample = bits_per_sample / 8;
        if (_dtsx_debug.debug_flag) {
            ALOGD("[%s:%d] Core1 pcm(len:%d, sr:%d, ch:%d)", __func__, __LINE__,
            dtsx_dec->outlen_pcm, dtsx_dec->pcm_out_info.sample_rate, dtsx_dec->pcm_out_info.channel_num);
        }

        ret = (_aml_dts_metadata_update)(dtsx_dec->p_dtsx_dec_inst, dtsx_dec->p_dtsx_pp_inst);
        if (ret != 0) {
            ALOGW("[%s:%d] dtsx metadata update fail:%d", __func__, __LINE__, ret);
        }
#if 0
        if (dtsx_dec->outlen_pcm > dec_pcm_data->buf_size) {
            ALOGI("[%s:%d] realloc decode buffer from (%d) to (%u)", __func__, __LINE__, dec_pcm_data->buf_size, dtsx_dec->outlen_pcm);
            dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, dtsx_dec->outlen_pcm);
            if (dec_pcm_data->buf == NULL) {
                ALOGE("[%s:%d] realloc for decode buffer(%u) failed", __func__, __LINE__, dtsx_dec->outlen_pcm);
                return AML_DEC_RETURN_TYPE_FAIL;
            }
            dec_pcm_data->buf_size = dtsx_dec->outlen_pcm;
        }
        memcpy(dec_pcm_data->buf, dtsx_dec->a_dtsx_pp_output[DTS_DSP2_OUTPUTBUS0], dtsx_dec->outlen_pcm);

        if (aml_dec->frame_cnt == 0) {
            ALOGI("[%s:%d] mute the first frame", __func__, __LINE__);
            memset(dec_pcm_data->buf, 0, dtsx_dec->outlen_pcm);
        }
#endif
        // dump dec core#1 output data
        if (_dtsx_debug.fp_decode_pcm) {
            fwrite(dtsx_dec->a_dtsx_pp_output[DTS_DSP2_OUTPUTBUS0], 1, dtsx_dec->outlen_pcm, _dtsx_debug.fp_decode_pcm);
        }

        //we use two dec_buf to do fade in/out
        //dec_buf[0]: save the data that needs to be output now.
        //dec_buf[1]: save the data that needs to be output after dec_buf[0]
        if (!dtsx_dec->dec_buf[0].buf_is_full && dtsx_dec->dec_buf[1].buf_is_full) {
            //ALOGD("[%s:%d] Step 2", __func__, __LINE__);
            // dec_buf[0] is empty and dec_buf[1] is full, we need move dec_buf[1] data to dec_buf[0], then save dec out data to dec_buf[1]
            // dec_buf[0] will be output in dtsx_postprocess_threadloop()
            if (-1 == _dtsx_decoder_copy_data_to_dec_buf(&dtsx_dec->dec_buf[0], dtsx_dec->dec_buf[1].buf, dtsx_dec->dec_buf[1].data_len)) {
                return AML_DEC_RETURN_TYPE_FAIL;
            }
            if (-1 == _dtsx_decoder_copy_data_to_dec_buf(&dtsx_dec->dec_buf[1], dtsx_dec->a_dtsx_pp_output[DTS_DSP2_OUTPUTBUS0], dtsx_dec->outlen_pcm)) {
                return AML_DEC_RETURN_TYPE_FAIL;
            }
        } else if (!dtsx_dec->dec_buf[0].buf_is_full && !dtsx_dec->dec_buf[1].buf_is_full) {
            //ALOGD("[%s:%d] Step 1", __func__, __LINE__);
            // both dec_buf[0] and dec_buf[1] are empty, we need queue dec out data to dec_buf[1] and not output until wait next dec out data come
            // need set need_fade_in to true then do fade in when data output in dtsx_postprocess_threadloop()
            if (-1 == _dtsx_decoder_copy_data_to_dec_buf(&dtsx_dec->dec_buf[1], dtsx_dec->a_dtsx_pp_output[DTS_DSP2_OUTPUTBUS0], dtsx_dec->outlen_pcm)) {
                return AML_DEC_RETURN_TYPE_FAIL;
            } else {
                //set ease in config
                if (dtsx_dec->pcm_out_info.channel_num != 0 && dtsx_dec->pcm_out_info.sample_rate != 0) {
                    int duration = 32;
                    _dtsx_ease_conifg(aml_dec, DTSX_EASE_IN, duration, dtsx_dec->pcm_out_info.channel_num, dtsx_dec->pcm_out_info.sample_rate);
                } else {
                    ALOGE("[%s:%d] Can not do ease in, since ch(%d) or sr(%d) is 0", __func__, __LINE__, dtsx_dec->pcm_out_info.channel_num, dtsx_dec->pcm_out_info.sample_rate);
                }
                //set ease in/out flag
                dtsx_dec->need_fade_in = true;//start to output, need do fade in
                dtsx_dec->need_fade_out = true;// after pasue, need to do fade out
                goto DEC_END;
            }
        }
        //ALOGD("[%s:%d] dec0 is_full(%d) before output", __func__, __LINE__, dtsx_dec->dec_buf[0].buf_is_full);
        while (dtsx_dec->dec_buf[0].buf_is_full) {//Need to wait until dtsx_dec->dec_buf[0] has been output in dtsx_postprocess_threadloop
            usleep(1000);
            if (dtsx_dec->dts_postprocess_thread_exit) {
                //process thread exit, dtsx_dec->dec_buf[0].buf_is_full may not be set to false,
                //so we set dtsx_dec->dec_buf[0].buf_is_full to false here, then the while loop can exit
                dtsx_dec->dec_buf[0].buf_is_full = false;
            }
        }
        //ALOGD("[%s:%d] dec0 is_full(%d) after output", __func__, __LINE__, dtsx_dec->dec_buf[0].buf_is_full);

DEC_END:
        if (dtsx_dec->a_dtsx_pp_output_size[DTS_DSP2_OUTPUTBUS0] > 0) {
            /* Cache a lot of data, needs to be decoded multiple times. */
            aml_dec->frame_cnt++;
            return AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN;
        } else {
            return AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN;
        }
    } else if (frame_size == 0) {
        return AML_DEC_RETURN_TYPE_CACHE_DATA;
    } else {
        return AML_DEC_RETURN_TYPE_FAIL;
    }
}

#if 0
int dca_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t *aml_dec_info)
{
    int ret = -1;
    struct dca_dts_dec *dts_dec = (struct dca_dts_dec *)aml_dec;

    if (!dts_decoder_getinfo || !aml_dec_info || !dts_dec) {
        return ret;
    }

    if (!DCA_CHECK_STATUS(dts_dec->status, DCA_INITED)) {
        return ret;
    }

    switch (info_type) {
        case AML_DEC_STREAM_INFO:
        {
            dtsx_info_t dca_info;
            struct aml_audio_device *adev = (struct aml_audio_device *)(aml_dec->dev);
            memset(&dca_info, 0, sizeof(dca_info));
            ret = (*dts_decoder_getinfo)(DCA_STREAM_INFO, (dtsx_info_t *)&dca_info);
            if (ret >= 0) {
                aml_dec_info->dec_info.stream_ch = dca_info.stream_info.stream_ch;
                aml_dec_info->dec_info.stream_sr = dca_info.stream_info.stream_sr;
                aml_dec_info->dec_info.stream_bitrate = dca_info.stream_info.stream_bitrate;
                aml_dec_info->dec_info.stream_error_num = dca_info.stream_info.error_frames;
                aml_dec_info->dec_info.stream_drop_num = dca_info.stream_info.dropped_frames;
                aml_dec_info->dec_info.stream_decode_num = dca_info.stream_info.decode_frames;
                dts_dec->stream_type = _dts_stream_type_mapping(dca_info.stream_info.stream_type);
                dts_dec->is_headphone_x = !!(dca_info.stream_info.stream_type & DTSSTREAMTYPE_DTS_HEADPHONE);
                ///< aml_dec_info->dec_info.output_bLFE = // not support yet
            } else {
                memset(aml_dec_info, 0, sizeof(aml_dec_info_t));
            }
            adev->dts_hd.stream_type = dts_dec->stream_type;
            adev->dts_hd.is_headphone_x = dts_dec->is_headphone_x;
            break;
        }

        case AML_DEC_OUTPUT_INFO:
        {
            dtsx_info_t dca_info;
            memset(&dca_info, 0, sizeof(dca_info));
            ret = (*dts_decoder_getinfo)(DCA_OUTPUT_INFO, (dtsx_info_t *)&dca_info);
            if (ret >= 0) {
                aml_dec_info->dec_output_info.output_ch = dca_info.output_info.output_ch;
                aml_dec_info->dec_output_info.output_sr = dca_info.output_info.output_sr;
                aml_dec_info->dec_output_info.output_bitwidth = dca_info.output_info.output_bitwidth;
            } else {
                memset(aml_dec_info, 0, sizeof(aml_dec_info_t));
            }
            break;
        }

        default:
            break;
    }
    return ret;
}
#endif

int dtsx_get_out_ch_internal(void)
{
    if (!_aml_dts_postprocess_get_out_info || !_dtsx_dec || !_dtsx_dec->p_dtsx_pp_inst) {
        return 0;
    }

    int nSampleRate =0;
    int nChannel =0;
    int nBitWidth =0;

    //int rc = _aml_dts_postprocess_get_out_info(_dtsx_dec->p_dtsx_pp_inst, DTS_DSP2_OUTPUTBUS0, &nSampleRate, &nChannel, &nBitWidth);
    nChannel = _dtsx_dec->aml_dec.dec_pcm_data.data_pcm_ch;
    if (nChannel == 0) {
        ALOGE("[%s:%d] _aml_dts_postprocess_get_out_info fail", __func__, __LINE__);
        return -1;
    } else {
        if (_dtsx_debug.debug_flag) {
            ALOGD("[%s:%d] Pcm [BUS-0] output nChannel(%d)",
                __func__, __LINE__, nChannel);
        }
        return nChannel;
    }
}

static int _dtsx_auto_config_output_for_vx(dtsx_dec_t *dtsx_dec, int value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};
    if (dtsx_dec /*&& DCA_CHECK_STATUS(dtsx_dec->status, DCA_INITED)*/) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_config_output_for_vx=%d", value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    }

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX auto config output for vx  failed", __func__, __LINE__);
    } else {
        ALOGI("[%s:%d] DTSX auto config output for vx success", __func__, __LINE__);
        _dtsx_config_params.auto_config_out_for_vx = value;
    }

    return ret;
}

int dtsx_set_out_ch_internal(int ch_num)
{
    if (!_aml_dts_postprocess_dynamic_parameter_set || !_dtsx_dec || !_dtsx_dec->p_dtsx_pp_inst) {
        ///< static param, will take effect after decoder_init.
        _dtsx_config_params.auto_config_out_for_vx = ch_num;
        ALOGI("DTSX Channel Output Mode = %d",ch_num);
        return 0;
    }

    return _dtsx_auto_config_output_for_vx(_dtsx_dec, ch_num);
}


void dtsx_reset_config_params(void)
{
    int i = 0;
    if (_dtsx_dec && _dtsx_dec->init_argv) {
        for (i = 0; i < DTSX_PARAM_COUNT_MAX; i++) {
            if (_dtsx_dec->init_argv[i]) {
                memset(_dtsx_dec->init_argv[i], 0, DTSX_PARAM_STRING_LEN);
            }
        }
    }
    _dtsx_dec->init_argc = 0;
    ALOGI("[%s:%d] DTSX reset config params success", __func__, __LINE__);
    return ;
}

int dtsx_set_postprocess_dynamic_parameter(char *cmd)
{
    int ret = -1;

    if (_dtsx_dec && _dtsx_dec->init_argv) {
        dtsx_reset_config_params();
        snprintf(_dtsx_dec->init_argv[_dtsx_dec->init_argc++], DTSX_PARAM_STRING_LEN, "%s", cmd);
        ret = (_aml_dts_postprocess_dynamic_parameter_set)(_dtsx_dec->p_dtsx_pp_inst, _dtsx_dec->init_argc, _dtsx_dec->init_argv);
        if (ret != 0) {
            ALOGW("[%s:%d] DTSX dynamic set parameter failed", __func__, __LINE__);
        }
    } else {
        ALOGE("[%s:%d] DTSX is uninitialized, cmd:%s", __func__, __LINE__, cmd);
    }

    return ret;
}

int dtsx_spk_drc_enable(bool enable)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_drcenable=%d", enable);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_enable[DTSX_OUTPUT_SPK] = enable;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX spk drc %sable failed", __func__, __LINE__, enable ? "en" : "dis");
    } else {
        ALOGI("[%s:%d] DTSX spk drc %sable success", __func__, __LINE__, enable ? "en" : "dis");
    }

    return ret;
}

int dtsx_transcoder_drc_enable(bool enable)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcenable=%d", enable);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_enable[DTSX_OUTPUT_ARC] = enable;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX transcoder drc %sable failed", __func__, __LINE__, enable ? "en" : "dis");
    } else {
        ALOGI("[%s:%d] DTSX transcoder drc %sable success", __func__, __LINE__, enable ? "en" : "dis");
    }

    return ret;
}

int dtsx_hp_drc_enable(bool enable)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_drcenable=%d", enable);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_enable[DTSX_OUTPUT_HP] = enable;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX hp drc %sable failed", __func__, __LINE__, enable ? "en" : "dis");
    } else {
        ALOGI("[%s:%d] DTSX hp drc %sable success", __func__, __LINE__, enable ? "en" : "dis");
    }
    return ret;
}

int dtsx_spk_drc_profile(int profile)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (profile < DTSX2_DEC_DRC_PROFILE_LOW || profile >= DTSX2_DEC_DRC_PROFILE_MAX) {
        ALOGW("[%s:%d] DTSX set spk drc profile:%d failed", __func__, __LINE__, profile);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_drcprofile=%d", profile);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_profile[DTSX_OUTPUT_SPK] = profile;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set spk drc profile:%d failed", __func__, __LINE__, profile);
    } else {
        ALOGI("[%s:%d] DTSX set spk drc profile:%d success", __func__, __LINE__, profile);
    }
    return ret;
}

int dtsx_transcoder_drc_profile(int profile)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (profile < DTSX2_DEC_DRC_PROFILE_LOW || profile >= DTSX2_DEC_DRC_PROFILE_MAX) {
        ALOGW("[%s:%d] DTSX set transcoder drc profile:%d failed", __func__, __LINE__, profile);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcprofile=%d", profile);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_profile[DTSX_OUTPUT_ARC] = profile;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set transcoder drc profile:%d failed", __func__, __LINE__, profile);
    } else {
        ALOGI("[%s:%d] DTSX set transcoder drc profile:%d success", __func__, __LINE__, profile);
    }
    return ret;
}

int dtsx_hp_drc_profile(int profile)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (profile < DTSX2_DEC_DRC_PROFILE_LOW || profile >= DTSX2_DEC_DRC_PROFILE_MAX) {
        ALOGW("[%s:%d] DTSX set hp drc profile:%d failed", __func__, __LINE__, profile);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_drcprofile=%d", profile);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_profile[DTSX_OUTPUT_HP] = profile;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set hp drc profile:%d failed", __func__, __LINE__, profile);
    } else {
        ALOGI("[%s:%d] DTSX set hp drc profile:%d success", __func__, __LINE__, profile);
    }
    return ret;
}

int dtsx_spk_drc_default_curve(int default_curve)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (default_curve < DTSX_DEC_DRC_DEFAULT_CURVE_NO_COMPRESSION ||
        default_curve >= DTSX_DEC_DRC_DEFAULT_CURVE_MAX) {
        ALOGW("[%s:%d] DTSX set spk drc default curve:%d failed", __func__, __LINE__, default_curve);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_drcdefaultcurve=%d", default_curve);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_default_curve[DTSX_OUTPUT_SPK] = default_curve;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set spk drc default curve:%d failed", __func__, __LINE__, default_curve);
    } else {
        ALOGI("[%s:%d] DTSX set spk drc default curve:%d success", __func__, __LINE__, default_curve);
    }
    return ret;
}

int dtsx_transcoder_drc_default_curve(int default_curve)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (default_curve < DTSX_DEC_DRC_DEFAULT_CURVE_NO_COMPRESSION ||
        default_curve >= DTSX_DEC_DRC_DEFAULT_CURVE_MAX) {
        ALOGW("[%s:%d] DTSX set transcoder drc default curve:%d failed", __func__, __LINE__, default_curve);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcdefaultcurve=%d", default_curve);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_default_curve[DTSX_OUTPUT_ARC] = default_curve;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set transcoder drc default curve:%d failed", __func__, __LINE__, default_curve);
    } else {
        ALOGI("[%s:%d] DTSX set transcoder drc default curve:%d success", __func__, __LINE__, default_curve);
    }
    return ret;
}

int dtsx_hp_drc_default_curve(int default_curve)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (default_curve < DTSX_DEC_DRC_DEFAULT_CURVE_NO_COMPRESSION ||
        default_curve >= DTSX_DEC_DRC_DEFAULT_CURVE_MAX) {
        ALOGW("[%s:%d] DTSX set hp drc default curve:%d failed", __func__, __LINE__, default_curve);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_drcdefaultcurve=%d", default_curve);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_default_curve[DTSX_OUTPUT_HP] = default_curve;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set hp drc default curve:%d failed", __func__, __LINE__, default_curve);
    } else {
        ALOGI("[%s:%d] DTSX set hp drc default curve:%d success", __func__, __LINE__, default_curve);
    }
    return ret;
}

int dtsx_spk_drc_cut_value(int cut_value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (cut_value < DTSX_DRC_CUT_VALUE_MIN || cut_value > DTSX_DRC_CUT_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set spk drc cut value:%d failed", __func__, __LINE__, cut_value);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_drccutvalue=%d", cut_value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_cut_value[DTSX_OUTPUT_SPK] = cut_value;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX spk drc cut value:%d failed", __func__, __LINE__, cut_value);
    } else {
        ALOGI("[%s:%d] DTSX spk drc cut value:%d success", __func__, __LINE__, cut_value);
    }
    return ret;
}

int dtsx_transcoder_drc_cut_value(int cut_value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (cut_value < DTSX_DRC_CUT_VALUE_MIN || cut_value > DTSX_DRC_CUT_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set transcoder drc cut value:%d failed", __func__, __LINE__, cut_value);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drccutvalue=%d", cut_value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_cut_value[DTSX_OUTPUT_ARC] = cut_value;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set transcoder drc cut value:%d failed", __func__, __LINE__, cut_value);
    } else {
        ALOGI("[%s:%d] DTSX set transcoder drc cut value:%d success", __func__, __LINE__, cut_value);
    }
    return ret;
}

int dtsx_hp_drc_cut_value(int cut_value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (DTSX_DRC_CUT_VALUE_MIN < 0 || cut_value > DTSX_DRC_CUT_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set hp drc cut value:%d failed", __func__, __LINE__, cut_value);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_drccutvalue=%d", cut_value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_cut_value[DTSX_OUTPUT_HP] = cut_value;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set hp drc cut value:%d failed", __func__, __LINE__, cut_value);
    } else {
        ALOGI("[%s:%d] DTSX set hp drc cut value:%d success", __func__, __LINE__, cut_value);
    }
    return ret;
}

int dtsx_spk_drc_boost_value(int boost_value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (boost_value < DTSX_DRC_BOOST_VALUE_MIN || boost_value > DTSX_DRC_BOOST_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set spk drc boost value:%d failed", __func__, __LINE__, boost_value);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_drcboostvalue=%d", boost_value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_boost_value[DTSX_OUTPUT_SPK] = boost_value;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set spk drc boost value:%d failed", __func__, __LINE__, boost_value);
    } else {
        ALOGI("[%s:%d] DTSX set spk drc boost value:%d success", __func__, __LINE__, boost_value);
    }
    return ret;
}

int dtsx_transcoder_drc_boost_value(int boost_value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (boost_value < DTSX_DRC_BOOST_VALUE_MIN || boost_value > DTSX_DRC_BOOST_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set transcoder drc boost value:%d failed", __func__, __LINE__, boost_value);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_drcboostvalue=%d", boost_value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_boost_value[DTSX_OUTPUT_ARC] = boost_value;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set transcoder drc boost value:%d failed", __func__, __LINE__, boost_value);
    } else {
        ALOGI("[%s:%d] DTSX set transcoder drc boost value:%d success", __func__, __LINE__, boost_value);
    }
    return ret;
}

int dtsx_hp_drc_boost_value(int boost_value)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (boost_value < DTSX_DRC_BOOST_VALUE_MIN || boost_value > DTSX_DRC_BOOST_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set hp drc boost value:%d failed", __func__, __LINE__, boost_value);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_drcboostvalue=%d", boost_value);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.drc_boost_value[DTSX_OUTPUT_HP] = boost_value;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set hp drc boost value:%d failed", __func__, __LINE__, boost_value);
    } else {
        ALOGI("[%s:%d] DTSX set hp drc boost value:%d success", __func__, __LINE__, boost_value);
    }
    return ret;
}

int dtsx_spk_loudness_enable(bool enable)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_loudnessenable=%d", enable);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.loudness_enable[DTSX_OUTPUT_SPK] = enable;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX spk loudness %sable failed", __func__, __LINE__, enable ? "en" : "dis");
    } else {
        ALOGI("[%s:%d] DTSX spk loudness %sable success", __func__, __LINE__, enable ? "en" : "dis");
    }
    return ret;
}

int dtsx_transcoder_loudness_enable(bool enable)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_loudnessenable=%d", enable);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.loudness_enable[DTSX_OUTPUT_ARC] = enable;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX transcoder loudness %sable failed", __func__, __LINE__, enable ? "en" : "dis");
    }else {
        ALOGI("[%s:%d] DTSX transcoder loudness %sable success", __func__, __LINE__, enable ? "en" : "dis");
    }
    return ret;
}

int dtsx_hp_loudness_enable(bool enable)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_loudnessenable=%d", enable);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.loudness_enable[DTSX_OUTPUT_HP] = enable;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX hp loudness %sable failed", __func__, __LINE__, enable ? "en" : "dis");
    } else {
        ALOGI("[%s:%d] DTSX hp loudness %sable success", __func__, __LINE__, enable ? "en" : "dis");
    }
    return ret;
}

int dtsx_spk_loudness_target(int target)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (target < DTSX_LOUDNESS_TARGET_VALUE_MIN || target > DTSX_LOUDNESS_TARGET_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set spk loudness target (%d) failed", __func__, __LINE__, target);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_spk_loudnesstarget=%d", target);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.loudness_target[DTSX_OUTPUT_SPK] = target;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set spk loudness target (%d) failed", __func__, __LINE__, target);
    } else {
        ALOGI("[%s:%d] DTSX set spk loudness target (%d) success", __func__, __LINE__, target);
    }
    return ret;
}

int dtsx_transcoder_loudness_target(int target)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (target < DTSX_LOUDNESS_TARGET_VALUE_MIN || target > DTSX_LOUDNESS_TARGET_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set transcoder loudness target (%d) failed", __func__, __LINE__, target);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_transcoder_loudnesstarget=%d", target);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.loudness_target[DTSX_OUTPUT_ARC] = target;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set transcoder loudness target (%d) failed", __func__, __LINE__, target);
    } else {
        ALOGI("[%s:%d] DTSX set transcoder loudness target (%d) success", __func__, __LINE__, target);
    }
    return ret;
}

int dtsx_hp_loudness_target(int target)
{
    int ret = -1;
    char cmd[DTSX_PARAM_STRING_LEN] = {0};

    if (target < DTSX_LOUDNESS_TARGET_VALUE_MIN || target > DTSX_LOUDNESS_TARGET_VALUE_MAX) {
        ALOGW("[%s:%d] DTSX set hp loudness target (%d) failed", __func__, __LINE__, target);
        return ret;
    }

    if (_dtsx_dec) {
        snprintf(cmd, DTSX_PARAM_STRING_LEN, "dtsx_hp_loudnesstarget=%d", target);
        ret = dtsx_set_postprocess_dynamic_parameter(cmd);
    } else {
        ret = 0;
    }
    _dtsx_config_params.loudness_target[DTSX_OUTPUT_HP] = target;

    if (ret != 0) {
        ALOGW("[%s:%d] DTSX set hp loudness target (%d) failed", __func__, __LINE__, target);
    } else {
        ALOGI("[%s:%d] DTSX set hp loudness target (%d) success", __func__, __LINE__, target);
    }
    return ret;
}

void dtsx_set_sink_dev_type(int sink_dev_type)
{
    _dtsx_config_params.dec_sink_dev_type = sink_dev_type;
    _dtsx_config_params.pp_sink_dev_type  = sink_dev_type;
    ALOGI("[%s:%d] DTSX set sink dev type (%x)", __func__, __LINE__, sink_dev_type);
}

aml_dec_func_t aml_dtsx_func = {
    .f_init                 = dtsx_decoder_init_patch,
    .f_release              = dtsx_decoder_release_patch,
    .f_process              = dtsx_decoder_process_patch,
    .f_config               = NULL,
    .f_info                 = NULL,
};
