/*
 * Copyright (C) 2019 Amlogic Corporation.
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
#define LOG_TAG "config_data"


#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include "aml_config_data.h"
#include <cutils/log.h>


#define AML_AUDIO_CONFIG_FILE_PATH "/etc/halaudio/aml_audio_config.json"
cJSON *audio_config_jason = NULL;

int aml_audio_config_parser()
{
    audio_config_jason = aml_config_parser(AML_AUDIO_CONFIG_FILE_PATH);
    if (audio_config_jason) {
        return 0;
    } else {
        return -1;
    }
}

int aml_get_jason_int_value(char* key,int defvalue)
{
    cJSON *temp = NULL;
    int value = defvalue;
    if (audio_config_jason) {
        temp = cJSON_GetObjectItem(audio_config_jason, key);
        aml_printf_cJSON(key, temp);
        if (temp) {
            value = temp->valueint;
        }
    }
    return value;
}

