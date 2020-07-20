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

#include <stdio.h>
#include <unistd.h>
#include <cutils/log.h>
#include <sys/prctl.h>
#include <cutils/properties.h>
#include <dlfcn.h>

#include "dolby_lib_api.h"

#ifndef RET_OK
#define RET_OK 0
#endif

#ifndef RET_FAIL
#define RET_FAIL -1
#endif


#define DOLBY_MS12_LIB_PATH "/vendor/lib/libdolbyms12.so"
#define DOLBY_DCV_LIB_PATH_A "/vendor/lib/libHwAudio_dcvdec.so"


/*
 *@brief file_accessible
 */
static int file_accessible(char *path)
{
    // file is readable or not
    if (access(path, R_OK) == 0) {
        return RET_OK;
    } else {
        return RET_FAIL;
    }
}


/*
 *@brief detect_dolby_lib_type
 */
enum eDolbyLibType detect_dolby_lib_type(void)
{
    void *handle;

    handle = dlopen(DOLBY_MS12_LIB_PATH, RTLD_NOW);
    if (handle) {
        ALOGI("%s, FOUND libdolbyms12 lib", __FUNCTION__);
        dlclose(handle);
        return eDolbyMS12Lib;
    } else {
        ALOGI("%s, failed to detect libdolbyms12 lib: %s", __FUNCTION__, dlerror());
    }

    handle = dlopen(DOLBY_DCV_LIB_PATH_A, RTLD_NOW);
    if (handle) {
        ALOGI("%s, FOUND libHwAudio_dcvdec lib\n", __FUNCTION__); 
        dlclose(handle);
        return eDolbyDcvLib;
    } else {
        ALOGI("%s, failed to detect libHwAudio_dcvdec libi: %s", __FUNCTION__, dlerror());
    }

    ALOGI("%s, No Dolby libs are found", __FUNCTION__);
    return eDolbyNull;
}

