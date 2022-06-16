cmake_minimum_required(VERSION 3.8)

include(GNUInstallDirs)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)")
install(FILES libHwAudio_dtshd.so
	DESTINATION /usr/lib)
else ()
install(FILES libHwAudio_dtshd64.so
    DESTINATION /usr/lib RENAME libHwAudio_dtshd.so)
endif ()