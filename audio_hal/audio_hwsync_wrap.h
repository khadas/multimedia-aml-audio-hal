/*
 * Copyright (C) 2010 Amlogic Corporation.
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



#ifndef _AUDIO_HWSYNC_WRAP_H_
#define _AUDIO_HWSYNC_WRAP_H_

#include <stdbool.h>
#include "audio_hwsync.h"

#define TSYNC_FIRSTAPTS "/sys/class/tsync/firstapts"
#define TSYNC_FIRSTVPTS "/sys/class/tsync/firstvpts"
#define TSYNC_PCRSCR    "/sys/class/tsync/pts_pcrscr"
#define TSYNC_EVENT     "/sys/class/tsync/event"
#define TSYNC_APTS      "/sys/class/tsync/pts_audio"
#define TSYNC_VPTS      "/sys/class/tsync/pts_video"
#define TSYNC_ENABLE    "/sys/class/tsync/enable"
#define TSYNC_MODE      "/sys/class/tsync/mode"
#define TSYNC_VSTARTED  "/sys/class/tsync/videostarted"

void* aml_hwsync_wrap_mediasync_create (void);

#endif
