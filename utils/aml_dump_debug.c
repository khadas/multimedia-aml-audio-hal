/*
 * hardware/amlogic/audio/utils/aml_dump_debug.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include <aml_dump_debug.h>

#undef  LOG_TAG
#define LOG_TAG "aml_dump_debug"


void DoDumpData(const void *data_buf, int size, int aud_src_type) {
    int tmp_type = -1;
    char prop_value[PROPERTY_VALUE_MAX] = { 0 };
    char file_path[PROPERTY_VALUE_MAX] = { 0 };

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("media.audiohal.dumpdata.en", prop_value, "null");
    if (strcasecmp(prop_value, "null") == 0
            || strcasecmp(prop_value, "0") == 0) {
        return;
    }

    property_get("media.audiohal.dumpdata.src", prop_value, "null");
    if (strcasecmp(prop_value, "null") == 0
            || strcasecmp(prop_value, "input") == 0
            || strcasecmp(prop_value, "0") == 0) {
        tmp_type = CC_DUMP_SRC_TYPE_INPUT;
    } else if (strcasecmp(prop_value, "output") == 0
            || strcasecmp(prop_value, "1") == 0) {
        tmp_type = CC_DUMP_SRC_TYPE_OUTPUT;
    } else if (strcasecmp(prop_value, "input_parse") == 0
            || strcasecmp(prop_value, "2") == 0) {
        tmp_type = CC_DUMP_SRC_TYPE_INPUT_PARSE;
    }

    if (tmp_type != aud_src_type) {
        return;
    }

    memset(file_path, '\0', PROPERTY_VALUE_MAX);
    property_get("media.audiohal.dumpdata.path", file_path, "null");
    if (strcasecmp(file_path, "null") == 0) {
        file_path[0] = '\0';
    }
    FILE *fp1 = fopen(file_path, "a+");
    if (fp1) {
        int flen = fwrite((char *)data_buf, 1, size, fp1);
        if (flen > 0) {
            ALOGV("%s buffer %p size %d\n", __FUNCTION__, data_buf, size);
        } else {
            ALOGV("%s error flen %d\n", __FUNCTION__, flen);
        }
        fclose(fp1);
    }
    return;
}
