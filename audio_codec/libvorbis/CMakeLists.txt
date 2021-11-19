cmake_minimum_required(VERSION 3.8)

include(GNUInstallDirs)

#############################################
# vorbis-ahal
#############################################
aux_source_directory(. SRC_FILES)
aux_source_directory(./Tremolo SRC_FILES)
message(WARNING "PROJECT_SOURCE_DIR is ${PROJECT_SOURCE_DIR}")
message(WARNING "SRC_FILES is ${SRC_FILES}")
message(WARNING "CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
    ENABLE_LANGUAGE(C ASM)
    list(APPEND ARM_FILES Tremolo/bitwiseARM.s)
    list(APPEND ARM_FILES Tremolo/dpen.s)
    list(APPEND ARM_FILES Tremolo/floor1ARM.s)
    list(APPEND ARM_FILES Tremolo/mdctARM.s)
else ()
    add_definitions(-DONLY_C)
endif ()

message(WARNING "ARM_FILES is ${ARM_FILES}")

add_library(vorbis-ahal SHARED
    ${SRC_FILES}
    ${ARM_FILES}
)

target_compile_options(vorbis-ahal PRIVATE -Werror -Wmissing-field-initializers -Wunused-parameter -Wunused-variable -Wsign-compare)

add_definitions(-DUSE_DEFAULT_STDLIB  -DUSE_VORBIS_DECODER)

#set_target_properties(vorbis-ahal PROPERTIES
#    VERSION ${PROJECT_VERSION}
#    SOVERSION 1)

target_include_directories(vorbis-ahal PRIVATE
    ${PROJECT_SOURCE_DIR}/audio_codec/libvorbis
    ${PROJECT_SOURCE_DIR}/audio_codec/libvorbis/Tremolo
    ${PROJECT_SOURCE_DIR}/amadec)

#target_link_libraries(vorbis-ahal
#    libutils libz libbinder libdl libcutils libc liblog)

install(TARGETS vorbis-ahal
    LIBRARY DESTINATION "lib")
