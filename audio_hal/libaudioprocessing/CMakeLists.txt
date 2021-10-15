#############################################
# libaudioprocessing.a
#############################################
add_library(audioprocessing STATIC
	AudioResampler.cpp
	AudioResamplerSinc.cpp
	AudioResamplerCubic.cpp
	AudioResamplerDyn.cpp
)

target_compile_options(audioprocessing PRIVATE -fPIC)

target_include_directories(audioprocessing PRIVATE
	${PROJECT_SOURCE_DIR}/audio_hal
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/include/utils
	${PROJECT_SOURCE_DIR}/include/basic_utils)
