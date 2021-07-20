cmake_minimum_required(VERSION 3.8)

include(GNUInstallDirs)

#############################################
# libms12v2
#############################################
add_library(ms12v2 SHARED
	src/aml_audio_ms12.c
	src/dolby_ms12_config_params.cpp
	src/DolbyMS12ConfigParams.cpp
	src/dolby_ms12.cpp
	src/DolbyMS12.cpp
	src/dolby_ms12_status.cpp
	src/DolbyMS12Status.cpp
)

target_compile_options(ms12v2 PRIVATE -fPIC)

add_definitions(-DNDEBUG -DREPLACE_OUTPUT_BUFFER_WITH_CALLBACK -DUSE_MSYNC)

set_target_properties(ms12v2 PROPERTIES
	VERSION ${PROJECT_VERSION}
	SOVERSION 1)

target_include_directories(ms12v2 PRIVATE
	./include
	${PROJECT_SOURCE_DIR}/include
	${PROJECT_SOURCE_DIR}/include/utils
	${PROJECT_SOURCE_DIR}/utils/include)

target_link_libraries(ms12v2
	-lcutils -llog -lpthread -ldl -lstdc++ -lbinder)

install(TARGETS ms12v2
	LIBRARY DESTINATION "lib")
