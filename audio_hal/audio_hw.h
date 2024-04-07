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

#ifndef _AUDIO_HW_H_
#define _AUDIO_HW_H_

#include <audio_utils/resampler.h>
#include <hardware/audio.h>
#include <cutils/list.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>

/* ALSA cards for AML */
#define CARD_AMLOGIC_BOARD 0
#define CARD_AMLOGIC_DEFAULT CARD_AMLOGIC_BOARD
/* ALSA ports for AML */
#if (ENABLE_HUITONG == 0)
#define PORT_MM 1
#endif

#define ADD_AUDIO_DELAY_INTERFACE
#include "audio_buffer.h"
#include "audio_hwsync.h"
#include "audio_post_process.h"
#include "aml_hw_mixer.h"
#include "audio_eq_drc_compensation.h"
#include "aml_DRC_param_gen.h"
#include "aml_EQ_param_gen.h"
#include "aml_audio_types_def.h"
#include "aml_alsa_mixer.h"
#include "aml_audio_ms12.h"
#include "audio_port.h"
#include "aml_audio_ease.h"
#include "aml_malloc_debug.h"
#include "audio_hdmi_util.h"
#include "aml_audio_speed_manager.h"
#include "aml_audio_aec.h"

#ifdef ADD_AUDIO_DELAY_INTERFACE
#include "aml_audio_delay.h"
#endif

#ifdef USE_MSYNC
#include <aml_avsync.h>
#include <aml_avsync_log.h>
#else
#include "aml_avsync_stub.h"
#endif

#include "aml_audio_resample_manager.h"
#include "aml_audio_resampler.h"
#include "aml_dec_api.h"
#include "aml_dtshd_dec_api.h"
#include "aml_dtsx_dec_api.h"
#include "audio_format_parse.h"
#include "aml_audio_heaacparser.h"
#include "hal_scaletempo.h"
#include "hal_clipmeta.h"

/* number of frames per period */
/*
 * change DEFAULT_PERIOD_SIZE from 1024 to 512 for passing CTS
 * test case test4_1MeasurePeakRms(android.media.cts.VisualizerTest)
 */
#define DEFAULT_PLAYBACK_PERIOD_SIZE 512//1024
#define DEFAULT_CAPTURE_PERIOD_SIZE  1024
#define DEFAULT_PLAYBACK_PERIOD_CNT 6

#define LOW_LATENCY_PLAYBACK_PERIOD_SIZE 256
#define LOW_LATENCY_CAPTURE_PERIOD_SIZE  512

/* number of ICE61937 format frames per period */
#define DEFAULT_IEC_SIZE 6144
#define PATCH_PERIOD_COUNT  4

/* minimum sleep time in out_write() when write threshold is not reached */
#define MIN_WRITE_SLEEP_US 5000

#define RESAMPLER_BUFFER_FRAMES (DEFAULT_PLAYBACK_PERIOD_SIZE * 6)
#define RESAMPLER_BUFFER_SIZE (4 * RESAMPLER_BUFFER_FRAMES)

static unsigned int DEFAULT_OUT_SAMPLING_RATE = 48000;

/* sampling rate when using MM low power port */
#define MM_LOW_POWER_SAMPLING_RATE 44100
/* sampling rate when using MM full power port */
#define MM_FULL_POWER_SAMPLING_RATE 48000
/* sampling rate when using VX port for narrow band */
#define VX_NB_SAMPLING_RATE 8000
/* sampling rate when using AEC for capture */
#define AEC_CAP_SAMPLING_RATE 16000

#define AUDIO_PARAMETER_STREAM_EQ "audioeffect_eq"
#define AUDIO_PARAMETER_STREAM_SRS "audioeffect_srs_param"
#define AUDIO_PARAMETER_STREAM_SRS_GAIN "audioeffect_srs_gain"
#define AUDIO_PARAMETER_STREAM_SRS_SWITCH "audioeffect_srs_switch"

/* Get a new HW synchronization source identifier.
 * Return a valid source (positive integer) or AUDIO_HW_SYNC_INVALID if an error occurs
 * or no HW sync is available. */
#define AUDIO_PARAMETER_HW_AV_SYNC "hw_av_sync"

#define AUDIO_PARAMETER_HW_AV_EAC3_SYNC "HwAvSyncEAC3Supported"

#define SYS_NODE_EARC           "/sys/class/extcon/earcrx/state"

#define DDP_FRAME_SIZE      (768)
#define EAC3_MULTIPLIER     (4) //EAC3 bitstream in IEC61937
#define HBR_MULTIPLIER      (16) //MAT or DTSHD bitstream in IEC61937
#define JITTER_DURATION_MS  (3)
#define FLOAT_ZERO              (0.00002)   /* the APM mute volume is 0.00001, less than 0.00002 we think is mute. */
#define TV_SPEAKER_OUTPUT_CH_NUM    10

struct renderpts_item {
    struct listnode list;
    uint64_t cur_outapts;
    uint64_t out_start_apts;
};

struct aml_pts_handle {
    struct listnode frame_list;
    pthread_mutex_t list_lock;
    int64_t prepts;
    int64_t gapts;
    int64_t ms12_pcmoutstep_count;
    int32_t ms12_pcmoutstep_val;
    int32_t ptsnum;
};


/*the same as "AUDIO HAL FORMAT" in kernel*/
enum audio_hal_format {
    TYPE_PCM = 0,
    TYPE_DTS_EXPRESS = 1,
    TYPE_AC3 = 2,
    TYPE_DTS = 3,
    TYPE_EAC3 = 4,
    TYPE_DTS_HD = 5 ,
    TYPE_MULTI_PCM = 6,
    TYPE_TRUE_HD = 7,
    TYPE_DTS_HD_MA = 8,//should not used after we unify DTS-HD&DTS-HD MA
    TYPE_PCM_HIGH_SR = 9,
    TYPE_AC4 = 10,
    TYPE_MAT = 11,
    TYPE_DDP_ATMOS = 12,
    TYPE_TRUE_HD_ATMOS = 13,
    TYPE_MAT_ATMOS = 14,
    TYPE_AC4_ATMOS = 15,
    TYPE_DTS_HP = 16,
    TYPE_DDP_ATMOS_PROMPT_ON_ATMOS = 17,
    TYPE_TRUE_HD_ATMOS_PROMPT_ON_ATMOS = 18,
    TYPE_MAT_ATMOS_PROMPT_ON_ATMOS = 19,
    TYPE_AC4_ATMOS_PROMPT_ON_ATMOS = 20,
    TYPE_AAC  = 21,
    TYPE_HEAAC = 22,
    TYPE_DTSX = 23,
};

#define FRAMESIZE_16BIT_STEREO 4
#define FRAMESIZE_32BIT_STEREO 8
#define FRAMESIZE_32BIT_3ch 12
#define FRAMESIZE_32BIT_5ch 20
#define FRAMESIZE_32BIT_8ch 32

/* copy from VTS */
/* hardware/interfaces/audio/core/all-versions/default/include/core/default/Util.h */
/*
    0            ->         Result::OK
    -EINVAL      ->         Result::INVALID_ARGUMENTS
    -ENODATA     ->         Result::INVALID_STATE
    -ENODEV      ->         Result::NOT_INITIALIZED
    -ENOSYS      ->         Result::NOT_SUPPORTED
*/
enum Result {
    OK,
    NOT_INITIALIZED,
    INVALID_ARGUMENTS,
    INVALID_STATE,
    NOT_SUPPORTED,
    RESULT_TOO_BIG
};

#define AML_HAL_MIXER_BUF_SIZE  64*1024

#define SYSTEM_APP_SOUND_MIXING_ON 1
#define SYSTEM_APP_SOUND_MIXING_OFF 0
struct aml_hal_mixer {
    unsigned char start_buf[AML_HAL_MIXER_BUF_SIZE];
    unsigned int wp;
    unsigned int rp;
    unsigned int buf_size;
    /* flag to check if need cache some data before write to mix */
    unsigned char need_cache_flag;
    pthread_mutex_t lock;
};

struct drc_data {
    uint32_t band_id;
    uint32_t attrack_time;
    uint32_t release_time;
    float threshold;
    unsigned int fc;
    unsigned int estimate_time;
    float K;
    unsigned int delays;
};

struct eq_data {
    double G;
    double Q;
    unsigned int fc;
    unsigned int type;
    unsigned int band_id;
};

enum patch_src_assortion {
    SRC_DTV                     = 0,
    SRC_ATV                     = 1,
    SRC_LINEIN                  = 2,
    SRC_HDMIIN                  = 3,
    SRC_SPDIFIN                 = 4,
    SRC_REMOTE_SUBMIXIN         = 5,
    SRC_WIRED_HEADSETIN         = 6,
    SRC_BUILTIN_MIC             = 7,
    SRC_ECHO_REFERENCE          = 8,
    SRC_ARCIN                   = 9,
    SRC_OTHER                   = 10,
    SRC_INVAL                   = 11,
};

typedef enum {
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_UNKNOWN = 0,
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_C,/**< Center */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_MONO = TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_C,
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_R,/**< Left and right speakers */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_STEREO = TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_R,
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R,/**< Left, center and right speakers */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_R_S,/**< Left, right and surround speakers */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_S,/**< Left,center right and surround speakers */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_R_SL_RS,/**< Left, right, surround left and surround right */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_SL_SR,/**< Left, center, right, surround left and surround right */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_SL_SR_LFE,/**< Left, center, right, surround left, surround right and lfe*/
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_5_1 = TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_SL_SR_LFE,
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_SL_SR_RL_RR_LFE, /**< Left, center, right, surround left, surround right, rear left, rear right and lfe */
    TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_7_1 = TIF_HAL_PLAYBACK_AUDIO_SOURCE_CHANNEL_CONFIGURATION_L_C_R_SL_SR_RL_RR_LFE
} TIF_HAL_Playback_AudioSourceChannelConfiguration_t;
enum OUT_PORT {
    OUTPORT_SPEAKER             = 0,
    OUTPORT_HDMI_ARC            = 1,
    OUTPORT_HDMI                = 2,
    OUTPORT_SPDIF               = 3,
    OUTPORT_AUX_LINE            = 4,
    OUTPORT_HEADPHONE           = 5,
    OUTPORT_REMOTE_SUBMIX       = 6,
    OUTPORT_A2DP                = 7,
    OUTPORT_BT_SCO              = 8,
    OUTPORT_BT_SCO_HEADSET      = 9,
    OUTPORT_MAX                 = 10,
};

enum IN_PORT {
    INPORT_TUNER                = 0,
    INPORT_HDMIIN               = 1,
    INPORT_SPDIF                = 2,
    INPORT_LINEIN               = 3,
    INPORT_REMOTE_SUBMIXIN      = 4,
    INPORT_WIRED_HEADSETIN      = 5,
    INPORT_BUILTIN_MIC          = 6,
    INPORT_ECHO_REFERENCE       = 7,
    INPORT_ARCIN                = 8,
    INPORT_DTV                  = 9,
    INPORT_ATV                  = 10,
    INPORT_MEDIA                = 11,
    INPORT_MAX                  = 13,
};

struct audio_patch_set {
    struct listnode list;
    struct audio_patch audio_patch;
};

typedef enum stream_usecase {
    STREAM_PCM_NORMAL       = 0,
    STREAM_PCM_DIRECT       = 1,
    STREAM_PCM_HWSYNC       = 2,
    STREAM_RAW_DIRECT       = 3,
    STREAM_RAW_HWSYNC       = 4,
    STREAM_PCM_PATCH        = 5,
    STREAM_RAW_PATCH        = 6,
    STREAM_PCM_MMAP         = 7,
    STREAM_USECASE_EXT1  = 8,
    STREAM_USECASE_EXT2  = 9,
    STREAM_USECASE_EXT3  = 10,
    STREAM_USECASE_EXT4  = 11,
    STREAM_USECASE_EXT5  = 12,

    STREAM_USECASE_MAX      = 13,
} stream_usecase_t;

typedef enum alsa_device {
    I2S_DEVICE = 0,
    DIGITAL_DEVICE, /*for spdifa*/
    TDM_DEVICE,
    EARC_DEVICE,
	DIGITAL_DEVICE2, /*for spdifb*/
    ALSA_DEVICE_CNT
} alsa_device_t;

enum stream_status {
    STREAM_STANDBY = 0,
    STREAM_HW_WRITING,
    STREAM_MIXING,
    STREAM_PAUSED
};

#if defined(IS_ATOM_PROJECT)
typedef enum atom_stream_type {
    STREAM_ANDROID = 0,
    STREAM_HDMI,
    STREAM_OPTAUX
} atom_stream_type_t;
#endif

/* Base on user settings */
typedef enum picture_mode {
    PQ_STANDARD = 0,
    PQ_MOVIE,
    PQ_DYNAMIC,
    PQ_NATURAL,
    PQ_GAME,
    PQ_PC,
    PQ_CUSTOM,
    PQ_MODE_MAX
} picture_mode_t ;

typedef union {
    unsigned long long timeStamp;
    unsigned char tsB[8];
} aec_timestamp;

struct aml_audio_mixer;

typedef struct audio_hal_info{
    audio_format_t format;
    bool is_dolby_atmos;
    bool is_decoding; /*store the decoder status*/
    bool first_decoding_frame;
    int update_type;
    int update_cnt;
    int channel_number;
    int sample_rate;
    int lfepresent;
    int channel_mode;
    int digital_audio_info;
} audio_hal_info_t;

struct aml_bt_output {
    bool active;
    struct pcm *pcm_bt;
    char *bt_out_buffer;
    size_t bt_out_frames;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    int16_t *resampler_buffer;
    size_t resampler_buffer_size_in_frames;
    size_t resampler_in_frames;
};

#define HDMI_ARC_MAX_FORMAT  20

enum {
    EASE_SETTING_INIT    = 0,
    EASE_SETTING_START   = 1,
    EASE_SETTING_PENDING = 2
};

enum {
    GAP_PASSTHROUGH_STATE_IDLE = 0,
    GAP_PASSTHROUGH_STATE_SET,
    GAP_PASSTHROUGH_STATE_WAIT_START,
    GAP_PASSTHROUGH_STATE_INSERT,
    GAP_PASSTHROUGH_STATE_DONE,
};

struct aml_audio_device {
    struct audio_hw_device hw_device;
    /* see note below on mutex acquisition order */
    pthread_mutex_t lock;
    pthread_mutex_t pcm_write_lock;
    /*
    if dolby ms12 is enabled, the ms12 thread will change the
    stream information depending on the main input format.
    we need protect the risk and this case.
    */
    pthread_mutex_t trans_lock;
    int mode;
    audio_devices_t in_device;
    audio_devices_t out_device;
    int in_call;
    struct aml_stream_in *active_input;
    bool mic_mute;
    bool speaker_mute;
    unsigned int card;
    struct audio_route *ar;
    struct echo_reference_itfe *echo_reference;
    bool low_power;
    struct aml_hal_mixer hal_mixer;
    struct pcm *pcm;
    struct aml_bt_output bt_output;
    bool pcm_paused;
    unsigned hdmi_arc_ad[HDMI_ARC_MAX_FORMAT];
    bool hi_pcm_mode;
    bool audio_patching;
    /* audio configuration for dolby HDMI/SPDIF output */
    int hdmi_format;
    int pre_hdmi_format;
    int spdif_format;
    bool spdif_enable;
    int hdmi_is_pth_active;
    int disable_pcm_mixing;
    /* mute/unmute for vchip  lock control */
    bool parental_control_av_mute;
    int routing;
    struct audio_config output_config;
    /* The HDMI ARC capability info currently set. */
    struct aml_arc_hdmi_desc hdmi_descs;
    /* Save the HDMI ARC actual capability info. */
    struct aml_arc_hdmi_desc hdmi_arc_capability_desc;
    /* HDMIRX default EDID */
    char default_EDID_array[EDID_ARRAY_MAX_LEN];
    /*it is used to save the string of last set_arc_hdmi and to check whether ARC or EARC status has changed*/
    char last_arc_hdmi_array[EDID_ARRAY_MAX_LEN];
    bool need_to_update_arc_status;
    int arc_hdmi_updated;
    int a2dp_updated;
    bool need_reset_a2dp;
    void * a2dp_hal;
    int hdmi_format_updated;
    struct aml_native_postprocess native_postprocess;
    /* used only for real TV source */
    enum patch_src_assortion patch_src;
    /* for port config infos */
    float sink_gain[OUTPORT_MAX];
    float speaker_volume;
    enum OUT_PORT active_outport;
    float src_gain[INPORT_MAX];
    enum IN_PORT active_inport;
    /* message to handle usecase changes */
    bool usecase_changed;
    struct aml_stream_out *active_outputs[STREAM_USECASE_MAX];
    pthread_mutex_t active_outputs_lock;
    pthread_mutex_t patch_lock;
    pthread_mutex_t dtv_patch_lock;//this only use for locking adev->audio_patch in dtv source,
                                    //since ms12_output will use adev->audio_patch to do media sync
    struct aml_audio_patch *audio_patch;
    /* indicates atv to mixer patch, no need HAL patching  */
    bool tuner2mix_patch;
    /* Now only two pcm handle supported: I2S, SPDIF */
    pthread_mutex_t alsa_pcm_lock;
    struct pcm *pcm_handle[ALSA_DEVICE_CNT];
    int pcm_refs[ALSA_DEVICE_CNT];
    bool is_paused[ALSA_DEVICE_CNT];
    struct aml_hw_mixer hw_mixer;
    audio_format_t sink_format;
    unsigned int sink_max_channels;
    bool sink_allow_max_channel;
    audio_format_t optical_format;
    audio_format_t sink_capability;
    audio_format_t last_sink_capability;
    volatile int32_t next_unique_ID;
    /* list head for audio_patch */
    struct listnode patch_list;

    bool dual_spdifenc_inited;
    /* Dolby MS12 lib variable start */
    struct dolby_ms12_desc ms12;
    bool dolby_ms12_need_recovery;
    struct pcm_config ms12_config;
    int mixing_level;
    int advol_level;
    uint8_t ad_fade;
    uint8_t ad_pan;
    bool associate_audio_mixing_enable;
    uint64_t a2dp_no_reconfig_ms12;
    /* Dolby MS12 lib variable end */

    /**
     * enum eDolbyLibType
     * DolbyDcvLib  = dcv dec lib   , libHwAudio_dcvdec.so
     * DolbyMS12Lib = dolby MS12 lib, libdolbyms12.so
     */
    int dolby_lib_type;
    int dolby_lib_type_last;
    int dolby_decode_enable;   /*it can decode dolby, not passthrough lib*/
    int dts_lib_type;
    int dts_decode_enable;

    /*used for dts decoder*/
    struct dca_dts_dec dts_hd;
    bool bHDMIARCon;
    bool bHDMIConnected;
    bool bHDMIConnected_update;

    /**
     * buffer pointer whose data output to headphone
     * buffer size equal to efect_buf_size
     */
    void *spk_output_buf;
    void *hp_output_buf;
    void *spdif_output_buf;
    void *effect_buf;
    size_t effect_buf_size;
    size_t spk_tuning_lvl;
    /* ringbuffer for tuning latency total buf size */
    size_t spk_tuning_buf_size;
    ring_buffer_t spk_tuning_rbuf;
    bool mix_init_flag;
    struct eq_drc_data eq_data;
    struct drc_data Drc_data;
    struct eq_data Eq_data;
    /*used for high pricision A/V from amlogic amadec decoder*/
    unsigned first_apts;
    /*
    first apts flag for alsa hardware prepare,true,need set apts to hw.
    by default it is false as we do not need set the first apts in normal use case.
    */
    bool first_apts_flag;
    size_t frame_trigger_thred;
    void *dev_to_mix_parser;
    int continuous_audio_mode;
    int continuous_audio_mode_default;
    int delay_disable_continuous;
    bool atoms_lock_flag;
    bool need_remove_conti_mode;
    int  exiting_ms12;
    bool doing_reinit_ms12;    /*we are doing reinit ms12*/
    bool doing_cleanup_ms12;   /*we are doing cleanup ms12*/
    bool ms12_to_be_cleanup;
    struct timespec ms12_exiting_start;
    int debug_flag;
    int dcvlib_bypass_enable;
    int dtslib_bypass_enable;
    float dts_post_gain;
    bool spdif_encoder_init_flag;
    /*atsc has video in program*/
    bool is_has_video;
    struct aml_stream_out *ms12_out;
    int spdif_fmt_hw;
    /*amlogic soft ware noise gate fot analog TV source*/
    void* aml_ng_handle;
    int aml_ng_enable;
    float aml_ng_level;
    int source_mute;
    int aml_ng_attack_time;
    int aml_ng_release_time;
    int system_app_mixing_status;
    int audio_type;
    struct aml_mixer_handle alsa_mixer;
    struct amlAudioMixer *audio_mixer;
    bool is_TV;
    bool is_STB;
    bool is_SBR;
    bool useSubMix;
    //int cnt_stream_using_mixer;
    int tsync_fd;
    bool raw_to_pcm_flag;
    bool is_netflix;
    int dtv_aformat;
    unsigned int dtv_i2s_clock;
    unsigned int dtv_spidif_clock;
    unsigned int dtv_droppcm_size;
    int need_reset_ringbuffer;
    unsigned int tv_mute;
    int sub_apid;
    int sub_afmt;
    int pid;
    int demux_id;
    int is_multi_demux;
    bool compensate_video_enable;
    bool patch_start;
    bool mute_start;
    aml_audio_ease_t  *audio_ease;
    /*four variable used for when audio discontinue and underrun,
      whether mute output*/
    int discontinue_mute_flag;
    int audio_discontinue;
    int no_underrun_count;
    int no_underrun_max;
    int dap_bypass_enable;
    float dap_bypassgain;
    int start_mute_flag;
    int start_mute_count;
    int start_mute_max;
    int underrun_mute_flag;
    int ad_start_enable;
    int count;
    int sound_track_mode;
    void *alsa_handle[ALSA_DEVICE_CNT];
    int FactoryChannelReverse;
    bool dual_spdif_support; /*1 means supports spdif_a & spdif_b & spdif interface*/
    bool ms12_force_ddp_out; /*1 force ms12 output ddp*/

    /* user setting picture mode */
    picture_mode_t pic_mode;
    bool game_mode;
    bool mode_reconfig_in;
    bool mode_reconfig_out;
    bool mode_reconfig_ms12;
    /* user setting picture mode end */

    uint64_t  sys_audio_frame_written;
    void* hw_mediasync;
    struct aec_t *aec;
    struct aec_context *aml_aec;
    bool bt_wbs;
    int security_mem_level;
    int dolby_ms12_dap_init_mode;
    void *aml_dtv_audio_instances;
    pthread_mutex_t dtv_lock;
    /* display audio format on UI, both streaming and hdmiin*/
    audio_hal_info_t audio_hal_info;
    bool is_ms12_tuning_dat; /* a flag to determine the MS12 tuning data file is existing */
    /*
    defined for default speaker output channels:
    stb: default 2 channels.
    tv:  default 8 channels(2ch speaker,2ch spdif,2ch headphone)
    soundbar:depending on the prop defined by device
    */
    int  default_alsa_ch;

    uint64_t gap_offset;
    uint32_t gap_pts;
    bool gap_ignore_pts;
    int gap_passthrough_state;
    uint32_t gap_passthrough_ms12_no_output_counter;
    bool arc_connected_reconfig;  /*when arc connected, set it as to true*/
    float master_volume;
    bool master_mute;
    int syss_mixgain;
    int apps_mixgain;

    int vol_ease_setting_state;
    int vol_ease_setting_gain;
    int vol_ease_setting_duration;
    int vol_ease_setting_shape;
    float record_volume_before_mute;//Record the master volume before mute
#ifndef NO_AUDIO_CAP
    /* CapTure IpcBuffer */
    pthread_mutex_t cap_buffer_lock;
    void *cap_buffer;
    int cap_delay;
#endif
    int synctype; //tsplayer TS mode input set
    int injection_enable;

#ifdef USE_MEDIAINFO
    void *minfo_h;
    uint32_t info_rid;
    int minfo_atmos_flag;
#endif
    bool audio_focus_enable;
    uint32_t decoder_drc_control;
    uint32_t dap_drc_control;
    uint32_t downmix_type;

    /* board specific json configs */
    int hdmitx_src; /* HDMITX src select for TDM */
    bool spdif_independent;  /*spdif output can be independent with HDMI output*/
    aml_dec_info_t dec_stream_info;
    /* -End- */

    bool vx_enable;
    bool user_setting_scaletempo;

    bool is_hdmi_arc_interact_done;
};

struct meta_data {
    uint32_t frame_size;
    uint64_t pts;
    uint64_t payload_offset;
};

struct meta_data_list {
    struct listnode list;
    struct meta_data mdata;
};

typedef enum {
    AUDIO_PARAMTYPE_DEC_STATUS = 0,
    AUDIO_PARAMTYPE_PTS,
    AUDIO_PARAMTYPE_SAMPLE_RATE,
    AUDIO_PARAMTYPE_CHANNEL_MODE,
    AUDIO_PARAMTYPE_DIGITAL_AUDIO_INFO,
} audio_paramtype;

typedef enum audio_data_handle_state {
    AUDIO_DATA_HANDLE_NONE = 0,
    AUDIO_DATA_HANDLE_START,
    AUDIO_DATA_HANDLE_DETECT,
    AUDIO_DATA_HANDLE_DETECTED,
    AUDIO_DATA_HANDLE_EASE_CONFIG,
    AUDIO_DATA_HANDLE_EASING,
    AUDIO_DATA_HANDLE_FINISHED,

    AUDIO_DATA_HANDLE_MAX
} audio_data_handle_state_t;

enum {
    AVSYNC_TYPE_NULL    = 0,
    AVSYNC_TYPE_TSYNC   = 1,
    AVSYNC_TYPE_MSYNC   = 2,
    AVSYNC_TYPE_MEDIASYNC   = 3,
};

struct aml_stream_out {
    struct audio_stream_out stream;
    /* see note below on mutex acquisition order */
    pthread_mutex_t lock;
    struct audio_config audioCfg;
    /* config which set to ALSA device */
    struct pcm_config config;
    audio_format_t    alsa_output_format;
    /* channel mask exposed to AudioFlinger. */
    audio_channel_mask_t hal_channel_mask;
    /* format mask exposed to AudioFlinger. */
    audio_format_t hal_format;
    /* samplerate exposed to AudioFlinger. */
    unsigned int hal_rate;
    unsigned int hal_ch;
    unsigned int hal_frame_size;
    unsigned int hal_decode_block_size;
    unsigned int rate_convert;
    audio_output_flags_t flags;
    audio_devices_t out_device;
    struct pcm *pcm;
    struct resampler_itfe *resampler;
    char *buffer;
    size_t buffer_frames;
    bool standby;
    struct aml_audio_device *dev;
    int write_threshold;
    bool low_power;
    unsigned multich;
    int codec_type;
    uint64_t frame_write_sum;
    uint64_t frame_skip_sum;
    uint64_t last_frames_position;
    uint64_t spdif_enc_init_frame_write_sum;
    int skip_frame;
    int32_t *tmp_buffer_8ch;
    size_t tmp_buffer_8ch_size;
    int is_tv_platform;
    void *audioeffect_tmp_buffer;
    bool pause_status;
    bool need_sync;
    bool with_header;
    float volume_l_org;
    float volume_r_org;
    float volume_l;
    float volume_r;
    float last_volume_l;
    float last_volume_r;
    bool ms12_vol_ctrl;
    int last_codec_type;
    /**
     * as raw audio framesize  is 1 computed by audio_stream_out_frame_size
     * we need divide more when we got 61937 audio package
     */
    int raw_61937_frame_size;
    /* recorded for wraparound print info */
    unsigned last_dsp_frame;
    audio_header_t *pheader;
    struct timespec timestamp;
    struct timespec lasttimestamp;
    stream_usecase_t usecase;
    //uint32_t dev_usecase_masks;
    /**
     * flag indicates that this stream need do mixing
     * int is_in_mixing: 1;
     * Normal pcm may not hold alsa pcm device.
     */
    int is_normal_pcm;
    unsigned int card;
    alsa_device_t device;
    ssize_t (*write)(struct audio_stream_out *stream, struct audio_buffer *abuffer);
    enum stream_status status;
    audio_format_t hal_internal_format;
    bool dual_output_flag;
    uint64_t input_bytes_size;
    uint64_t continuous_audio_offset;
    bool direct_raw_config;
    bool is_device_differ_with_ms12;
    uint64_t total_write_size;
    uint64_t payload_offset;
    int  ddp_frame_size;
    int dropped_size;
    unsigned long long mute_bytes;
    bool is_get_mute_bytes;
    bool normal_pcm_mixing_config;
    uint32_t latency_frames;
    unsigned int inputPortID;
    int inputPortType;
    int exiting;
    pthread_mutex_t cond_lock;
    pthread_cond_t cond;
    struct hw_avsync_header_extractor *hwsync_extractor;
    struct listnode mdata_list;
    pthread_mutex_t mdata_lock;
    bool first_pts_set;
    bool need_first_sync;
    uint64_t last_pts;
    uint64_t last_payload_offset;
    struct audio_config out_cfg;
    int debug_stream;
    uint64_t us_used_last_write;
    bool offload_mute;
    bool need_convert;
    size_t last_playload_used;
    int ddp_frame_nblks;
    uint64_t total_ddp_frame_nblks;
    int framevalid_flag;
    bool bypass_submix;
    int need_drop_size;
    int position_update;
    bool spdifenc_init;
    void *spdifenc_handle;
    bool dual_spdif;
    int codec_type2;  /*used for dual bitstream output*/
    struct pcm *pcm2; /*used for dual bitstream output*/
    int pcm2_mute_cnt;
    bool tv_src_stream;
    bool is_ms12_stream;
    unsigned int write_func;
    uint64_t  last_frame_reported;
    struct timespec  last_timestamp_reported;
    void    *pstMmapAudioParam;    // aml_mmap_audio_param_st (aml_mmap_audio.h)
    struct resample_para aml_resample;
    unsigned char *resample_outbuf;
    bool restore_hdmitx_selection;
    bool restore_continuous;
    bool restore_dolby_lib_type;
    bool continuous_mode_check;
    void * spdif_dec_handle;
    void * ac3_parser_handle;
    void * ac4_parser_handle;
    void * heaac_parser_handle;
    struct heaac_parser_info heaac_info;
    int64_t last_mmap_nano_second;
    int32_t last_mmap_position;
    uint64_t main_input_ns;
    bool is_sink_format_prepared;
    bool is_ms12_main_decoder;
    aml_dec_config_t  dec_config;               /*store the decode config*/
    aml_dec_t *aml_dec;                         /*store the decoder handle*/
    int ad_substream_supported;
    aml_audio_resample_t *resample_handle;
    aml_audio_speed_t *speed_handle;

    /*spdif output related info start*/
    audio_format_t optical_format;
    //audio_format_t spdif_audio_format;
    void *spdifout_handle;
    //audio_format_t spdif2_audio_format;
    void *spdifout2_handle;
    /*spdif output related info end*/
    void * virtual_buf_handle;
    bool is_add2active_output;
    uint32_t alsa_write_cnt;
    uint64_t alsa_write_frames;
    aml_audio_ease_t  *audio_stream_ease;
    audio_data_handle_state_t audio_data_handle_state;
    uint16_t easing_time;
    float output_speed;
    uint64_t write_time;
    uint64_t pause_time;
    int write_count;
    int avsync_type;
    avsync_ctx_t *avsync_ctx;
    bool is_dtscd;
    bool dts_check;
    bool alsa_running_status;
    bool alsa_status_changed;
    /*flag indicate the ms12 2ch lock is on*/
    bool ms12_acmod2ch_lock_disable;
    bool frame_write_sum_updated;
    uint32_t timer_id;
    uint64_t ms12_main_input_size;
    uint64_t dolby_ms12_input_main_sum;
    uint64_t parse_consumed_size;

    //DTV sync
    bool set_init_avsync_policy;

    /* llp */
    void *llp_buf;
#if 0
    bool llp_input_thread_created;
    bool llp_input_thread_exit;
    pthread_t llp_input_threadID;
#endif
    uint64_t llp_rp;
    uint64_t llp_underrun_wp;
    bool enable_scaletempo;
    struct scale_tempo * scaletempo;
    bool b_install_sync_callback;
    bool will_pause;
    int fast_quit;
    bool eos;

    //partial audio frame
    clip_meta_t *clip_meta;
    uint64_t clip_offset;
    /* for dcvlib clip only start */
    uint64_t dec_cachedsize;
    uint64_t consumedsize;
    /* for dcvlib clip only end */
};

typedef ssize_t (*write_func)(struct audio_stream_out *stream, const void *buffer, size_t bytes);

#define MAX_PREPROCESSORS 3 /* maximum one AGC + one NS + one AEC per input stream */
struct aml_stream_in {
    struct audio_stream_in stream;
#if defined(ENABLE_HBG_PATCH)
    int hbg_channel;
#endif
    pthread_mutex_t lock;       /* see note below on mutex acquisition order */
    struct pcm_config config;
    struct pcm *pcm;
    unsigned int device;
    audio_channel_mask_t hal_channel_mask;
    audio_format_t hal_format;
    struct resampler_itfe *resampler;
    struct resampler_buffer_provider buf_provider;
    int16_t *buffer;
    size_t frames_in;
    unsigned int requested_rate;
    uint32_t main_channels;
    bool standby;
    int source;
    struct echo_reference_itfe *echo_reference;
    bool need_echo_reference;
    effect_handle_t preprocessors[MAX_PREPROCESSORS];
    int num_preprocessors;
    int16_t *proc_buf;
    size_t proc_buf_size;
    size_t proc_frames_in;
    int16_t *ref_buf;
    size_t ref_buf_size;
    size_t ref_frames_in;
    int read_status;
    /* HW parser audio format */
    int spdif_fmt_hw;
    /* SW parser audio format */
    audio_format_t spdif_fmt_sw;
    struct timespec mute_start_ts;
    bool mute_flag;
    int mute_log_cntr;
    int mute_mdelay;
    struct aml_audio_device *dev;
    void *input_tmp_buffer;
    size_t input_tmp_buffer_size;
    void *tmp_buffer_8ch;
    size_t tmp_buffer_8ch_size;
    unsigned int frames_read;
    uint64_t timestamp_nsec;
    bool bt_sco_active;
    hdmiin_audio_packet_t audio_packet_type;
    hdmiin_audio_packet_t last_audio_packet_type;
};

typedef enum {
    CALLBACK_SCALE_TEMPO = 0,
    CALLBACK_LLP_BUFFER  = 1,
    CALLBACK_CLIP_META   = 2,
} ms12_callback_type_t;

typedef  int (*do_standby_func)(struct aml_stream_out *out);
typedef  int (*do_startup_func)(struct aml_stream_out *out);

static inline int continuous_mode(struct aml_audio_device *adev)
{
    return adev->continuous_audio_mode;
}
static inline bool direct_continous(struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    if ((out->flags & AUDIO_OUTPUT_FLAG_DIRECT) && adev->continuous_audio_mode) {
        return true;
    } else {
        return false;
    }
}
static inline bool primary_continous(struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    if ((out->flags & AUDIO_OUTPUT_FLAG_PRIMARY) && adev->continuous_audio_mode) {
        return true;
    } else {
        return false;
    }
}
/* called when adev locked */
static inline int dolby_stream_active(struct aml_audio_device *adev)
{
    int i = 0;
    int is_dolby = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && (out->hal_internal_format == AUDIO_FORMAT_AC3
            || out->hal_internal_format == AUDIO_FORMAT_E_AC3
            || out->hal_internal_format == AUDIO_FORMAT_DOLBY_TRUEHD
            || out->hal_internal_format == AUDIO_FORMAT_AC4
            || out->hal_internal_format == AUDIO_FORMAT_MAT
            || out->hal_internal_format == AUDIO_FORMAT_HE_AAC_V1
            || out->hal_internal_format == AUDIO_FORMAT_HE_AAC_V2)) {
            is_dolby = 1;
            break;
        }
    }
    return is_dolby;
}

/* called when adev locked */
static inline int dts_stream_active(struct aml_audio_device *adev)
{
    int i = 0;
    int is_dts = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && (out->hal_internal_format == AUDIO_FORMAT_DTS
            || out->hal_internal_format == AUDIO_FORMAT_DTS_HD)) {
            is_dts = 1;
            break;
        }
    }
    return is_dts;
}


/* called when adev locked */
static inline int hwsync_lpcm_active(struct aml_audio_device *adev)
{
    int i = 0;
    int is_hwsync_lpcm = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && audio_is_linear_pcm(out->hal_internal_format) && (out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC)) {
            is_hwsync_lpcm = 1;
            break;
        }
    }
    return is_hwsync_lpcm;
}

static inline struct aml_stream_out *direct_active(struct aml_audio_device *adev)
{
    int i = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && (out->flags & AUDIO_OUTPUT_FLAG_DIRECT)) {
            return out;
        }
    }
    return NULL;
}

static inline bool is_bypass_submix_active(struct aml_audio_device *adev)
{
    int i = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && (out->bypass_submix)) {
            return true;
        }
    }
    return false;
}

/* called when adev locked */
static inline int get_mmap_pcm_active_count(struct aml_audio_device *adev)
{
    int i = 0;
    int count = 0;
    struct aml_stream_out *out = NULL;
    for (i = 0 ; i < STREAM_USECASE_MAX; i++) {
        out = adev->active_outputs[i];
        if (out && audio_is_linear_pcm(out->hal_internal_format) && (out->flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) {
            count ++;
        }
    }
    return count;
}

/*
 *@brief get_output_format get the output format always return the "sink_format" of adev
 */
audio_format_t get_output_format(struct audio_stream_out *stream);
void *audio_patch_output_threadloop(void *data);

/*
 *@brief audio_hal_data_processing
 * format:
 *    if pcm-16bits-stereo, add audio effect process, and mapping to 8ch
 *    if raw data, packet it to IEC61937 format with spdif encoder
 *    if IEC61937 format, write them to hardware
 * return
 *    0, success
 *    -1, fail
 */
ssize_t audio_hal_data_processing(struct audio_stream_out *stream
                                    , const void *input_buffer
                                    , size_t input_buffer_bytes
                                    , void **output_buffer
                                    , size_t *output_buffer_bytes
                                    , audio_format_t output_format);
/*
 *@brief audio_hal_data_processing_ms12v2
 * format:
 *    if pcm-16bits-8ch, mapping to 8ch
 *    if raw data, packet it to IEC61937 format with spdif encoder
 *    if IEC61937 format, write them to hardware
 * return
 *    0, success
 *    -1, fail
 */
ssize_t audio_hal_data_processing_ms12v2(struct audio_stream_out *stream
                                  , const void *input_buffer
                                  , size_t input_buffer_bytes
                                  , void **output_buffer
                                  , size_t *output_buffer_bytes
                                  , audio_format_t output_format
                                  , int n_ms12_channel);

/*
 *@brief hw_write the api to write the data to audio hardware
 */
ssize_t hw_write(struct audio_stream_out *stream
                    , const void *buffer
                    , size_t bytes
                    , audio_format_t output_format);

int do_output_standby_l(struct audio_stream *stream);
int dsp_process_output(struct aml_audio_device *adev, void *in_buffer,
                       size_t bytes);
int release_patch_l(struct aml_audio_device *adev);
enum hwsync_status check_hwsync_status (uint32_t apts_gap);
int config_output(struct audio_stream_out *stream, bool reset_decoder);
int start_ease_in(struct aml_audio_device *adev);
int start_ease_out(struct aml_audio_device *adev);
int out_standby_direct (struct audio_stream *stream);

void *adev_get_handle();
audio_format_t get_non_ms12_output_format(audio_format_t src_format, struct aml_audio_device *aml_dev);

int start_input_stream(struct aml_stream_in *in);

int do_input_standby (struct aml_stream_in *in);

int adev_ms12_prepare(struct audio_hw_device *dev);

void adev_ms12_cleanup(struct audio_hw_device *dev);

int ms12_clipmeta(void *priv_data, void *info, unsigned long long offset);

/* 'bytes' are the number of bytes written to audio FIFO, for which 'timestamp' is valid.
 * 'available' is the number of frames available to read (for input) or yet to be played
 * (for output) frames in the PCM buffer.
 * timestamp and available are updated by pcm_get_htimestamp(), so they use the same
 * datatypes as the corresponding arguments to that function. */
struct aec_info {
    struct timespec timestamp;
    uint64_t timestamp_usec;
    unsigned int available;
    size_t bytes;
};
/* Capture codec parameters */
/* Set up a capture period of 32 ms:
 * CAPTURE_PERIOD = PERIOD_SIZE / SAMPLE_RATE, so (32e-3) = PERIOD_SIZE / (16e3)
 * => PERIOD_SIZE = 512 frames, where each "frame" consists of 1 sample of every channel (here, 2ch) */
#define CAPTURE_PERIOD_MULTIPLIER 16
#define CAPTURE_PERIOD_SIZE (CODEC_BASE_FRAME_COUNT * CAPTURE_PERIOD_MULTIPLIER)
#define CAPTURE_PERIOD_START_THRESHOLD 4
#define CAPTURE_CODEC_SAMPLING_RATE 16000

#ifdef ENABLE_AEC_HAL
#define NUM_AEC_REFERENCE_CHANNELS 1
#endif
#ifdef ENABLE_AEC_APP
/* App AEC uses 2-channel reference */
#define NUM_AEC_REFERENCE_CHANNELS 2
#endif /* #ifdef ENABLE_AEC_FUNC */
#define CODEC_BASE_FRAME_COUNT 32
#define PLAYBACK_PERIOD_MULTIPLIER 32  /* 21 ms */
#define PLAYBACK_PERIOD_SIZE (CODEC_BASE_FRAME_COUNT * PLAYBACK_PERIOD_MULTIPLIER)
#define CHANNEL_STEREO 2
#define PLAYBACK_CODEC_SAMPLING_RATE 48000
/* Maximum string length in audio hal. */
#define AUDIO_HAL_CHAR_MAX_LEN                          (256)
#endif
