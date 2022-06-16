cmake_minimum_required(VERSION 3.8)

project(aml_audio_hal
	VERSION 1.0)

include(GNUInstallDirs)
execute_process(COMMAND sh version_config.sh WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory("libms12_v24")
add_subdirectory("audio_codec/libfaad")
add_subdirectory("audio_codec/libmad")
add_subdirectory("audio_codec/libvorbis")
add_subdirectory("audio_codec/libdts")
add_subdirectory("audio_codec/libdcv")
add_subdirectory("audio_hal/libaudioprocessing")

add_library(spdifenc STATIC
	utils/SPDIFEncoderAD.cpp
	utils/spdifenc_wrap.cpp
)

target_include_directories(spdifenc PRIVATE
	./audio_hal
	./include
	./include/utils
	./include/basic_utils
	./utils/tinyalsa/include
	./utils/include)

target_compile_options(spdifenc PRIVATE -Wno-deprecated-declarations -fPIC)

option(USE_DTV "Compile with DTV option" OFF)
option(NO_USE_CAP "Don't support audio capture" OFF)
option(USE_SC2 "Current platform is sc2" OFF)
option(NO_USE_CAP_MUTE_HDMI "Don't support audio capture mute for HDMI INPUT" OFF)
option(USE_APP_MIXING "Support appsound mixing" ON)
option(USE_DIAG_LOG "Support diagnostic logging for RDK" OFF)


if(USE_DTV)
    list(APPEND DTV_FILES dtv_audio_utils/audio_data_read/audio_dtv_ad.c)
    list(APPEND DTV_FILES audio_hal/audio_hw_dtv.c)
    list(APPEND DTV_FILES audio_hal/audio_dtv_utils.c)
    list(APPEND DTV_LIBS -L./amadec -laudamadec)
    list(APPEND DTV_LIBS -lam_adp -lasound)
    list(APPEND DVB_AUDIO_INC ./dtv_audio_utils/sync)
    list(APPEND DVB_AUDIO_INC ./dtv_audio_utils/dmxwrap/MultiHwDemux)
    list(APPEND DVB_AUDIO_INC ./amadec/include)
    list(APPEND DVB_AUDIO_LIBS -L./dtv_audio_utils -ldvbaudioutils)

    add_definitions(-DUSE_DTV)
endif()

if(USE_APP_MIXING)
add_definitions(-DUSE_APP_MIXING)
endif()

if(USE_DIAG_LOG)
add_definitions(-DDIAG_LOG)
endif()
option(USE_EQ_DRC "Compile with EQ/DRC" OFF)

if(USE_EQ_DRC)
	list(APPEND EQ_DRC_FILES
		amlogic_AQ_tools/audio_eq_drc_compensation.c
		amlogic_AQ_tools/audio_eq_drc_parser.c
		amlogic_AQ_tools/ini/dictionary.c
		amlogic_AQ_tools/ini/iniparser.c)
	add_definitions(-DUSE_EQ_DRC)
endif()

#############################################
# libaudio_hal
#############################################
file(GLOB BASIC_UTILS_FILES "utils/basic_utils/*.cpp")
file(GLOB BASIC_CUTILS_FILES "utils/basic_cutils/*.c")
add_library(audio_hal SHARED
	aml_adecs/aml_aac_dec_api.c
	aml_adecs/aml_ddp_dec_api.c
	aml_adecs/aml_dec_api.c
	aml_adecs/aml_dts_dec_api.c
	aml_adecs/aml_mpeg_dec_api.c
	aml_adecs/aml_pcm_dec_api.c
	aml_adecs/aml_flac_dec_api.c
	aml_adecs/aml_vorbis_dec_api.c
	aml_parser/ac3_parser_utils.c
	aml_parser/aml_ac3_parser.c
	aml_parser/aml_audio_ac3parser.c
	aml_parser/aml_audio_ac4parser.c
	aml_parser/aml_audio_bitsparser.c
	aml_parser/aml_audio_matparser.c
	aml_resampler/aml_audio_resample_manager.c
	aml_resampler/aml_audio_resampler.c
	aml_resampler/aml_resample_wrap.cpp
	aml_resampler/audio_android_resample_api.c
	aml_resampler/audio_simple_resample_api.c
	aml_speed/aml_audio_speed_manager.c
	aml_speed/audio_sonic_speed_api.c
	aml_speed/sonic_speed_wrapper.c
	aml_speed/sonic/sonic.c
	audio_hal/alsa_config_parameters.c
	audio_hal/alsa_manager.c
	audio_hal/audio_bt_sco.c
	audio_hal/aml_audio_delay.c
	#audio_hal/a2dp_hal.cpp
	audio_hal/aml_audio_ease.c
	audio_hal/amlAudioMixer.c
	audio_hal/aml_audio_ms12_bypass.c
	audio_hal/aml_audio_stream.c
    audio_hal/aml_audio_spdifout.c
	audio_hal/aml_audio_timer.c
	audio_hal/aml_avsync_tuning.c
	audio_hal/audio_format_parse.c
	audio_hal/audio_hw.c
	audio_hal/audio_hw_ms12_common.c
	#audio_hal/audio_hw_ms12.c  by default, we compile V2,V1 is not used now. TBD
	audio_hal/audio_hw_ms12_v2.c
	audio_hal/aml_audio_ms12_sync.c
	audio_hal/audio_hw_profile.c
	audio_hal/audio_route.c
	audio_hal/audio_hwsync.c
	audio_hal/audio_hwsync_wrap.c
	audio_hal/audio_hw_utils.c
	audio_hal/audio_port.c
	audio_hal/audio_post_process.c
	audio_hal/audio_virtual_buf.c
	audio_hal/dolby_lib_api.c
	audio_hal/hw_avsync.c
	audio_hal/hw_avsync_callbacks.c
	audio_hal/spdif_encoder_api.c
	audio_hal/sub_mixing_factory.c
	audio_hal/aml_audio_dev2mix_process.c
	audio_hal/aml_audio_ms12_render.c
	audio_hal/aml_audio_nonms12_render.c
	audio_hal/aml_audio_spdifdec.c
	audio_hal/aml_dtvsync.c
	audio_hal/aml_esmode_sync.c
	audio_hal/audio_mediasync_wrap.c
	audio_hal/audio_hdmi_util.c
	audio_hal/aml_audio_hal_avsync.c
	audio_hal/tinyalsa_ext.c
	audio_hal/aml_config_parser.c
	audio_hal/aml_config_data.c
	audio_hal/audio_effect_if.c
	utils/cJSON/cJSON.c
	utils/aml_hw_mixer.c
	utils/alsa_device_parser.c
	utils/aml_alsa_mixer.c
	utils/aml_android_utils.c
	utils/aml_audio_mixer.c
	utils/aml_buffer_provider.c
	utils/aml_data_utils.c
	utils/aml_dump_debug.c
	utils/aml_malloc_debug.c
	utils/aml_ringbuffer.c
	utils/aml_volume_utils.c
	utils/aml_configs/aml_conf_loader.c
	utils/aml_configs/aml_conf_parser.c
	utils/audio_data_process.c
	utils/tinyalsa/pcm.c
	utils/tinyalsa/mixer.c
	${BASIC_UTILS_FILES}
	${BASIC_CUTILS_FILES}
	#rcaudio/audio_hal_thunks.cpp
    ${DTV_FILES}
    ${EQ_DRC_FILES}
)

add_definitions(-DNDEBUG -DREPLACE_OUTPUT_BUFFER_WITH_CALLBACK -DDOLBY_MS12_ENABLE -DMS12_V24_ENABLE -DBUILD_LINUX)
if(NO_USE_CAP)
add_definitions(-DNO_AUDIO_CAP)

if(NO_USE_CAP_MUTE_HDMI)
	add_definitions(-DNO_AUDIO_CAP_MUTE_HDMI)
endif()
endif()

message(WARNING "Audio_hal CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
set(PREBUILD_STATIC_LIBS
	${CMAKE_SOURCE_DIR}/amlogic_AQ_tools/lib_aml_ng.a
	${CMAKE_SOURCE_DIR}/amlogic_AQ_tools/Amlogic_DRC_Param_Generator.a
	${CMAKE_SOURCE_DIR}/amlogic_AQ_tools/Amlogic_EQ_Param_Generator.a)
else ()
set(PREBUILD_STATIC_LIBS
	${CMAKE_SOURCE_DIR}/amlogic_AQ_tools/lib_aml_ng64.a
	${CMAKE_SOURCE_DIR}/amlogic_AQ_tools/Amlogic_DRC_Param_Generator64.a
	${CMAKE_SOURCE_DIR}/amlogic_AQ_tools/Amlogic_EQ_Param_Generator64.a)
endif ()

set_target_properties(audio_hal PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION 1)

target_include_directories(audio_hal PRIVATE
	./include
	../hal_audio_service/include
	./include/utils
	./include/basic_utils
	./include/basic_utils/utils
	./aml_speed/include
	./aml_speed
	./aml_resampler/include
	./aml_resampler
	./aml_adecs/include
	./aml_adecs/
	./aml_parser/include
	./aml_parser
	./utils/include
	./utils/tinyalsa/include
	./utils/cJSON
	./libms12_v24/include
	./amlogic_AQ_tools
	./dtv_audio_utils/audio_read_api
	./audio_hal
	./amadec)

target_compile_options(audio_hal PRIVATE -D_GNU_SOURCE -DTV_AUDIO_OUTPUT -Wno-deprecated-declarations -funwind-tables -fasynchronous-unwind-tables -fPIC)

target_link_libraries(audio_hal
	audioprocessing spdifenc ms12v2
	-lcutils -lamaudioutils -llog -lexpat -lpthread -ldl -lstdc++ -Wl,-z,defs)

if(USE_EQ_DRC)
	target_include_directories(audio_hal PRIVATE
		./include/amlogic_AQ_tools
		./amlogic_AQ_tools
		./amlogic_AQ_tools/ini)
	target_link_libraries(audio_hal ${PREBUILD_STATIC_LIBS})
	target_compile_options(audio_hal PRIVATE -DAML_EQ_DRC)
endif()

if(USE_DTV)
	target_include_directories(audio_hal PRIVATE
		${DTV_INC}
		${DVB_AUDIO_INC})
	target_link_libraries(audio_hal
		${DTV_LIBS}
		${DVB_AUDIO_LIBS})
endif()


target_link_libraries(audio_hal -lamlavsync)


if(USE_SC2)
	add_definitions(-DDVB_AUDIO_SC2)
endif()

install(TARGETS audio_hal
	LIBRARY DESTINATION "lib")

install(FILES audio_hal/mixer_paths.xml
	DESTINATION /etc)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
install(PROGRAMS amlogic_AQ_tools/Amlogic_DRC_Param_Generator amlogic_AQ_tools/Amlogic_EQ_Param_Generator
	DESTINATION /usr/bin)
endif ()
