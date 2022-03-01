cmake_minimum_required(VERSION 3.8)

include(GNUInstallDirs)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
install(FILES libHwAudio_dcvdec.so
    DESTINATION /usr/lib)
else ()
install(FILES libHwAudio_dcvdec64.so
    DESTINATION /usr/lib RENAME libHwAudio_dcvdec.so)
endif ()