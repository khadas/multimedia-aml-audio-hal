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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0
#define __USE_GNU

#include <cutils/log.h>
#include <dolby_ms12.h>
#include <dolby_ms12_config_params.h>
#include <dolby_ms12_status.h>
#include <aml_android_utils.h>
#include <sys/prctl.h>
#include <cutils/properties.h>

#include "audio_hw_ms12.h"
#include "alsa_config_parameters.h"
#include "aml_ac3_parser.h"
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include "audio_hw.h"
#include "alsa_manager.h"
#include "aml_audio_stream.h"
#include "dolby_lib_api.h"
#include "aml_audio_timer.h"
#include "audio_virtual_buf.h"
#include "ac3_parser_utils.h"
#include "aml_audio_ac3parser.h"
#include "audio_hw_utils.h"
#include "aml_audio_spdif_output.h"
#include "aml_audio_ms12_bypass.h"
#include "aml_audio_ac4parser.h"
#include "aml_volume_utils.h"



#define DOLBY_DRC_LINE_MODE 0
#define DOLBY_DRC_RF_MODE   1
#define DDP_MAX_BUFFER_SIZE 2560//dolby ms12 input buffer threshold
#define CONVERT_ONEDB_TO_GAIN  1.122018f
#define MS12_MAIN_INPUT_BUF_NS (96000000LL)
/*if we choose 96ms, it will cause audio filnger underrun,
  if we choose 64ms, it will cause ms12 underrun,
  so we choose 84ms now
*/
#define MS12_SYS_INPUT_BUF_NS  (84000000LL)

#define NANO_SECOND_PER_SECOND 1000000000LL


#define MS12_MAIN_BUF_INCREASE_TIME_MS (0)
#define MS12_SYS_BUF_INCREASE_TIME_MS (1000)
#define DDPI_UDC_COMP_LINE 2


#define MS12_OUTPUT_PCM_FILE "/data/vendor/audiohal/ms12_pcm.raw"
#define MS12_OUTPUT_BITSTREAM_FILE "/data/vendor/audiohal/ms12_bitstream.raw"
#define MS12_INPUT_SYS_PCM_FILE "/data/vendor/audiohal/ms12_input_sys.pcm"
#define MS12_INPUT_SYS_MAIN_FILE "/data/vendor/audiohal/ms12_input_main.raw"
#define MS12_INPUT_SYS_MAIN_FILE2 "/data/vendor/audiohal/ms12_input_spdif.raw"
#define MS12_INPUT_SYS_APP_FILE "/data/vendor/audiohal/ms12_input_app.pcm"

#define MS12_MAIN_WRITE_RETIMES             (600)
#define MS12_ATMOS_TRANSITION_THRESHOLD     (3)

static int nbytes_of_dolby_ms12_downmix_output_pcm_frame();


/*
 *@brief dump ms12 output data
 */
static void dump_ms12_output_data(void *buffer, int size, char *file_name)
{
    if (aml_getprop_bool("media.audiohal.outdump")) {
        FILE *fp1 = fopen(file_name, "a+");
        if (fp1) {
            int flen = fwrite((char *)buffer, 1, size, fp1);
            ALOGV("%s buffer %p size %d\n", __FUNCTION__, buffer, size);
            fclose(fp1);
        }
    }
}

static void *dolby_ms12_threadloop(void *data);

static void set_ms12_out_ddp_5_1(audio_format_t input_format, bool is_sink_supported_ddp_atmos)
{
    /*In case of AC-4 or Dolby Digital Plus input, set legacy ddp out ON/OFF*/
    bool is_ddp = (input_format == AUDIO_FORMAT_AC3) || (input_format == AUDIO_FORMAT_E_AC3);
    bool is_ac4 = (input_format == AUDIO_FORMAT_AC4);
    if (is_ddp || is_ac4) {
        bool is_out_ddp_5_1 = !is_sink_supported_ddp_atmos;
        /*
         *case1 support ddp atmos(is_out_ddp_5_1 as false), MS12 should output Dolby Atmos as 5.1.2
         *case2 only support ddp(is_out_ddp_5_1 as true), MS12 should downmix Atmos signals rendered from 5.1.2 to 5.1
         *It only effect the DDP-Atmos(5.1.2) output.
         *1.ATMOS INPUT, the ddp encoder output will output DDP-ATMOS
         *2.None-Atmos and Continuous with -atmos_lock=1,the ddp encoder output will output DDP-ATMOS.
         */
        dolby_ms12_set_ddp_5_1_out(is_out_ddp_5_1);
    }
}

bool is_platform_supported_ddp_atmos(bool atmos_supported, enum OUT_PORT current_out_port)
{
    if (current_out_port == OUTPORT_HDMI_ARC) {
        /*ARC case*/
        return atmos_supported;
    }
    else {
        /*SPEAKER/HEADPHONE case*/
        return true;
    }

}

bool is_ms12_out_ddp_5_1_suitable(bool is_ddp_atmos)
{
    bool is_ms12_out_ddp_5_1 = dolby_ms12_get_ddp_5_1_out();
    bool is_sink_only_ddp_5_1 = !is_ddp_atmos;

    if (is_ms12_out_ddp_5_1 == is_sink_only_ddp_5_1) {
        /*
         *Sink device can decode MS12 DDP bitstream correctly
         *case1 Sink device Support DDP ATMOS, MS12 output DDP-ATMOS(5.1.2)
         *case2 Sink device Support DDP, MS12 output DDP(5.1)
         */
        return true;
    } else {
        /*
         *case1 Sink device support DDP ATMOS, but MS12 output DDP(5.1)
         *case2 Sink device support DDP 5.1, but MS12 output DDP-ATMOS(5.1.2)
         *should reconfig the parameter about MS12 output DDP bitstream.
         */
        return false;
    }
}

/*
 *@brief get ms12 output configure mask
 */
static int get_ms12_output_mask(audio_format_t sink_format, audio_format_t optical_format, bool spdif_on)
{
    int  output_config;
    if (sink_format == AUDIO_FORMAT_E_AC3)
        output_config =  MS12_OUTPUT_MASK_DDP;
    else if (sink_format == AUDIO_FORMAT_AC3)
        output_config = MS12_OUTPUT_MASK_DD;
    else if (sink_format == AUDIO_FORMAT_MAT)
        output_config = MS12_OUTPUT_MASK_MAT;
    else if (sink_format == AUDIO_FORMAT_PCM_16_BIT && optical_format == AUDIO_FORMAT_AC3)
        output_config = MS12_OUTPUT_MASK_DD | MS12_OUTPUT_MASK_SPEAKER;
    else if (sink_format == AUDIO_FORMAT_PCM_16_BIT && optical_format == AUDIO_FORMAT_E_AC3)
        output_config = MS12_OUTPUT_MASK_DDP | MS12_OUTPUT_MASK_SPEAKER;
    else if (sink_format == AUDIO_FORMAT_PCM_16_BIT && optical_format == AUDIO_FORMAT_MAT)
        output_config = MS12_OUTPUT_MASK_MAT | MS12_OUTPUT_MASK_SPEAKER;
    else
        output_config = MS12_OUTPUT_MASK_SPEAKER | MS12_OUTPUT_MASK_STEREO;

    /* enable downmix output for headphone always-on */
    output_config |= MS12_OUTPUT_MASK_STEREO;

    if (spdif_on && (output_config & (MS12_OUTPUT_MASK_DDP))) {
        output_config |= MS12_OUTPUT_MASK_DD;
    }

    return output_config;
}

static void update_ms12_atmos_info(struct dolby_ms12_desc *ms12)
{
    int is_atmos = 0;

    is_atmos = (dolby_ms12_get_input_atmos_info() == 1);
    /*atmos change from 1 to 0, we need a threshold cnt*/
    if (ms12->is_dolby_atmos && !is_atmos) {
        ms12->atmos_info_change_cnt++;
        if (ms12->atmos_info_change_cnt > MS12_ATMOS_TRANSITION_THRESHOLD) {
            ms12->is_dolby_atmos = 0;
        } else {
            ms12->is_dolby_atmos = 1;
        }
    } else {
        ms12->atmos_info_change_cnt = 0;
        ms12->is_dolby_atmos = is_atmos;
    }
    ALOGV("atmos =%d new =%d cnt=%d",ms12->is_dolby_atmos, is_atmos, ms12->atmos_info_change_cnt);
    return;
}

void set_ms12_ad_mixing_enable(struct dolby_ms12_desc *ms12, int ad_mixing_enable)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-xa", ad_mixing_enable);
    if (strlen(parm) > 0)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_ad_mixing_level(struct dolby_ms12_desc *ms12, int mixing_level)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-xu", mixing_level);
    if (strlen(parm) > 0)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_dolby_ms12_runtime_pause(struct dolby_ms12_desc *ms12, int is_pause)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-pause", is_pause);
    if ((strlen(parm) > 0) && ms12) {
        aml_ms12_update_runtime_params(ms12, parm);
    }
}

void set_dolby_ms12_runtime_system_mixing_enable(struct dolby_ms12_desc *ms12, int system_mixing_enable)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-xs", system_mixing_enable);
    if ((strlen(parm) > 0) && ms12) {
        aml_ms12_update_runtime_params(ms12, parm);
    }
}

void set_ms12_atmos_lock(struct dolby_ms12_desc *ms12, bool is_atmos_lock_on)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-atmos_lock", is_atmos_lock_on);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

/*
 *@brief get dolby ms12 prepared
 */
int get_the_dolby_ms12_prepared(
    struct aml_stream_out *aml_out
    , audio_format_t input_format
    , audio_channel_mask_t input_channel_mask
    , int input_sample_rate)
{
    ALOGI("+%s()", __FUNCTION__);
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_stream_out *out;
    struct aml_arc_hdmi_desc descs = {0};
    bool is_atmos_supported = false;
    int ret = 0;
    // LINUX change
    // Linux does not use sysfs to get drc settings
    // drc settings will be from ms12_runtime switches
    // with set_parameter API
#if 0
    int dolby_ms12_drc_mode = DOLBY_DRC_RF_MODE;
    int drc_mode = 0;
    int drc_cut = 0;
    int drc_boost = 0;
#endif
    struct aml_audio_patch *patch = adev->audio_patch;
    ALOGI("\n+%s()", __FUNCTION__);
    pthread_mutex_lock(&ms12->lock);
    ALOGI("++%s(), locked", __FUNCTION__);
    set_audio_system_format(AUDIO_FORMAT_PCM_16_BIT);
    /* set MS12 TV tuning mode */
    dolby_ms12_config_params_set_tv_tuning(adev->ms12_tv_tuning);
    /*
    when HDMITX send pause frame,we treated as INVALID format.
    for MS12,we treat it as LPCM and mute the frame
    */
    if (input_format == AUDIO_FORMAT_INVALID) {
        input_format = AUDIO_FORMAT_PCM_16_BIT;
    }
    set_audio_app_format(AUDIO_FORMAT_PCM_16_BIT);
    set_audio_main_format(input_format);
    ALOGI("+%s() dual_decoder_support %d\n", __FUNCTION__, adev->dual_decoder_support);

    // LINUX change
#if 0
    if (0 == aml_audio_get_dolby_drc_mode(&drc_mode, &drc_cut, &drc_boost))
        dolby_ms12_drc_mode = (drc_mode == DDPI_UDC_COMP_LINE) ? DOLBY_DRC_LINE_MODE : DOLBY_DRC_RF_MODE;
    //for mul-pcm
    dolby_ms12_set_drc_boost(drc_boost);
    dolby_ms12_set_drc_cut(drc_cut);
    //for 2-channel downmix
    dolby_ms12_set_drc_boost_stereo(drc_boost);
    dolby_ms12_set_drc_cut_stereo(drc_cut);

    /*
     * if main input is hdmi-in/dtv/other-source PCM
     * would not go through the DRC processing
     * DRC LineMode means to bypass DRC processing.
     */
    if (audio_is_linear_pcm(input_format)) {
        dolby_ms12_drc_mode = DOLBY_DRC_LINE_MODE;
    }
#endif

    /*set the associate audio format*/
    if (adev->dual_decoder_support == true) {
        set_audio_associate_format(input_format);
        ALOGI("%s set_audio_associate_format %#x", __FUNCTION__, input_format);
    }
    dolby_ms12_set_asscociated_audio_mixing(adev->associate_audio_mixing_enable);
    dolby_ms12_set_user_control_value_for_mixing_main_and_associated_audio(adev->mixing_level);

    // LINUX change
#if 0
    dolby_ms12_set_drc_mode(dolby_ms12_drc_mode);
    dolby_ms12_set_dap_drc_mode(dolby_ms12_drc_mode);
    ALOGI("%s dolby_ms12_set_drc_mode %s", __FUNCTION__, (dolby_ms12_drc_mode == DOLBY_DRC_RF_MODE) ? "RF MODE" : "LINE MODE");
#endif

    /*set the continous output flag*/
    set_dolby_ms12_continuous_mode((bool)adev->continuous_audio_mode);
    dolby_ms12_set_atmos_lock_flag(adev->atoms_lock_flag);

    /*set the dolby ms12 debug level*/
    dolby_ms12_enable_debug();

    /*
     *In case of AC-4 or Dolby Digital Plus input,
     *set output DDP bitstream format DDP Atmos(5.1.2) or DDP(5.1)
     */
    // LINUX change
#if 1
    get_sink_capability(adev, &descs);
    is_atmos_supported = is_platform_supported_ddp_atmos(descs.ddp_fmt.atmos_supported, adev->active_outport);
    set_ms12_out_ddp_5_1(input_format, is_atmos_supported);
#else
    // set -legacy_ddplus_out option based on Atmos capbility on sink side
    set_ms12_out_ddp_5_1(input_format, adev->hdmi_descs.ddp_fmt.atmos_supported);
#endif

    /* create  the ms12 output stream here */
    /*************************************/
    if (continous_mode(adev)) {
        // TODO: zz: Might have memory leak, not clear route to release this pointer
        out = (struct aml_stream_out *)calloc(1, sizeof(struct aml_stream_out));
        if (!out) {
            ALOGE("%s malloc stream failed", __func__);
            goto Err;
        }
        /* copy stream information */
        memcpy(out, aml_out, sizeof(struct aml_stream_out));
        if (adev->is_TV) {
            out->is_tv_platform  = 1;
            out->config.channels = 8;
            out->config.format = PCM_FORMAT_S16_LE;
            out->tmp_buffer_8ch = malloc(out->config.period_size * 4 * 8);
            if (out->tmp_buffer_8ch == NULL) {
                ALOGE("%s cannot malloc memory for out->tmp_buffer_8ch", __func__);
                goto Err_tmp_buf_8ch;

            }
            out->tmp_buffer_8ch_size = out->config.period_size * 4 * 8;
            out->audioeffect_tmp_buffer = malloc(out->config.period_size * 6);
            if (out->audioeffect_tmp_buffer == NULL) {
                ALOGE("%s cannot malloc memory for audioeffect_tmp_buffer", __func__);
                goto Err_audioeffect_tmp_buf;
            }
        }
        ALOGI("%s create ms12 stream %p,original stream %p", __func__, out, aml_out);
    } else {
        out = aml_out;
    }
    adev->ms12_out = out;
    ALOGI("%s adev->ms12_out =  %p", __func__, adev->ms12_out);
    /************end**************/
    /*set the system app sound mixing enable*/
    if (adev->continuous_audio_mode) {
        adev->system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_ON;
    }
    dolby_ms12_set_system_app_audio_mixing(adev->system_app_mixing_status);

#if 0
    int output_config = MS12_OUTPUT_MASK_SPEAKER | MS12_OUTPUT_MASK_DD | MS12_OUTPUT_MASK_DDP | MS12_OUTPUT_MASK_STEREO;
    /*we reconfig the ms12 nodes depending on the user case when digital input case to refine ms12 perfermance*/
    if (patch && \
           (patch->input_src == AUDIO_DEVICE_IN_HDMI || patch->input_src == AUDIO_DEVICE_IN_SPDIF)) {
        output_config = get_ms12_output_mask(adev->sink_format, adev->optical_format,adev->active_outport == OUTPORT_HDMI_ARC);
    }
#else
    // LINUX Change
    // Currently we only enable max one PCM and one bitstream (DD/DDP/MAT) output to save on CPU loading
    int output_config = get_ms12_output_mask(adev->sink_format, adev->optical_format, adev->spdif_on);

    if (adev->active_outport == OUTPORT_HDMI_ARC) {
        /* when ARC/eARC is running, disable DAP for CPU loading */
        output_config &= ~MS12_OUTPUT_MASK_SPEAKER;
    }
#endif

    aml_ms12_config(ms12, input_format, input_channel_mask, input_sample_rate,output_config);
    if (ms12->dolby_ms12_enable) {
        //register Dolby MS12 callback
        dolby_ms12_register_output_callback(ms12_output, (void *)out);
        ms12->device = usecase_device_adapter_with_ms12(out->device,AUDIO_FORMAT_PCM_16_BIT/* adev->sink_format*/);
        ALOGI("%s out [dual_output_flag %d] adev [format sink %#x optical %#x] ms12 [output-format %#x device %d]",
              __FUNCTION__, out->dual_output_flag, adev->sink_format, adev->optical_format, ms12->output_config, ms12->device);
        memcpy((void *) & (adev->ms12_config), (const void *) & (out->config), sizeof(struct pcm_config));
        get_hardware_config_parameters(
            &(adev->ms12_config)
            ,AUDIO_FORMAT_PCM_16_BIT/* adev->sink_format*/
            , audio_channel_count_from_out_mask(ms12->output_channelmask)
            , ms12->output_samplerate
            , out->is_tv_platform
            , continous_mode(adev));

        if (continous_mode(adev)) {
            ms12->dolby_ms12_thread_exit = false;
            ret = pthread_create(&(ms12->dolby_ms12_threadID), NULL, &dolby_ms12_threadloop, out);
            if (ret != 0) {
                ALOGE("%s, Create dolby_ms12_thread fail!\n", __FUNCTION__);
                goto Err_dolby_ms12_thread;
            }
            ALOGI("%s() thread is builded, get dolby_ms12_threadID %ld\n", __FUNCTION__, ms12->dolby_ms12_threadID);
        }
        //n bytes of dowmix output pcm frame, 16bits_per_sample / stereo, it value is 4btes.
        ms12->nbytes_of_dmx_output_pcm_frame = nbytes_of_dolby_ms12_downmix_output_pcm_frame();
        ms12->hdmi_format = adev->hdmi_format;
        ms12->optical_format = adev->optical_format;
        ms12->main_input_fmt = input_format;
        ms12->main_input_sr = input_sample_rate;
    }
    ms12->sys_audio_base_pos = adev->sys_audio_frame_written;
    ALOGI("set ms12 sys pos =%lld", ms12->sys_audio_base_pos);

    /* set up initial volume if it's compressed format or direct PCM */
    if (is_dolby_ms12_support_compression_format(input_format) ||
        is_direct_stream_and_pcm_format(aml_out)) {
        dolby_ms12_set_main_volume(aml_out->volume_l);
    }

    aml_ac3_parser_open(&ms12->ac3_parser_handle);
    aml_ms12_bypass_open(&ms12->ms12_bypass_handle);
    ring_buffer_init(&ms12->spdif_ring_buffer, ms12->dolby_ms12_out_max_size);
    ms12->lpcm_temp_buffer = (unsigned char*)malloc(ms12->dolby_ms12_out_max_size);
    if (!ms12->lpcm_temp_buffer) {
        ALOGE("%s malloc lpcm_temp_buffer failed", __func__);
        if (continous_mode(adev))
            goto Err_dolby_ms12_thread;
        else
            goto Err;
    }
    ALOGI("--%s(), locked", __FUNCTION__);
    pthread_mutex_unlock(&ms12->lock);
    ALOGI("-%s()\n\n", __FUNCTION__);
    return ret;

Err_dolby_ms12_thread:
    if (continous_mode(adev)) {
        if (ms12->dolby_ms12_enable) {
            ALOGE("%s() %d exit dolby_ms12_thread\n", __FUNCTION__, __LINE__);
            ms12->dolby_ms12_thread_exit = true;
            ms12->dolby_ms12_threadID = 0;
        }
        free(out->audioeffect_tmp_buffer);
    }
Err_audioeffect_tmp_buf:
    free(out->tmp_buffer_8ch);
Err_tmp_buf_8ch:
    free(out);
Err:
    pthread_mutex_unlock(&ms12->lock);
    return ret;
}

static bool is_iec61937_format(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    /*
     *Attation, the DTV input frame(format/size) is IEC61937.
     *but the dd/ddp of HDMI-IN, has same format as IEC61937 but size do not match.
     *Fixme: in Kodi APK, audio passthrough choose AUDIO_FORMAT_IEC61937.
    */
    return (aml_out->hal_format == AUDIO_FORMAT_IEC61937);
}

bool is_bypass_ms12(struct audio_stream_out *stream) {
    bool bypass_ms12 = false;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    if (adev->hdmi_format == BYPASS && ms12->optical_format == aml_out->hal_internal_format) {
        if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3 ||
            aml_out->hal_internal_format == AUDIO_FORMAT_AC3) {
            /*current we only support 48k ddp/dd bypass*/
            if (aml_out->hal_rate == 48000) {
                   bypass_ms12 = true;
            }
        }
    }
    return bypass_ms12;
}

static void endian16_convert(void *buf, int size)
{
    int i;
    unsigned short *p = (unsigned short *)buf;
    for (i = 0; i < size / 2; i++, p++) {
        *p = ((*p & 0xff) << 8) | ((*p) >> 8);
    }
}

static int scan_dolby_frame_info(const unsigned char *frame_buf,
        int length,
        int *frame_offset,
        int *frame_size,
        int *frame_numblocks,
        int *framevalid_flag)
{
    int scan_frame_offset;
    int scan_frame_size;
    int scan_channel_num;
    int scan_numblks;
    int scan_timeslice_61937;
    // int scan_framevalid_flag;
    int ret = 0;
    int total_channel_num  = 0;

    if (!frame_buf || (length <= 0) || !frame_offset || !frame_size || !frame_numblocks || !framevalid_flag) {
        ret = -1;
    } else {
        ret = parse_dolby_frame_header(frame_buf, length, &scan_frame_offset, &scan_frame_size
                                       , &scan_channel_num, &scan_numblks,
                                       &scan_timeslice_61937, framevalid_flag);

        if (ret == 0) {
            *frame_offset = scan_frame_offset;
            *frame_size = scan_frame_size;
            *frame_numblocks = scan_numblks;
            //this scan is useful, return 0
            return 0;
        }
    }
    //this scan is useless, return -1
    return -1;
}

/*dtv single decoder, if input data is less than one iec61937 size, and do not contain one complete frame
 *after adding the frame_deficiency, got a complete frame without scan the frame
 *keyword: frame_deficiency
 */
bool is_frame_lack_of_data_in_dtv(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    return (!adev->dual_decoder_support
        && (is_iec61937_format(stream) || aml_out->hal_format == AUDIO_FORMAT_IEC61937)
        && (aml_out->frame_deficiency > 0));
}

/*
 *in continuous mode, the dolby frame will be splited into several part
 *because of out_get_buffer_size is an stable size, but the dolby frame size is variable.
 */
bool is_frame_lack_of_data_in_continuous(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    bool is_lack = (adev->continuous_audio_mode == 1) \
                    && ((aml_out->hal_format == AUDIO_FORMAT_AC3) || (aml_out->hal_format == AUDIO_FORMAT_E_AC3)) \
                    && (aml_out->frame_deficiency > 0);

    return is_lack;
}


int dolby_ms12_bypass_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    if (is_bypass_ms12(stream)) {
        /*for HDMI in IEC61937 format, we support passthrough it*/
        if (is_iec61937_format(stream)) {
            aml_audio_spdif_output_direct(stream, (void*)buffer, bytes, aml_out->hal_internal_format);
            ALOGV("IEC61937 bypass out size=%d", bytes);
        }
    }
    return 0;
}


/*
 *@brief dolby ms12 main process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     use_size: buffer used size
 */

int dolby_ms12_main_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *use_size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int ms12_output_size = 0;
    int dolby_ms12_input_bytes = 0;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;

    void *input_buffer = (void *)buffer;
    size_t input_bytes = bytes;
    int dual_decoder_used_bytes = 0;
    int single_decoder_used_bytes = 0;
    void *main_frame_buffer = input_buffer;/*input_buffer as default*/
    int main_frame_size = input_bytes;/*input_bytes as default*/
    void *associate_frame_buffer = NULL;
    int associate_frame_size = 0;
    size_t main_frame_deficiency = 0;
    int32_t parser_used_size = 0;
    int dependent_frame = 0;
    int sample_rate = 48000;
    struct ac4_parser_info ac4_info = { 0 };

    if (adev->debug_flag >= 2) {
        ALOGI("\n%s() in continuous %d input ms12 bytes %d input bytes %zu\n",
              __FUNCTION__, adev->continuous_audio_mode, dolby_ms12_input_bytes, input_bytes);
    }

    /*this status is only updated in hw_write, continuous mode also need it*/
    if (adev->continuous_audio_mode) {
        if (aml_out->status != STREAM_HW_WRITING) {
            aml_out->status = STREAM_HW_WRITING;
        }
    }

    /*I can't find where to init this, so I put it here*/
    if ((ms12->main_virtual_buf_handle == NULL) && adev->continuous_audio_mode == 1) {
        /*set the virtual buf size to 96ms*/
        audio_virtual_buf_open(&ms12->main_virtual_buf_handle
            , "ms12 main input"
            , MS12_MAIN_INPUT_BUF_NS
            , MS12_MAIN_INPUT_BUF_NS
            , MS12_MAIN_BUF_INCREASE_TIME_MS);
    }

    if (is_frame_lack_of_data_in_dtv(stream)) {
        ALOGV("\n%s() frame_deficiency = %d , input bytes = %d\n",
            __FUNCTION__, aml_out->frame_deficiency , input_bytes);
        if (aml_out->frame_deficiency <= (int)input_bytes) {
            main_frame_size = aml_out->frame_deficiency;
            single_decoder_used_bytes = aml_out->frame_deficiency;
            aml_out->frame_deficiency = 0;
        } else {
            main_frame_size = input_bytes;
            single_decoder_used_bytes = input_bytes;
            aml_out->frame_deficiency -= input_bytes;
        }
        goto MAIN_INPUT;
    }

    if (is_frame_lack_of_data_in_continuous(stream)) {
        ALOGV("\n%s() frame_deficiency = %d , input bytes = %d\n",
            __FUNCTION__, aml_out->frame_deficiency , input_bytes);
        if (aml_out->frame_deficiency <= (int)input_bytes) {
            main_frame_size = aml_out->frame_deficiency;
            single_decoder_used_bytes = aml_out->frame_deficiency;
            // aml_out->frame_deficiency = 0;
        } else {
            main_frame_size = input_bytes;
            single_decoder_used_bytes = input_bytes;
            // aml_out->frame_deficiency -= input_bytes;
        }
        goto MAIN_INPUT;
    }

    if (ms12->dolby_ms12_enable) {
        //ms12 input main
        int dual_input_ret = 0;
        pthread_mutex_lock(&ms12->main_lock);
        if (adev->dual_decoder_support == true) {
            dual_input_ret = scan_dolby_main_associate_frame(input_buffer
                             , input_bytes
                             , &dual_decoder_used_bytes
                             , &main_frame_buffer
                             , &main_frame_size
                             , &associate_frame_buffer
                             , &associate_frame_size);
            if (dual_input_ret) {
                ALOGE("%s used size %zu dont find the iec61937 format header, rescan next time!\n", __FUNCTION__, *use_size);
                goto  exit;
            }
        }
        /*
        As the audio payload may cross two write process,we can not skip the
        data when we do not get a complete payload.for ATSC,as we have a
        complete burst align for 6144/24576,so we always can find a valid
        payload in one write process.
        */
        else if (is_iec61937_format(stream)) {
            //keyword: frame_deficiency
            int single_input_ret = scan_dolby_main_frame_ext(input_buffer
                                   , input_bytes
                                   , &single_decoder_used_bytes
                                   , &main_frame_buffer
                                   , &main_frame_size
                                   , &main_frame_deficiency);
            if (single_input_ret) {
                ALOGE("%s used size %zu dont find the iec61937 format header, rescan next time!\n", __FUNCTION__, *use_size);
                *use_size = bytes;
                goto  exit;
            }
            if (main_frame_deficiency > 0) {
                main_frame_size = main_frame_size - main_frame_deficiency;
            }
            aml_out->frame_deficiency = main_frame_deficiency;
        }
        /*
         *continuous output with dolby atmos input, the ddp frame size is variable.
         */
        else if ((adev->continuous_audio_mode == 1) || (adev->hdmi_format == BYPASS)) {
            if ((aml_out->hal_format == AUDIO_FORMAT_AC3) || (aml_out->hal_format == AUDIO_FORMAT_E_AC3)) {
                const unsigned char *frame_buf = (const unsigned char *)main_frame_buffer;
                // int main_frame_size = input_bytes;
                int frame_offset = 0;
                int frame_size = 0;
                int frame_numblocks = 0;
                if (adev->debug_flag) {
                    ALOGI("%s line %d ###### frame size %d deficiency %d #####",
                        __func__, __LINE__, aml_out->ddp_frame_size, aml_out->frame_deficiency);
                }

                struct ac3_parser_info ac3_info = { 0 };
                aml_ac3_parser_process(ms12->ac3_parser_handle, input_buffer, bytes, &parser_used_size, &main_frame_buffer, &main_frame_size, &ac3_info);
                ALOGV("bytes=%d, parser_used_size=%d, main_frame_size=%d", bytes, parser_used_size, main_frame_size);
                if (main_frame_size != 0) {
                    struct bypass_frame_info frame_info = { 0 };
                    aml_out->ddp_frame_size    = main_frame_size;
                    aml_out->frame_deficiency  = aml_out->ddp_frame_size;
                    frame_info.audio_format    = aml_out->hal_format;
                    frame_info.samplerate      = ac3_info.sample_rate;
                    frame_info.dependency_frame = ac3_info.frame_dependent;
                    frame_info.numblks         = ac3_info.numblks;
                    aml_ms12_bypass_checkin_data(ms12->ms12_bypass_handle, main_frame_buffer, main_frame_size, &frame_info);
                }
                aml_out->ddp_frame_nblks = ac3_info.numblks;
                aml_out->total_ddp_frame_nblks += aml_out->ddp_frame_nblks;
                dependent_frame = ac3_info.frame_dependent;
                sample_rate = ac3_info.sample_rate;
                if (ac3_info.frame_size == 0) {
                    *use_size = parser_used_size;
                    if (parser_used_size == 0) {
                        *use_size = bytes;
                    }
                    goto exit;
                }

           } else if (aml_out->hal_format == AUDIO_FORMAT_AC4) {
                aml_ac4_parser_process(aml_out->ac4_parser_handle, input_buffer, bytes, &parser_used_size, &main_frame_buffer, &main_frame_size, &ac4_info);
                ALOGV("frame size =%d frame rate=%d sample rate=%d used =%d", ac4_info.frame_size, ac4_info.frame_rate, ac4_info.sample_rate, parser_used_size);
                if (main_frame_size == 0 && parser_used_size == 0) {
                    *use_size = bytes;
                    ALOGE("wrong ac4 frame size");
                    goto exit;
                }

            }
        }

        if (adev->dual_decoder_support == true) {
            /*if there is associate frame, send it to dolby ms12.*/
            char tmp_array[4096] = {0};
            if (!associate_frame_buffer || (associate_frame_size == 0)) {
                associate_frame_buffer = (void *)&tmp_array[0];
                associate_frame_size = sizeof(tmp_array);
            }
            if (associate_frame_size < main_frame_size) {
                ALOGV("%s() main frame addr %p size %d associate frame addr %p size %d, need a larger ad input size!\n",
                      __FUNCTION__, main_frame_buffer, main_frame_size, associate_frame_buffer, associate_frame_size);
                memcpy(&tmp_array[0], associate_frame_buffer, associate_frame_size);
                associate_frame_size = sizeof(tmp_array);
            }
            dolby_ms12_input_associate(ms12->dolby_ms12_ptr
                                       , (const void *)associate_frame_buffer
                                       , (size_t)associate_frame_size
                                       , ms12->input_config_format
                                       , audio_channel_count_from_out_mask(ms12->config_channel_mask)
                                       , ms12->config_sample_rate
                                      );
        }

MAIN_INPUT:
        if (main_frame_buffer && (main_frame_size > 0)) {
            /*input main frame*/
            int main_format = ms12->input_config_format;
            int main_channel_num = audio_channel_count_from_out_mask(ms12->config_channel_mask);
            int main_sample_rate = ms12->config_sample_rate;
            if ((dolby_ms12_get_dolby_main1_file_is_dummy() == true) && \
                (dolby_ms12_get_ott_sound_input_enable() == true) && \
                (adev->continuous_audio_mode == 1)) {
                //hwsync pcm, 16bits-stereo
                main_format = AUDIO_FORMAT_PCM_16_BIT;
                main_channel_num = aml_out->hal_ch;
                main_sample_rate = 48000;
            }
            /*we check whether there is enough space*/
            if ((adev->continuous_audio_mode == 1) && is_dolby_ms12_support_compression_format(aml_out->hal_internal_format))
            {
                int max_size = 0;
                int main_avail = 0;
                int wait_retry = 0;
                do {
                    main_avail = dolby_ms12_get_main_buffer_avail(&max_size);
                    /* after flush, max_size value will be set to 0 and after write first data,
                     * it will be initialized
                     */
                    if (main_avail == 0 && max_size == 0) {
                        break;
                    }
                    if ((max_size - main_avail) >= main_frame_size) {
                        break;
                    }
                    aml_audio_sleep(5*1000);
                    wait_retry++;
                    /*it cost 3s*/
                    if (wait_retry >= MS12_MAIN_WRITE_RETIMES) {
                        *use_size = parser_used_size;
                        if (parser_used_size == 0) {
                            *use_size = bytes;
                        }
                        ALOGE("write dolby main time out, discard data=%d main_frame_size=%d", *use_size, main_frame_size);
                        goto exit;
                    }

                } while (aml_out->status != STREAM_STANDBY);
            }

            if (is_iec61937_format(stream)) {
                endian16_convert(main_frame_buffer, main_frame_size);
                if (adev->debug_flag > 0)
                    ALOGI("dolby_ms12_input_main %p format = %x", ms12, main_format);
                dolby_ms12_input_bytes = dolby_ms12_input_main(ms12->dolby_ms12_ptr
                                                    , main_frame_buffer
                                                    , main_frame_size
                                                    , main_format
                                                    , main_channel_num
                                                    , main_sample_rate);

                dump_ms12_output_data((void*)main_frame_buffer, dolby_ms12_input_bytes, MS12_INPUT_SYS_MAIN_FILE);

                if (dolby_ms12_input_bytes != main_frame_size) {
                    ALOGE("%s: dolby_ms12_input_main does not consume all data for iec61937 format", __func__);
                }
            }
            else {
                dolby_ms12_input_bytes = dolby_ms12_input_main(
                                                    ms12->dolby_ms12_ptr
                                                    , main_frame_buffer
                                                    , main_frame_size
                                                    , main_format
                                                    , main_channel_num
                                                    , main_sample_rate);
            }
            if (adev->debug_flag >= 2)
                ALOGI("%s line %d main_frame_size %d ret dolby_ms12 input_bytes %d",
                    __func__, __LINE__, main_frame_size, dolby_ms12_input_bytes);

            /*set the dolby ms12 debug level*/
            dolby_ms12_enable_debug();

            if (adev->continuous_audio_mode == 0) {
                ms12->underrun_cnt = dolby_ms12_get_main_underrun();
                dolby_ms12_scheduler_run(ms12->dolby_ms12_ptr);
            }

            if (dolby_ms12_input_bytes > 0) {
                if (adev->dual_decoder_support == true) {
                    *use_size = dual_decoder_used_bytes;
                } else {
                    if (adev->debug_flag >= 2) {
                        ALOGI("%s() continuous %d input ms12 bytes %d input bytes %zu sr %d main size %d parser size %d\n\n",
                              __FUNCTION__, adev->continuous_audio_mode, dolby_ms12_input_bytes, input_bytes, ms12->config_sample_rate, main_frame_size, single_decoder_used_bytes);
                    }

                    if ((adev->continuous_audio_mode == 1) && !is_iec61937_format(stream)) {
                        if (aml_out->frame_deficiency >= dolby_ms12_input_bytes)
                            aml_out->frame_deficiency -= dolby_ms12_input_bytes;
                        else {
                            //FIXME: if aml_out->frame_deficiency is less than dolby_ms12_input_bytes
                            //mostly occur the ac3 parser scan as a failure
                            //need storage the data in an temp buffer.
                            //TODO.
                            aml_out->frame_deficiency = aml_out->ddp_frame_size - dolby_ms12_input_bytes;
                        }
                        if (adev->debug_flag) {
                            ALOGI("%s line %d frame_deficiency %d ret dolby_ms12 input_bytes %d",
                                    __func__, __LINE__, aml_out->frame_deficiency, dolby_ms12_input_bytes);
                        }
                    }

                    // rate control when continuous_audio_mode for streaming
                    if ((adev->continuous_audio_mode == 1) && (adev->audio_patch == 0)) {
                        //FIXME, if ddp input, the size suppose as CONTINUOUS_OUTPUT_FRAME_SIZE
                        //if pcm input, suppose 2ch/16bits/48kHz
                        //FIXME, that MAT/TrueHD input is TODO!!!
                        uint64_t input_ns = 0;
                        if ((aml_out->hal_format == AUDIO_FORMAT_AC3) || \
                            (aml_out->hal_format == AUDIO_FORMAT_E_AC3)) {
                            int sample_nums = aml_out->ddp_frame_nblks * SAMPLE_NUMS_IN_ONE_BLOCK;
                            int frame_duration = DDP_FRAME_DURATION(sample_nums*1000, DDP_OUTPUT_SAMPLE_RATE);
                            //input_ns = (uint64_t)dolby_ms12_input_bytes * frame_duration * 1000000 / aml_out->ddp_frame_size;
                            input_ns = (uint64_t)sample_nums * NANO_SECOND_PER_SECOND / sample_rate;
                            ALOGV("sample_nums=%d input_ns=%lld", sample_nums, input_ns);
                            if (dependent_frame) {
                                input_ns = 0;
                            }
                        } else if (aml_out->hal_format == AUDIO_FORMAT_AC4) {
                            if (ac4_info.frame_rate) {
                                input_ns = (uint64_t)NANO_SECOND_PER_SECOND * 1000 / ac4_info.frame_rate;
                            } else {
                                input_ns = 0;
                            }
                            ALOGV("input ns =%lld frame rate=%d frame size=%d", input_ns, ac4_info.frame_rate, ac4_info.frame_size);
                        } else {
                            /*
                            for LPCM audio,we support it is 2 ch 48K audio.
                            */
                            if (main_channel_num == 0) {
                                main_channel_num = 2;
                            }
                            input_ns = (uint64_t)dolby_ms12_input_bytes * NANO_SECOND_PER_SECOND / (2 * main_channel_num) / ms12->config_sample_rate;
                        }
                        audio_virtual_buf_process(ms12->main_virtual_buf_handle, input_ns);
                    }

                    if (is_iec61937_format(stream)) {
                        *use_size = single_decoder_used_bytes;
                    } else {
                        *use_size = dolby_ms12_input_bytes;
                        if (((adev->continuous_audio_mode == 1) || (adev->hdmi_format == BYPASS)) &&
                            ((aml_out->hal_format == AUDIO_FORMAT_AC3) ||
                            (aml_out->hal_format == AUDIO_FORMAT_E_AC3) ||
                            (aml_out->hal_format == AUDIO_FORMAT_AC4))) {
                            *use_size = parser_used_size;
                        }
                    }

                }
            }
        } else {
            if (adev->dual_decoder_support == true) {
                *use_size = dual_decoder_used_bytes;
            } else {
                *use_size = input_bytes;
            }
        }
        ms12->is_bypass_ms12 = is_bypass_ms12(stream);
exit:
        dump_ms12_output_data((void*)buffer, *use_size, MS12_INPUT_SYS_MAIN_FILE2);
        pthread_mutex_unlock(&ms12->main_lock);
        return 0;
    } else {
        return -1;
    }
}


/*
 *@brief dolby ms12 system process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     use_size: buffer used size
 */
int dolby_ms12_system_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *use_size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    audio_channel_mask_t mixer_default_channelmask = AUDIO_CHANNEL_OUT_STEREO;
    int mixer_default_samplerate = 48000;
    int dolby_ms12_input_bytes = 0;
    int ms12_output_size = 0;
    pthread_mutex_lock(&ms12->lock);
    if (ms12->dolby_ms12_enable) {
        /*set the dolby ms12 debug level*/
        dolby_ms12_enable_debug();

        //Dual input, here get the system data
        dolby_ms12_input_bytes =
            dolby_ms12_input_system(
                ms12->dolby_ms12_ptr
                , buffer
                , bytes
                , AUDIO_FORMAT_PCM_16_BIT
                , audio_channel_count_from_out_mask(mixer_default_channelmask)
                , mixer_default_samplerate);
        if (dolby_ms12_input_bytes > 0) {
            *use_size = dolby_ms12_input_bytes;
        }else {
            *use_size = 0;
        }
    }
    //((void*)buffer, *use_size, MS12_INPUT_SYS_PCM_FILE);
    pthread_mutex_unlock(&ms12->lock);

    if (adev->continuous_audio_mode == 1) {
        uint64_t input_ns = 0;
        input_ns = (uint64_t)(*use_size) * NANO_SECOND_PER_SECOND / 4 / mixer_default_samplerate;

        if (ms12->system_virtual_buf_handle == NULL) {
            //aml_audio_sleep(input_ns/1000);
            if (input_ns == 0) {
                input_ns = (uint64_t)(bytes) * NANO_SECOND_PER_SECOND / 4 / mixer_default_samplerate;
            }
            audio_virtual_buf_open(&ms12->system_virtual_buf_handle, "ms12 system input", input_ns/2, MS12_SYS_INPUT_BUF_NS, MS12_SYS_BUF_INCREASE_TIME_MS);
        }
        audio_virtual_buf_process(ms12->system_virtual_buf_handle, input_ns);
    }

    return 0;
}


/*
 *@brief dolby ms12 app process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     use_size: buffer used size
 */
int dolby_ms12_app_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *use_size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    audio_channel_mask_t mixer_default_channelmask = AUDIO_CHANNEL_OUT_STEREO;
    int mixer_default_samplerate = 48000;
    int dolby_ms12_input_bytes = 0;
    int ms12_output_size = 0;
    int ret = 0;

    pthread_mutex_lock(&ms12->lock);
    if (ms12->dolby_ms12_enable) {
        /*set the dolby ms12 debug level*/
        dolby_ms12_enable_debug();

        //Dual input, here get the system data
        dolby_ms12_input_bytes =
            dolby_ms12_input_app(
                ms12->dolby_ms12_ptr
                , buffer
                , bytes
                , AUDIO_FORMAT_PCM_16_BIT
                , audio_channel_count_from_out_mask(mixer_default_channelmask)
                , mixer_default_samplerate);
        if (dolby_ms12_input_bytes > 0) {
            *use_size = dolby_ms12_input_bytes;
            ret = 0;
        } else {
            *use_size = 0;
            ret = -1;
        }
    }
    dump_ms12_output_data((void*)buffer, *use_size, MS12_INPUT_SYS_APP_FILE);
    pthread_mutex_unlock(&ms12->lock);

    return ret;
}


/*
 *@brief get dolby ms12 cleanup
 */
int get_dolby_ms12_cleanup(struct dolby_ms12_desc *ms12)
{
    int is_quit = 1;

    ALOGI("+%s()", __FUNCTION__);
    if (!ms12) {
        return -EINVAL;
    }

    pthread_mutex_lock(&ms12->lock);
    pthread_mutex_lock(&ms12->main_lock);
    ALOGI("++%s(), locked", __FUNCTION__);
    ALOGI("%s() dolby_ms12_set_quit_flag %d", __FUNCTION__, is_quit);
    dolby_ms12_set_quit_flag(is_quit);

    if (ms12->dolby_ms12_threadID != 0) {
        ms12->dolby_ms12_thread_exit = true;
        pthread_join(ms12->dolby_ms12_threadID, NULL);
        ms12->dolby_ms12_threadID = 0;
        ALOGI("%s() dolby_ms12_threadID reset to %ld\n", __FUNCTION__, ms12->dolby_ms12_threadID);
    }
    set_audio_system_format(AUDIO_FORMAT_INVALID);
    set_audio_app_format(AUDIO_FORMAT_INVALID);
    set_audio_main_format(AUDIO_FORMAT_INVALID);
    dolby_ms12_flush_main_input_buffer();
    dolby_ms12_config_params_set_system_flag(false);
    dolby_ms12_config_params_set_app_flag(false);
    aml_ms12_cleanup(ms12);
    ms12->output_config = 0;
    ms12->dolby_ms12_enable = false;
    ms12->is_dolby_atmos = false;
    ms12->input_total_ms = 0;
    ms12->bitsteam_cnt = 0;
    ms12->nbytes_of_dmx_output_pcm_frame = 0;
    ms12->is_bypass_ms12 = false;
    ms12->last_frames_postion = 0;
    audio_virtual_buf_close(&ms12->main_virtual_buf_handle);
    audio_virtual_buf_close(&ms12->system_virtual_buf_handle);
    aml_ac3_parser_close(ms12->ac3_parser_handle);
    ms12->ac3_parser_handle = NULL;
    ring_buffer_release(&ms12->spdif_ring_buffer);
    if (ms12->lpcm_temp_buffer) {
        free(ms12->lpcm_temp_buffer);
        ms12->lpcm_temp_buffer = NULL;
    }
    aml_ms12_bypass_close(ms12->ms12_bypass_handle);
    ms12->ms12_bypass_handle = NULL;
    ALOGI("--%s(), locked", __FUNCTION__);
    pthread_mutex_unlock(&ms12->main_lock);
    pthread_mutex_unlock(&ms12->lock);
    ALOGI("-%s()", __FUNCTION__);
    return 0;
}

/*
 *@brief set dolby ms12 primary gain
 */
int set_dolby_ms12_primary_input_db_gain(struct dolby_ms12_desc *ms12, int db_gain , int duration)
{
    MixGain gain;
    int ret = 0;

    ALOGI("+%s(): gain %ddb, ms12 enable(%d)",
          __FUNCTION__, db_gain, ms12->dolby_ms12_enable);

    gain.target = db_gain;
    gain.duration = duration;
    gain.shape = 0;
    dolby_ms12_set_system_sound_mixer_gain_values_for_primary_input(&gain);
    //dolby_ms12_set_input_mixer_gain_values_for_main_program_input(&gain);
    //Fixme when tunnel mode is working, the Alexa start and mute the main input!
    //dolby_ms12_set_input_mixer_gain_values_for_ott_sounds_input(&gain);
    // only update very limited parameter with out lock
    //ret = aml_ms12_update_runtime_params_lite(ms12);

exit:
    return ret;
}

#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK
int ms12_output(void *buffer, void *priv_data, size_t size, aml_dec_info_t *ms12_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    audio_format_t output_format = (ms12_info) ? ms12_info->data_type : AUDIO_FORMAT_PCM_16_BIT;
    int ret = 0;
    uint64_t before_time;
    uint64_t after_time;
    ms12->bitsteam_cnt++;
    if (adev->debug_flag > 1) {
        ALOGI("+%s() output size %zu,out format 0x%x.dual_output = %d, optical_format = 0x%x, sink_format = 0x%x, out total=%d main in=%d",
            __FUNCTION__, size,output_format, aml_out->dual_output_flag, adev->optical_format, adev->sink_format,
            ms12->bitsteam_cnt, ms12->input_total_ms);
    }
    if (adev->ms12.optical_format != adev->optical_format) {
         ALOGI("ms12 optical format change from 0x%x to  0x%x\n",adev->ms12.optical_format,adev->optical_format);
         //get_sink_format ((struct audio_stream_out *)aml_out);
         adev->ms12.optical_format= adev->optical_format;
         if (audio_is_linear_pcm(adev->optical_format))
             aml_audio_spdif_output_stop(stream_out);
    }

    update_ms12_atmos_info(ms12);

    if (!audio_is_linear_pcm(output_format)) {
        before_time = aml_audio_get_systime();
        if (output_format == adev->optical_format) {
            void *output_buf = NULL;
            int32_t out_size = 0;
            struct bypass_frame_info frame_info = { 0 };
            uint64_t ms12_dec_out_nframes = dolby_ms12_get_decoder_nframes_pcm_output(adev->ms12.dolby_ms12_ptr, aml_out->hal_internal_format, MAIN_INPUT_STREAM);
            if (ms12_dec_out_nframes != 0 &&
                (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3 || aml_out->hal_internal_format == AUDIO_FORMAT_AC3)) {
                uint64_t consume_offset = dolby_ms12_get_decoder_n_bytes_consumed(adev->ms12.dolby_ms12_ptr, aml_out->hal_internal_format, MAIN_INPUT_STREAM);
                aml_ms12_bypass_checkout_data(ms12->ms12_bypass_handle, &output_buf, &out_size, consume_offset, &frame_info);
            }
            if (adev->hdmi_format != BYPASS) {
                ms12->is_bypass_ms12 = false;
            }
            if (!ms12->is_bypass_ms12) {
                output_buf = buffer;
                out_size   = size;
            } else {
                ALOGV("bypass ms12 size=%d", out_size);
                output_format = aml_out->hal_internal_format;
                if (ms12->is_continuous_paused) {
                    aml_audio_spdif_output_stop(stream_out);
                    out_size = 0;
                }
            }
            if (adev->debug_flag > 1) {
                ALOGI("hdmi format=%d bypass =%d size=%d",adev->hdmi_format, ms12->is_bypass_ms12, out_size);
            }
            if (out_size != 0) {
                /* TM2 with dedicated eARC port */
                if (SUPPORT_EARC_OUT_HW && adev->active_outport == OUTPORT_HDMI_ARC) {
                    if (adev->bHDMIARCon &&
                        audio_hal_data_processing((struct audio_stream_out *)aml_out, buffer, size, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                        ret = hw_write((struct audio_stream_out *)aml_out, output_buffer, output_buffer_bytes, output_format);
                    }
                } else {
                    ret = aml_audio_spdif_output(stream_out, output_buf, out_size,output_format);
                }
                dump_ms12_output_data(buffer, size, MS12_OUTPUT_BITSTREAM_FILE);
            }

        } else if (adev->spdif_on && (output_format == AUDIO_FORMAT_AC3)) {
            /* a secondary DD output which may happen for spdif output when main digital output is DDP for HDMI/ARC */
            ret = aml_audio_spdif_output(stream_out, buffer, size, output_format);
        }
        after_time = aml_audio_get_systime();
    } else if (!(ms12->output_config & MS12_OUTPUT_MASK_SPEAKER) || adev->dap_bypass_enable) {
        /* MS12 does not have DAP output, it can happen when
         * MS12 does not have DAP tuning file installed (dap_bypass_enable is true) or
         * DAP is disabled (MS12_OUTPUT_MASK_SPEAKER is not set when configure MS12),
         * such as to save CPU loading when ARC/eARC is ON.
         */
        if (ms12_info->pcm_type == NORMAL_LPCM) {
            if (get_buffer_write_space (&ms12->spdif_ring_buffer) >= (int) size) {
                ring_buffer_write(&ms12->spdif_ring_buffer, buffer, size, UNCOVER_WRITE);
            }
            if (audio_hal_data_processing((struct audio_stream_out *)aml_out, buffer, size, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                ret = hw_write((struct audio_stream_out *)aml_out, output_buffer, output_buffer_bytes, output_format);
            }
            if (adev->debug_flag > 1) {
                ALOGI("-%s() output_buffer_bytes %d ret %d", __FUNCTION__, output_buffer_bytes, ret);
            }
        }
    } else if (!adev->dap_bypass_enable) {
        if (ms12_info->pcm_type == NORMAL_LPCM) {
            /* will process spdif_ring_buf in audio_hal_data_processing with DAP_LPCM */
            if (get_buffer_write_space (&ms12->spdif_ring_buffer) >= (int) size) {
                ring_buffer_write(&ms12->spdif_ring_buffer, buffer, size, UNCOVER_WRITE);
            }
        } else if (ms12_info->pcm_type == DAP_LPCM) {
            bool usb_pcm_in = (!adev->audio_patch) && adev->ms12_main1_dolby_dummy;
            if (usb_pcm_in) {
                apply_volume(CONVERT_ONEDB_TO_GAIN, buffer, sizeof(int16_t), size); //add 1db to buffer
            }
            if (ms12_info->output_ch == 2) {
                if (audio_hal_data_processing((struct audio_stream_out *)aml_out, buffer, size, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                    ret = hw_write((struct audio_stream_out *)aml_out, output_buffer, output_buffer_bytes, output_format);
                }
            } else if (audio_hal_data_processing_ms12v2((struct audio_stream_out *)aml_out, buffer, size, &output_buffer, &output_buffer_bytes, output_format, ms12_info->output_ch) == 0) {
                ret = hw_write((struct audio_stream_out *)aml_out, output_buffer, output_buffer_bytes, output_format);
            }
        }
    }

    if (((ms12->output_config & (MS12_OUTPUT_MASK_DD | MS12_OUTPUT_MASK_DDP | MS12_OUTPUT_MASK_MAT)) == 0) &&
        (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3 ||
         aml_out->hal_internal_format == AUDIO_FORMAT_AC3)) {
        /* when there is no compressed output from MS12, need consume bypass checkin data */
        void *output_buf = NULL;
        int32_t out_size = 0;
        struct bypass_frame_info frame_info = { 0 };
        uint64_t consume_offset = dolby_ms12_get_decoder_n_bytes_consumed(adev->ms12.dolby_ms12_ptr, aml_out->hal_internal_format, MAIN_INPUT_STREAM);
        if (consume_offset) {
            aml_ms12_bypass_checkout_data(ms12->ms12_bypass_handle, &output_buf, &out_size, consume_offset, &frame_info);
        }
    }

    return ret;
}
#endif

static void *dolby_ms12_threadloop(void *data)
{
    ALOGI("+%s() ", __FUNCTION__);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    if (ms12 == NULL) {
        ALOGE("%s ms12 pointer invalid!", __FUNCTION__);
        goto Error;
    }

    if (ms12->dolby_ms12_enable) {
        dolby_ms12_set_quit_flag(ms12->dolby_ms12_thread_exit);
    }

    prctl(PR_SET_NAME, (unsigned long)"DOLBY_MS12");
    aml_set_thread_priority("DOLBY_MS12", ms12->dolby_ms12_threadID);

    /*affinity the thread to cpu 2/3 which has few IRQ*/
    aml_audio_set_cpu23_affinity();

    while ((ms12->dolby_ms12_thread_exit == false) && (ms12->dolby_ms12_enable)) {
        ALOGV("%s() goto dolby_ms12_scheduler_run", __FUNCTION__);
        if (ms12->dolby_ms12_ptr) {
            ms12->underrun_cnt = dolby_ms12_get_main_underrun();
            dolby_ms12_scheduler_run(ms12->dolby_ms12_ptr);
        } else {
            ALOGE("%s() ms12->dolby_ms12_ptr is NULL, fatal error!", __FUNCTION__);
            break;
        }
        ALOGV("%s() dolby_ms12_scheduler_run end", __FUNCTION__);
    }
    ALOGI("%s remove   ms12 stream %p", __func__, aml_out);
    if (continous_mode(adev)) {
        pthread_mutex_lock(&adev->alsa_pcm_lock);
        aml_alsa_output_close((struct audio_stream_out*)aml_out);
        adev->spdif_encoder_init_flag = false;
        struct pcm *pcm = adev->pcm_handle[DIGITAL_DEVICE];
        if (aml_out->dual_output_flag && pcm) {
            ALOGI("%s close dual output pcm handle %p", __func__, pcm);
            pcm_close(pcm);
            adev->pcm_handle[DIGITAL_DEVICE] = NULL;
            aml_out->dual_output_flag = 0;
            if (adev->pcm_handle[DIGITAL_SPDIF_DEVICE]) {
                pcm_close(adev->pcm_handle[DIGITAL_SPDIF_DEVICE]);
                adev->pcm_handle[DIGITAL_SPDIF_DEVICE] = NULL;
            }
        }
        pthread_mutex_unlock(&adev->alsa_pcm_lock);
        release_audio_stream((struct audio_stream_out *)aml_out);
    }
    adev->ms12_out = NULL;
    ALOGI("-%s(), exit dolby_ms12_thread\n", __FUNCTION__);
    return ((void *)0);

Error:
    ALOGI("-%s(), exit dolby_ms12_thread, because of erro input params\n", __FUNCTION__);
    return ((void *)0);
}

int set_system_app_mixing_status(struct aml_stream_out *aml_out, int stream_status)
{
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_OFF;
    int ret = 0;

    if (STREAM_STANDBY == stream_status) {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_OFF;
    } else {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_ON;
    }

    //when under continuous_audio_mode, system app sound mixing always on.
    if (adev->continuous_audio_mode) {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_ON;
    }

    adev->system_app_mixing_status = system_app_mixing_status;

    if (adev->debug_flag) {
        ALOGI("%s stream-status %d set system-app-audio-mixing %d current %d continuous_audio_mode %d\n", __func__,
              stream_status, system_app_mixing_status, dolby_ms12_get_system_app_audio_mixing(), adev->continuous_audio_mode);
    }

    dolby_ms12_set_system_app_audio_mixing(system_app_mixing_status);

    if (ms12->dolby_ms12_enable) {
        pthread_mutex_lock(&ms12->lock);
        set_dolby_ms12_runtime_system_mixing_enable(ms12, system_app_mixing_status);
        pthread_mutex_unlock(&ms12->lock);
        ALOGI("%s return %d stream-status %d set system-app-audio-mixing %d\n",
              __func__, ret, stream_status, system_app_mixing_status);
        return ret;
    }

    return 1;
}


static int nbytes_of_dolby_ms12_downmix_output_pcm_frame()
{
    int pcm_out_chanenls = 2;
    int bytes_per_sample = 2;

    return pcm_out_chanenls*bytes_per_sample;
}


int dolby_ms12_main_flush(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    dolby_ms12_flush_main_input_buffer();
    if (ms12->ac3_parser_handle) {
        aml_ac3_parser_reset(ms12->ac3_parser_handle);
    }
    if (ms12->ms12_bypass_handle) {
        aml_ms12_bypass_reset(ms12->ms12_bypass_handle);
    }
    return 0;
}

void dolby_ms12_app_flush()
{
    dolby_ms12_flush_app_input_buffer();
}

void dolby_ms12_enable_debug()
{
    char buf[PROPERTY_VALUE_MAX];
    int level = 0;
    int ret = -1;

    ret = property_get("vendor.audio.dolbyms12.debug", buf, NULL);
    if (ret > 0)
    {
        level = atoi(buf);
        dolby_ms12_set_debug_level(level);
    }
}

