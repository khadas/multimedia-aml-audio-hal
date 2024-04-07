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

#ifndef _DTV_PATCH_HAL_AVSYNC_H_
#define _DTV_PATCH_HAL_AVSYNC_H_

#define PROPERTY_LOCAL_PASSTHROUGH_LATENCY  "vendor.media.dtv.passthrough.latencyms"
#define PROPERTY_PRESET_AC3_PASSTHROUGH_LATENCY  "vendor.media.dtv.passthrough.ac3prelatencyms"
#define PROPERTY_AUDIO_ADJUST_PCR_MAX   "vendor.media.audio.adjust.pcr.max"
#define PROPERTY_UNDERRUN_MUTE_MINTIME     "vendor.media.audio.underrun.mute.mintime"
#define PROPERTY_UNDERRUN_MUTE_MAXTIME     "vendor.media.audio.underrun.mute.maxtime"
#define PROPERTY_UNDERRUN_MAX_TIME      "vendor.media.audio.underruncheck.max.time"
#define PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS "vendor.media.audio.tuning.pcr.clocksteps"
#define PROPERTY_AUDIO_TUNING_CLOCK_FACTOR  "vendor.media.audio.tuning.clock.factor"
#define PROPERTY_DEBUG_TIME_INTERVAL  "vendor.media.audio.debug.timeinterval"


#define DTV_PTS_CORRECTION_THRESHOLD (90000 * 30 / 1000)
#define AUDIO_PTS_DISCONTINUE_THRESHOLD (90000 * 5)
#define DECODER_PTS_MAX_LATENCY (320 * 90)
#define DEMUX_PCR_APTS_LATENCY (300 * 90)
#define DEFAULT_ARC_DELAY_MS (100)
#define DEFAULT_SYSTEM_TIME (90000)
#define TIMEOUT_WAIT_TIME (1000 * 3)

#define DEFAULT_DTV_OUTPUT_CLOCK    (1000*1000)
#define DEFAULT_DTV_ADJUST_CLOCK    (1000)
#define DEFAULT_DTV_MIN_OUT_CLOCK   (1000*1000-100*1000)
#define DEFAULT_DTV_MAX_OUT_CLOCK   (1000*1000+100*1000)
#define DEFAULT_I2S_OUTPUT_CLOCK    (256*48000)
#define DEFAULT_CLOCK_MUL    (4)
#define DEFAULT_SPDIF_PLL_DDP_CLOCK    (256*48000*2)
#define DEFAULT_SPDIF_ADJUST_TIMES    (4)
#define DEFAULT_STRATEGY_ADJUST_CLOCK    (100)
#define DEFAULT_TUNING_PCR_CLOCK_STEPS (256 * 64)
#define DEFAULT_TUNING_CLOCK_FACTOR (7)
#define DEFAULT_AUDIO_DROP_THRESHOLD_MS (60)
#define DEFAULT_AUDIO_LEAST_CACHE_MS (50)
#define AUDIO_PCR_LATENCY_MAX (3000)
#define DEFULT_DEBUG_TIME_INTERVAL (5000)
#define DTV_AUDIO_START_MUTE_MAX_HTRESHOLD    (3 * 1000) //ms
#define DTV_AUDIO_RETUNE_DEFAULT_THRESHOLD    (60)  //ms
#define DTV_AUDIO_DROP_TIMEOUT_THRESHOLD      (5000) //ms
#define DTV_AUDIO_DROP_DEFAULT_THRESHOLD      (200)  //ms
#define DTV_AUDIO_CACHE_LATENCY_THRESHOLD   (1500)  //ms
#define DTV_AUDIO_JUMPED_DEFAULT_THRESHOLD   (400)  //ms
#define DEFAULT_DTV_ADJUST_CLOCK_THRESHOLD   (5) //percent

/* dtv NONMS12 tuning part */
//input format
#define  DTV_AVSYNC_NONMS12_PCM_LATENCY                    (0)
#define  DTV_AVSYNC_NONMS12_DD_LATENCY                     (0)
#define  DTV_AVSYNC_NONMS12_DDP_LATENCY                    (0)
#define  DTV_AVSYNC_NONMS12_PCM_LATENCY_PROPERTY           "vendor.media.audio.hal.nonms12.dtv.pcm"
#define  DTV_AVSYNC_NONMS12_DD_LATENCY_PROPERTY            "vendor.media.audio.hal.nonms12.dtv.dd"
#define  DTV_AVSYNC_NONMS12_DDP_LATENCY_PROPERTY           "vendor.media.audio.hal.nonms12.dtv.ddp"
//port speaker
#define  DTV_AVSYNC_NONMS12_TV_SPEAKER_LATENCY                    (0)
#define  DTV_AVSYNC_NONMS12_STB_SPEAKER_LATENCY                   (60)
#define  DTV_AVSYNC_NONMS12_TV_SPEAKER_LATENCY_PROPERTY           "vendor.media.audio.hal.nonms12.tv.dtv.speaker"
#define  DTV_AVSYNC_NONMS12_STB_SPEAKER_LATENCY_PROPERTY          "vendor.media.audio.hal.nonms12.stb.dtv.speaker"
//port A2DP
#define  DTV_AVSYNC_NONMS12_TV_MIX_A2DP_LATENCY                   (120)
#define  DTV_AVSYNC_NONMS12_TV_MIX_A2DP_LATENCY_PROPERTY          "vendor.media.audio.hal.nonms12.tv.dtv.mix.a2dp"
//port arc
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PCM_LATENCY              (0)
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DD_LATENCY               (60)
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DDP_LATENCY              (90)
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PCM_LATENCY_PROPERTY     "vendor.media.audio.hal.nonms12.dtv.arc.pcm"
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DD_LATENCY_PROPERTY      "vendor.media.audio.hal.nonms12.dtv.arc.dd"
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_DDP_LATENCY_PROPERTY     "vendor.media.audio.hal.nonms12.dtv.arc.ddp"
//passthrough arc
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PT_DD_LATENCY            (0)
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PT_DDP_LATENCY           (0)
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PT_DD_LATENCY_PROPERTY   "vendor.media.audio.hal.nonms12.dtv.pt.arc.dd"
#define  DTV_AVSYNC_NONMS12_HDMI_ARC_OUT_PT_DDP_LATENCY_PROPERTY  "vendor.media.audio.hal.nonms12.dtv.pt.arc.ddp"
//STB_box hdmi out
#define  AVSYNC_NONMS12_DTV_HDMI_OUT_PCM_LATENCY                  (85)
#define  AVSYNC_NONMS12_DTV_HDMI_OUT_DD_LATENCY                   (0)
#define  AVSYNC_NONMS12_DTV_HDMI_OUT_DDP_LATENCY                  (100)
#define  AVSYNC_NONMS12_DTV_HDMI_OUT_PCM_LATENCY_PROPERTY         "vendor.media.audio.hal.nonms12.dtv.hdmi.pcm"
#define  AVSYNC_NONMS12_DTV_HDMI_OUT_DD_LATENCY_PROPERTY          "vendor.media.audio.hal.nonms12.dtv.hdmi.dd"
#define  AVSYNC_NONMS12_DTV_HDMI_OUT_DDP_LATENCY_PROPERTY         "vendor.media.audio.hal.nonms12.dtv.hdmi.ddp"
/* end dtv NONMS12 tuning part */

/* dtv MS12 tuning part */
//input format
#define  DTV_AVSYNC_MS12_PCM_LATENCY                    (0)
#define  DTV_AVSYNC_MS12_DD_LATENCY                     (120)
#define  DTV_AVSYNC_MS12_DDP_LATENCY                    (150)
#define  DTV_AVSYNC_MS12_AC4_LATENCY                    (120)
#define  DTV_AVSYNC_MS12_AAC_LATENCY                    (10)
#define  DTV_AVSYNC_MS12_PCM_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.pcm"
#define  DTV_AVSYNC_MS12_DD_LATENCY_PROPERTY          "vendor.media.audio.hal.ms12.dtv.dd"
#define  DTV_AVSYNC_MS12_DDP_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.ddp"
#define  DTV_AVSYNC_MS12_AC4_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.ac4"
#define  DTV_AVSYNC_MS12_AAC_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.aac"
//port speaker
#define  DTV_AVSYNC_MS12_TV_SPEAKER_LATENCY                  (0)
#define  DTV_AVSYNC_MS12_STB_SPEAKER_LATENCY                 (-55)
#define  DTV_AVSYNC_MS12_TV_SPEAKER_LATENCY_PROPERTY                  "vendor.media.audio.hal.ms12.tv.dtv.speaker"
#define  DTV_AVSYNC_MS12_STB_SPEAKER_LATENCY_PROPERTY                 "vendor.media.audio.hal.ms12.stb.dtv.speaker"
//port A2DP
#define  DTV_AVSYNC_MS12_TV_MIX_A2DP_LATENCY                  (200)
#define  DTV_AVSYNC_MS12_TV_MIX_A2DP_LATENCY_PROPERTY                 "vendor.media.audio.hal.ms12.tv.dtv.mix.a2dp"
//port arc
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY             (0)
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY              (100)
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY             (120)
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_MAT_LATENCY             (120)
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY_PROPERTY            "vendor.media.audio.hal.ms12.dtv.arc.pcm"
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY_PROPERTY             "vendor.media.audio.hal.ms12.dtv.arc.dd"
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY_PROPERTY             "vendor.media.audio.hal.ms12.dtv.arc.ddp"
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_MAT_LATENCY_PROPERTY            "vendor.media.audio.hal.ms12.dtv.arc.mat"
//passthrough arc
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DD_LATENCY            (0)
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DDP_LATENCY           (0)
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DD_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.pt.arc.dd"
#define  DTV_AVSYNC_MS12_HDMI_ARC_OUT_PT_DDP_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.pt.arc.ddp"


//STB_box hdmi out
#define  AVSYNC_MS12_DTV_HDMI_OUT_PCM_LATENCY                (-55) /* if 0,  result locates at [-10, 10] */
#define  AVSYNC_MS12_DTV_HDMI_OUT_DD_LATENCY                 (0)
#define  AVSYNC_MS12_DTV_HDMI_OUT_DDP_LATENCY                (0)
#define  AVSYNC_MS12_DTV_HDMI_OUT_MAT_LATENCY                (-40)

#define  AVSYNC_MS12_DTV_HDMI_OUT_PCM_LATENCY_PROPERTY                "vendor.media.audio.hal.ms12.dtv.hdmi.pcm"
#define  AVSYNC_MS12_DTV_HDMI_OUT_DD_LATENCY_PROPERTY                 "vendor.media.audio.hal.ms12.dtv.hdmi.dd"
#define  AVSYNC_MS12_DTV_HDMI_OUT_DDP_LATENCY_PROPERTY                "vendor.media.audio.hal.ms12.dtv.hdmi.ddp"
#define  AVSYNC_MS12_DTV_HDMI_OUT_MAT_LATENCY_PROPERTY                "vendor.media.audio.hal.ms12.dtv.hdmi.mat"
//STB_box passthrough hdmi
#define  DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DD_LATENCY            (0)
#define  DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DDP_LATENCY           (-100)
#define  DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DD_LATENCY_PROPERTY         "vendor.media.audio.hal.ms12.dtv.passthrough.dd"
#define  DTV_AVSYNC_MS12_HDMI_OUT_PASSTHROUGH_DDP_LATENCY_PROPERTY        "vendor.media.audio.hal.ms12.dtv.passthrough.ddp"

/* end dtv MS12 tuning part */

//channel define
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLERATE 48
#define DEFAULT_DATA_WIDTH 2

   /*------------------------------------------------------------
    this function is designed for av alternately distributed situation
    startplay have below situation
    1.apts = vpts
    strategy 0: when pcr >= vpts, av pts sync output together
    2.apts > vpts
    strategy 1: audio write 0, video not show first frame
                when pcr >= vpts, video output normal
                when pcr >= apts, audio output normal
    strategy 2: show first frame, video fast play
                when pcr >= vpts, video output normal
                when pcr >= apts, audio output normal
    3.apts < vpts
    strategy 3: video show first frame and block
                when pcr >= apts, audio output normal
                when pcr >= vpts, av pts sync output together
    strategy 4: video not show first frame
                when pcr >= apts, mute audio
                when pcr >= vpts, av pts sync output together
    strategy 5: video show first frame and block
                when pcr >= apts, mute audio
                when pcr >= vpts, av pts sync output together
    strategy 6: drop audio pcm, video show first frame and block
                when apts >= vpts, pcr is set by (vpts - latency)
    strategy 7: drop audio pcm, video not show first frame
                when apts >= vpts, pcr is set by (vpts - latency)
    -------------------------------------------------------------*/
enum {
    STRATEGY_AVOUT_NORMAL = 0,
    STRATEGY_A_ZERO_V_NOSHOW,
    STRATEGY_A_NORMAL_V_SHOW_QUICK,
    STRATEGY_A_NORMAL_V_SHOW_BLOCK,
    STRATEGY_A_NORMAL_V_NOSHOW,
    STRATEGY_A_MUTE_V_SHOW_BLOCK,
    STRATEGY_A_DROP_V_SHOW_BLOCK,
    STRATEGY_A_DROP_V_NOSHOW,
};

struct avsync_para {
    int cur_pts_diff; // pcr-apts
    int pcr_adjust_max; //pcr adjust max value
    int checkin_underrun_flag; //audio checkin underrun
    int in_out_underrun_flag; //input/output both underrun
    unsigned long underrun_checkinpts; //the checkin apts when in underrun
    int underrun_mute_time_min; //not happen loop underrun mute time ms
    int underrun_mute_time_max; //happen loop underrun mute max time ms
    int underrun_max_time;  //max time of underrun to force clear
    struct timespec underrun_starttime; //input-output both underrun starttime
    struct timespec underrun_mute_starttime; //underrun mute start time
};

struct audiohal_debug_para {
    int debug_time_interval;
    unsigned int debug_last_checkin_apts;
    unsigned int debug_last_checkin_vpts;
    unsigned int debug_last_out_apts;
    unsigned int debug_last_out_vpts;
    unsigned int debug_last_demux_pcr;
    struct timespec debug_system_time;
};

extern int get_audio_discontinue(void);
extern unsigned long decoder_apts_lookup(unsigned int offset);

void dtv_adjust_i2s_output_clock(struct aml_audio_patch* patch, int direct, int step);
void dtv_adjust_spdif_output_clock(struct aml_audio_patch* patch, int direct, int step, bool spdifb);
int dtv_avsync_get_apts_latency(struct audio_stream_out *stream);



#endif
