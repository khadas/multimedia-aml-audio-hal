cmake_minimum_required(VERSION 3.8)

project(aml_audio_hal
	VERSION 1.0)

include(GNUInstallDirs)
if (DEFINED AML_BUILD_DIR)
	message("AML_BUILD_DIR is defined.")
else()
	message("AML_BUILD_DIR is not defined, set default.")
	set(AML_BUILD_DIR "./audio_hal")
endif()

execute_process(COMMAND sh version_config.sh ${AML_BUILD_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory("audio_codec/libvorbis")
add_subdirectory("audio_codec/libdts")
add_subdirectory("audio_codec/libdcv")
add_subdirectory("utils/libaudioprocessing")

add_library(spdifenc STATIC
	utils/SPDIFEncoderAD.cpp
	utils/spdifenc_wrap.cpp
	utils/spdif/AC3FrameScanner.cpp
	utils/spdif/BitFieldParser.cpp
	utils/spdif/DTSFrameScanner.cpp
	utils/spdif/FrameScanner.cpp
    utils/spdif/MatFrameScanner.cpp
	utils/spdif/SPDIFEncoder.cpp
)

target_include_directories(spdifenc PRIVATE
	./audio_hal
	./include
	./include/utils
	./include/basic_utils
	./utils/tinyalsa/include
	./utils/include
	./utils/include/spdif
	./utils/spdif)

target_compile_options(spdifenc PRIVATE -Wno-deprecated-declarations -fPIC)

option(DISABLE_SERVER "rm audio server" OFF)
option(USE_DTV "Compile with DTV option" OFF)
option(NO_USE_CAP "Don't support audio capture" OFF)
option(USE_SC2 "Current platform is sc2" OFF)
option(NO_USE_CAP_MUTE_HDMI "Don't support audio capture mute for HDMI INPUT" OFF)
option(USE_APP_MIXING "Support appsound mixing" ON)
option(USE_DIAG_LOG "Support diagnostic logging for RDK" OFF)
option(USE_MEDIA_INFO "Support media info API reporting" OFF)
option(USE_MS12_V26 "Use MS12 v2.6" OFF)
option(USE_MSYNC "Compile with MSYNC option" ON)
option(USE_DTS_VX4 "Use DTS VX4" ON)

if(USE_MS12_V26)
add_subdirectory("decoder/libms12_v26")
add_definitions(-DMS12_V26_ENABLE)
else()
add_subdirectory("decoder/libms12_v24")
add_definitions(-DMS12_V24_ENABLE)
endif()

#For DTS VX version selected
if(USE_DTS_VX4)
    message("Use DTS VX4")
    add_definitions(-DDTS_VX_V4_ENABLE)
endif()

if(DISABLE_SERVER)
    add_definitions(-DNO_SERVER)
endif()

if(USE_DTV)
    list(APPEND DTV_FILES input/dtv_patch.c)
    list(APPEND DTV_FILES input/dtv_patch_utils.c)
    list(APPEND DVB_AUDIO_INC ./dtv_audio_utils/sync)
    list(APPEND DVB_AUDIO_INC ./dtv_audio_utils/dmxwrap/MultiHwDemux)
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
		aml_aq_hw/audio_eq_drc_compensation.c
		aml_aq_hw/audio_eq_drc_parser.c
		aml_aq_hw/ini/dictionary.c
		aml_aq_hw/ini/iniparser.c)
	add_definitions(-DUSE_EQ_DRC)
endif()

if(USE_MEDIA_INFO)
    add_definitions(-DUSE_MEDIAINFO)
endif()

if(USE_MSYNC)
    add_definitions(-DUSE_MSYNC)
endif()

#############################################
# libaudio_hal
#############################################
file(GLOB BASIC_UTILS_FILES "utils/basic_utils/*.cpp")
file(GLOB BASIC_CUTILS_FILES "utils/basic_cutils/*.c")
add_library(audio_hal SHARED
	decoder/aml_aac_dec_api.c
	decoder/aml_ddp_dec_api.c
	decoder/aml_dec_api.c
	decoder/aml_dtshd_dec_api.c
	decoder/aml_dtsx_dec_api.c
	decoder/aml_mpeg_dec_api.c
	decoder/aml_pcm_dec_api.c
	decoder/aml_adpcm_dec_api.c
	decoder/aml_flac_dec_api.c
	decoder/aml_vorbis_dec_api.c
	utils/parser/ac3_parser_utils.c
	utils/parser/aml_ac3_parser.c
	utils/parser/aml_audio_ac3parser.c
	utils/parser/aml_audio_ac4parser.c
	utils/parser/aml_audio_bitsparser.c
	utils/parser/aml_audio_matparser.c
	utils/parser/aml_audio_heaacparser.c
	utils/resampler/aml_audio_resample_manager.c
	utils/resampler/aml_audio_resampler.c
	utils/resampler/aml_resample_wrap.cpp
	utils/resampler/audio_android_resample_api.c
	utils/resampler/audio_simple_resample_api.c
	utils/speed/aml_audio_speed_manager.c
	utils/speed/audio_sonic_speed_api.c
	utils/speed/sonic_speed_wrapper.c
	utils/speed/sonic/sonic.c
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
	input/tv_patch_avsync.c
	input/tv_patch_format_parser.c
	audio_hal/audio_hw.c
	audio_hal/audio_hw_ms12_common.c
	audio_hal/audio_hw_ms12_v2.c
	audio_hal/aml_audio_ms12_sync.c
	audio_hal/audio_hw_profile.c
	audio_hal/audio_route.c
	audio_hal/audio_hwsync.c
	audio_hal/audio_hw_utils.c
	audio_hal/audio_port.c
	audio_hal/audio_virtual_buf.c
	audio_hal/dolby_lib_api.c
	audio_hal/hw_avsync.c
	audio_hal/spdif_encoder_api.c
	audio_hal/aml_audio_dev2mix_process.c
	audio_hal/aml_audio_ms12_render.c
	audio_hal/aml_audio_nonms12_render.c
	audio_hal/aml_esmode_sync.c
	audio_hal/audio_mediasync_wrap.c
	input/hdmirx_utils.c
	input/dtv_patch_hal_avsync.c
	input/tv_patch.c
	input/tv_patch_ctrl.c
	audio_hal/aml_config_parser.c
	audio_hal/aml_config_data.c
	audio_hal/earc_utils.c
	audio_hal/hal_clipmeta.c
	audio_hal/aml_audio_scaletempo.c
	utils/cJSON/cJSON.c
	utils/aml_hw_mixer.c
	utils/alsa_device_parser.c
	utils/aml_alsa_mixer.c
	utils/aml_android_utils.c
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
	utils/channels.c
	utils/aml_audio_spdifdec.c
	utils/aml_avsync_stub.c
	vendor_process/audio_post_process.c
	vendor_process/audio_effect_if.c
	vendor_process/aml_audio_aec.c
	${BASIC_UTILS_FILES}
	${BASIC_CUTILS_FILES}
	#rcaudio/audio_hal_thunks.cpp
    ${DTV_FILES}
    ${EQ_DRC_FILES}
)

add_definitions(-DNDEBUG -DREPLACE_OUTPUT_BUFFER_WITH_CALLBACK -DDOLBY_MS12_ENABLE -DBUILD_LINUX)
if(NO_USE_CAP)
add_definitions(-DNO_AUDIO_CAP)

if(NO_USE_CAP_MUTE_HDMI)
	add_definitions(-DNO_AUDIO_CAP_MUTE_HDMI)
endif()
endif()

message(WARNING "Audio_hal CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
set(PREBUILD_STATIC_LIBS
	${CMAKE_SOURCE_DIR}/aml_aq_hw/lib_aml_ng.a
	${CMAKE_SOURCE_DIR}/aml_aq_hw/Amlogic_DRC_Param_Generator.a
	${CMAKE_SOURCE_DIR}/aml_aq_hw/Amlogic_EQ_Param_Generator.a)
else ()
set(PREBUILD_STATIC_LIBS
	${CMAKE_SOURCE_DIR}/aml_aq_hw/lib_aml_ng64.a
	${CMAKE_SOURCE_DIR}/aml_aq_hw/Amlogic_DRC_Param_Generator64.a
	${CMAKE_SOURCE_DIR}/aml_aq_hw/Amlogic_EQ_Param_Generator64.a)
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
	./utils/speed/include
	./utils/speed
	./utils/resampler/include
	./utils/resampler
	./decoder
	./utils/parser/include
	./utils/parser
	./utils/include
	./utils/include/spdif
	./utils/tinyalsa/include
	./utils/cJSON
	./vendor_process/include
	./input/include
	./aml_aq_hw
	./dtv_audio_utils/audio_read_api
	./audio_hal
	${AML_BUILD_DIR})

target_compile_options(audio_hal PRIVATE -D_GNU_SOURCE -DTV_AUDIO_OUTPUT -Wno-deprecated-declarations -funwind-tables -fasynchronous-unwind-tables -fPIC -g)

target_link_libraries(audio_hal
	audioprocessing spdifenc ms12v2
	-lcutils -lamaudioutils -llog -lexpat -lpthread -ldl -lstdc++ -lrt -Wl,-z,defs)

if(USE_MS12_V26)
	target_include_directories(audio_hal PRIVATE ./decoder/libms12_v26/include)
else()
	target_include_directories(audio_hal PRIVATE ./decoder/libms12_v24/include)
endif()

if(USE_EQ_DRC)
	target_include_directories(audio_hal PRIVATE
		./include/aml_aq_hw
		./aml_aq_hw
		./aml_aq_hw/ini)
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

if(USE_MEDIA_INFO)
	target_link_libraries(audio_hal -lamlminfo)
endif()

if(USE_MSYNC)
target_link_libraries(audio_hal -lamlavsync)
else()
target_link_libraries(audio_hal)
endif()

if(USE_SC2)
	add_definitions(-DDVB_AUDIO_SC2)
endif()

install(TARGETS audio_hal
	LIBRARY DESTINATION "lib")

install(FILES audio_hal/mixer_paths.xml
	DESTINATION /etc)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
install(PROGRAMS aml_aq_hw/Amlogic_DRC_Param_Generator aml_aq_hw/Amlogic_EQ_Param_Generator
	DESTINATION /usr/bin)
endif ()
