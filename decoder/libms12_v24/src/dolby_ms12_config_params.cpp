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

#define LOG_TAG "audio_hw_decoder_ms12v2"
// #define LOG_NDEBUG 0
// #define LOG_NALOGV 0

#include <cutils/log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <system/audio.h>
#include <utils/String8.h>
#include <errno.h>
#include <fcntl.h>
//#include <media/AudioSystem.h>

#include "DolbyMS12ConfigParams.h"

static android::DolbyMS12ConfigParams *gInstance = NULL;
static android::Mutex mLock;
static android::DolbyMS12ConfigParams* getInstance()
{
    ALOGV("+%s() config params\n", __FUNCTION__);
    android::Mutex::Autolock autoLock(mLock);

    if (gInstance == NULL) {
        gInstance = new android::DolbyMS12ConfigParams();
    }
    ALOGV("-%s() config params gInstance %p\n", __FUNCTION__, gInstance);
    if (gInstance) {
        return gInstance;
    } else {
        return NULL;
    }
}

#if 0
extern "C" void dolby_ms12_config_params_self_cleanup(void)
{
    ALOGV("%s()\n", __FUNCTION__);

    android::Mutex::Autolock autoLock(mLock);

    if (gInstance) {
        ALOGV("+%s() gInstance %p\n", __FUNCTION__, gInstance);
        delete gInstance;
        gInstance = NULL;
    }
    ALOGV("-%s() gInstance %p\n", __FUNCTION__, gInstance);
}
#endif

extern "C" void dolby_ms12_config_params_set_associate_flag(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAssociateFlag(flag);
    }
}

extern "C" bool dolby_ms12_config_params_get_associate_flag(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getAssociateFlag();
    } else {
        return false;
    }
}

extern "C" void dolby_ms12_config_params_set_system_flag(bool flag)
{
    ALOGV("%s() system flag %d\n", __FUNCTION__, flag);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setSystemFlag(flag);
    }
}

extern "C" bool dolby_ms12_config_params_get_system_flag(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getSystemFlag();
    } else {
        return false;
    }
}

extern "C" void dolby_ms12_config_params_set_app_flag(bool flag)
{
    ALOGV("%s() system flag %d\n", __FUNCTION__, flag);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAppFlag(flag);
    }
}

extern "C" bool dolby_ms12_config_params_get_app_flag(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getAppFlag();
    } else {
        return false;
    }
}

extern "C" void dolby_ms12_config_params_set_audio_stream_out_params(
    audio_output_flags_t flags
    , audio_format_t input_format
    , audio_channel_mask_t channel_mask
    , int sample_rate
    , int output_config)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->SetAudioStreamOutParams(flags, input_format, channel_mask, sample_rate, output_config);
    }
}

extern "C" void dolby_ms12_config_params_set_channel_mask_app(audio_channel_mask_t mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setChannelMaskOfAppSoundsInput(mask);
    }
}

extern "C" void dolby_ms12_config_params_set_channel_mask_system(audio_channel_mask_t mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setChannelMaskOfSystemSoundsInput(mask);
    }
}

extern "C" void dolby_ms12_config_params_set_channel_mask_ui(audio_channel_mask_t mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setChannelMaskOfUiSoundsInput(mask);
    }
}

/*config params begin*/
/**/
extern "C" int dolby_ms12_config_params_set_input_output_file_name(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetInputOutputFileName(ConfigParams, row_index);
    } else {
        return -1;
    }
}

extern "C" int dolby_ms12_config_params_set_functional_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetFunctionalSwitches(ConfigParams, row_index);
    } else {
        return -1;
    }
}

#if 0
extern "C" int dolby_ms12_config_params_set_ddplus_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetDdplusSwitches(ConfigParams, row_index);
    } else {
        return -1;
    }
}
#endif

extern "C" int dolby_ms12_config_params_set_ac4_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetAc4Switches(ConfigParams, row_index);
    } else {
        return -1;
    }
}

extern "C" int dolby_ms12_config_params_set_pcm_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetPCMSwitches(ConfigParams, row_index);
    } else {
        return -1;
    }
}

extern "C" int dolby_ms12_config_params_set_heaac_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetHEAACSwitches(ConfigParams, row_index);
    } else {
        return -1;
    }
}

extern "C" int dolby_ms12_config_params_set_dap_device_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetDAPDeviceSwitches(ConfigParams, row_index, 0);
    } else {
        return -1;
    }
}

extern "C" int dolby_ms12_config_params_set_dap_content_switches(char **ConfigParams, int *row_index)
{
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->SetDAPContentSwitches(ConfigParams, row_index);
    } else {
        return -1;
    }
}
/**/
/*config params end*/

extern "C" char *dolby_ms12_query_dap_parameters(const char *key)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->QueryDapParameters(key);
    } else {
        return NULL;
    }
}
extern "C" char **dolby_ms12_config_params_get_config_params(int *argc)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->GetDolbyMS12ConfigParams(argc);
    } else {
        return NULL;
    }
}

#if 0
extern "C" char **dolby_ms12_config_params_get_runtime_config_params(int *argc)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->GetDolbyMS12RuntimeConfigParams(argc);
    } else {
        return NULL;
    }
}
#endif

extern "C" char **dolby_ms12_config_params_update_runtime_config_params(int *argc, char *cmd)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->UpdateDolbyMS12RuntimeConfigParams(argc, cmd);
    } else {
        return NULL;
    }
}

#if 0
extern "C" char **dolby_ms12_config_params_get_runtime_config_params_lite(int *argc)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->GetDolbyMS12RuntimeConfigParams_lite(argc);
    } else {
        return NULL;
    }
}
#endif

extern "C" char **dolby_ms12_config_params_prepare_config_params(int max_raw_size, int max_column_size)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->PrepareConfigParams(max_raw_size, max_column_size);
    } else {
        return NULL;
    }
}

extern "C" void dolby_ms12_config_params_cleanup_config_params(char **ConfigParams, int max_raw_size)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->CleanupConfigParams(ConfigParams, max_raw_size);
    }
}
extern "C" int dolby_ms12_config_params_get_dolby_config_output_config(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->GetDolbyConfigOutputConfig();
    } else {
        return 0;
    }
}

extern "C" int dolby_ms12_config_params_get_dolby_config_output_samplerate(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->GetDolbyConfigOutputSampleRate();
    } else {
        return -1;
    }
}

extern "C" audio_channel_mask_t dolby_ms12_config_params_get_dolby_config_output_channelmask(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->GetDolbyConfigOutputChannelMask();
    } else {
        return (audio_channel_mask_t)-1;
    }
}

extern "C" void dolby_ms12_config_params_reset_init_config_params(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->ResetInitConfigParams();
    }
}

extern "C" void dolby_ms12_config_params_reset_runtime_config_params(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->ResetRuntimeConfigParams();
    }
}

/*****************************************************************************************************************/
/*Begin*/
//*Begin||Add the APT to set the params*//
//Functional Switches
//virtual void setLowComplexityNode(bool flag) { mLowComplexityMode = flag; }//did not use this part!
/*****************************************************************************************************************/
extern "C" void dolby_ms12_set_drc_boost(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDRCboostVal(val);
    }
}

extern "C" void dolby_ms12_set_drc_cut(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDRCcutVal(val);
    }
}

extern "C" void dolby_ms12_set_drc_boost_stereo(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDRCboostStereoVal(val);
    }
}

extern "C" void dolby_ms12_set_drc_cut_stereo(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDRCcutStereoVal(val);
    }
}

extern "C" void dolby_ms12_set_channel_mask_of_app_sound_input(audio_channel_mask_t channel_mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setChannelMaskOfAppSoundsInput(channel_mask);
    }
}

extern "C" void dolby_ms12_set_channel_mask_of_system_sound_input(audio_channel_mask_t channel_mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setChannelMaskOfSystemSoundsInput(channel_mask);
    }
}

extern "C" void dolby_ms12_set_channel_mask_of_ui_sound_input(audio_channel_mask_t channel_mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setChannelMaskOfUiSoundsInput(channel_mask);
    }
}

extern "C" void dolby_ms12_set_dap2_initialisation_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPV2InitialisationMode(val);
    }
}

extern "C" void dolby_ms12_set_dap2_virtual_bass_enable(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPV2VirtualBassEnable(flag);
    }
}

extern "C" void dolby_ms12_set_drc_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDRCMode(val);
    }
}

extern "C" void dolby_ms12_set_dap_drc_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPDRCMode(val);
    }
}

extern "C" void dolby_ms12_set_downmix_modes(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDownmixModes(val);
    }
}

extern "C" void dolby_ms12_set_evaluation_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setEvaluationMode(val);
    }
}


extern "C" void dolby_ms12_set_donwmix_51_pcm_to_51_on_multi_outputs(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDownmix71PCMto51OnMultiOutputs(val);
    }
}

extern "C" void dolby_ms12_set_encoder_channel_mode_locking_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setEncoderChannelModeLockingMode(val);
    }
}

extern "C" int dolby_ms12_get_encoder_channel_mode_locking_mode()
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getEncoderChannelModeLockingMode();
    }

    return 0;
}

extern "C" void dolby_ms12_set_risc_precision_flag(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setRISCprecisionFlag(val);
    }
}

extern "C" void dolby_ms12_set_dual_mono_reproduction_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDualmonoReproductionMode(val);
    }
}

extern "C" void dolby_ms12_set_associated_audio_mixing(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAssociatedAudioMixing(val);
    }
}

extern "C" void dolby_ms12_set_system_app_audio_mixing(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setSystemAppAudioMixing(val);
    }
}

extern "C" int dolby_ms12_get_system_app_audio_mixing(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getSystemAppAudioMixing();
    } else {
        return false;
    }
}

extern "C" void dolby_ms12_set_user_control_value_for_mixing_main_and_associated_audio(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setUserControlValuesForMixingMainAndAssociatedAudio(val);
    }
}

extern "C" void dolby_ms12_set_input_mixer_gain_values_for_main_program_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setInputMixerGainValuesForMainProgramInput(mixergain);
    }
}

extern "C" void dolby_ms12_set_input_mixer_gain_values_for_2nd_main_program_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setInputMixerGainValuesFor2ndMainProgramInput(mixergain);
    }
}

extern "C" void dolby_ms12_set_input_mixer_gain_values_for_ui_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->setInputMixerGainValuesForUIInput(mixergain);
    }
}

extern "C" void dolby_ms12_set_system_sound_mixer_gain_values_for_primary_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setSystemSoundMixerGainValuesForPrimaryInput(mixergain);
    }
}

extern "C" void dolby_ms12_set_system_sound_mixer_gain_values_for_app_sounds_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setSystemSoundMixerGainValuesForAppSoundsInput(mixergain);
    }
}

extern "C" void dolby_ms12_set_system_sound_mixer_gain_values_for_system_sounds_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setSystemSoundMixerGainValuesForSystemSoundsInput(mixergain);
    }
}

//DDPLUS SWITCHES
#if 0
extern "C" void dolby_ms12_set_ddp_associated_substream_selection(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDDPAssociatedSubstreamSelection(val);
    }
}
#endif

//PCM SWITCHES

extern "C" void dolby_ms12_set_pcm_compressor_profile(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setPCMCompressorProfile(val);
    }
}

//AC4 SWITCHES
extern "C" void dolby_ms12_set_ac4_lang(char *str)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4Lang(str);
    }
}

extern "C" void dolby_ms12_set_ac4_lang2(char *str)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4Lang2(str);
    }
}

extern "C" void dolby_ms12_set_ac4_ac(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4Ac(val);
    }
}

extern "C" void dolby_ms12_set_ac4_pat(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4Pat(val);
    }
}

extern "C" void dolby_ms12_set_ac4_presgroupidx(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4PresGroupIdx(val);
    }
}

extern "C" void dolby_ms12_set_ac4_de(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4De(val);
    }
}

extern "C" void dolby_ms12_set_ac4_shortprogid(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAC4ShortProgId(val);
    }
}

//HE-AAC SWITCHES
extern "C" void dolby_ms12_set_heaac_associated_instance_restricted_to_2channels(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setHEAACAssociatedInstanceRestrictedTo2Channels(val);
    }
}

extern "C" void dolby_ms12_set_heaac_default_dialnorm_value(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setHEAACDefaultDialnormValue(val);
    }
}

extern "C" void dolby_ms12_set_heaac_dualmono_reproduction_mode(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDualMonoreproductionMode(val);
    }
}

extern "C" void dolby_ms12_set_heaac_arib_channel_mapping_flag(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAribChannelMappingFlag(val);
    }
}


//DAP SWITCHES (device specific)
extern "C" void dolby_ms12_set_dap_gains(int val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPGains(val);
    }
}

extern "C" void dolby_ms12_set_dap_surround_decoder_enable(bool val)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPSurroundDecoderEnable(val);
    }
}


extern "C" void dolby_ms12_set_dap_surround_virtualizer(DAPSurroundVirtualizer *dapVirtualizerParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPSurroundVirtualizer(dapVirtualizerParameters);
    }
}


extern "C" int dolby_ms12_get_dap_surround_virtualizer(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getDAPSurroundVirtualizer();
    } else {
        return -1;
    }
}


extern "C" void dolby_ms12_set_dap_graphic_eq(DAPGraphicEQ *dapGraphicEQParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPGraphicEQ(dapGraphicEQParameters);
    }
}

extern "C" void dolby_ms12_set_dap_bass_enhancer(DAPBassEnhancer *dapBassEnhancerParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPBassEnhancer(dapBassEnhancerParameters);
    }
}

extern "C" void dolby_ms12_set_dap_virtual_bass(DAPVirtualBass *dapVirtualBassParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPVirtualBass(dapVirtualBassParameters);
    }
}


//DAP SWITCHES (content specific)
extern "C" void dolby_ms12_set_dap_mi_steering(DAPMISteering *dapMiSteeringParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPMISteering(dapMiSteeringParameters);
    }
}

extern "C" void dolby_ms12_set_dap_leveler(DAPLeveler *dapLevelerParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPLeveler(dapLevelerParameters);
    }
}

extern "C" void dolby_ms12_set_dap_ieq(DAPIEQ *dapIEQParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPIEQ(dapIEQParameters);
    }
}

extern "C" void dolby_ms12_set_dap_dialogue_enhancer(DAPDialogueEnhancer *dapDialogueEnhancerParameters)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDAPDialogueEnhancer(dapDialogueEnhancerParameters);
    }
}

extern "C" void dolby_ms12_set_dual_output_flag(bool need_dual_output)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDualOutputFlag(need_dual_output);
    }
}

extern "C" void set_dolby_ms12_continuous_mode(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setActiveOTTSignalFlag(flag);
    }
}

extern "C" bool is_dolby_ms12_continuous_mode(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getActiveOTTSignalFlag();
    } else {
        return false;
    }
}

extern "C" void dolby_ms12_set_input_mixer_gain_values_for_ott_sounds_input(MixGain *mixergain)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setInputMixerGainValuesForOTTSoundsInput(mixergain);
    }
}

extern "C" void dolby_ms12_set_atmos_lock_flag(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setAtmosLockFlag(flag);
    }
}


extern "C" int dolby_ms12_get_atmos_lock_flag(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getAtmosLockFlag();
    } else {
        return false;
    }
}

extern "C" void dolby_ms12_set_pause_flag(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setPauseFlag(flag);
    }
}

extern "C" void dolby_ms12_set_dolby_main1_as_dummy_file(bool is_dummy)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDolbyMain1FileNameAsDummy(is_dummy);
    }
}

extern "C" void dolby_ms12_set_dolby_main2_as_dummy_file(bool is_dummy)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setDolbyMain2NameAsDummy(is_dummy);
    }
}

extern "C" bool dolby_ms12_get_dolby_main1_file_is_dummy(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getMain1IsDummy();
    } else {
        ALOGI("%s() ret false\n", __FUNCTION__);
        return false;
    }
}

extern "C" void dolby_ms12_set_ott_sound_input_enable(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setOTTSoundInputEnable(flag);
    }
}

extern "C" bool dolby_ms12_get_ott_sound_input_enable(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getOTTSoundInputEnable();
    } else {
        ALOGI("%s() ret false\n", __FUNCTION__);
        return false;
    }
}

extern "C" void dolby_ms12_set_ddp_5_1_out(bool flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setLegacyDDPOut(flag);
    }
    else {
        ALOGW("Found the config_param handle illegal\n");
    }
}


extern "C" bool dolby_ms12_get_ddp_5_1_out(void)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->getLegacyDDPOut();
    } else {
        ALOGW("Found the config_param handle illegal\n");
        return false;
    }
}


extern "C" void dolby_ms12_set_enforce_timeslice(bool is_enforce)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->setEnforceTimeslice(is_enforce);
    }
}

extern "C" void dolby_ms12_set_tv_tuning_flag(bool tv_tuning_flag)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->setTVTuningFlag(tv_tuning_flag);
    }
}

extern "C" void dolby_ms12_set_hdmi_output_type(int hdmi_output_type)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        config_param->setHDMIOutoutType(hdmi_output_type);
    }
    else {
        ALOGW("Found the config_param handle illegal\n");
    }
}

extern "C" int dolby_ms12_get_channel_config(audio_channel_mask_t channel_mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->ChannelMask2ChannelConfig(channel_mask);
    }
    else {
        ALOGW("Found the config_param handle illegal\n");
    }
    return -1;
}

extern "C" int dolby_ms12_get_lfe_config(audio_channel_mask_t channel_mask)
{
    ALOGV("%s()\n", __FUNCTION__);
    android::DolbyMS12ConfigParams *config_param = getInstance();
    if (config_param) {
        return config_param->ChannelMask2LFEConfig(channel_mask);
    }
    else {
        ALOGW("Found the config_param handle illegal\n");
    }
    return -1;
}


/*****************************************************************************************************************/
/*END*/
/*****************************************************************************************************************/
