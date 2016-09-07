/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright (C) 2016 Kamil Trzciński
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

//
// This is based on hammerhead power HAL from Android 5.1
//

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <cutils/uevent.h>
#include <errno.h>
#include <sys/poll.h>
#include <pthread.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <stdbool.h>

#define LOG_TAG "PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

/* cpu spec files defined */
#define ROOMAGE     "/sys/devices/soc.0/cpu_budget_cool.16/roomage"
#define CPUHOT      "/sys/kernel/autohotplug/enable"
#define CPU0GOV     "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"

/* gpu spec files defined */
#define GPUFREQ     "/sys/devices/1c40000.gpu/dvfs/android"

/* ddr spec files defined */
#define DRAMFREQ    "/sys/class/devfreq/dramfreq/cur_freq"
#define DRAMPAUSE   "/sys/class/devfreq/dramfreq/adaptive/pause"

/*  value define */
#define ROOMAGE_PERF       "816000 4 0 0 1152000 4 0 0 0"
#define ROOMAGE_NORMAL     "0 4 0 0 1152000 4 0 0 0"
#define ROOMAGE_VIDEO      "0 4 0 0 1152000 4 0 0 0"
#define ROOMAGE_LOWPOWER   "0 1 0 0 1152000 4 0 0 0"

/* dram scene value defined */
#define DRAM_NORMAL         "0"
#define DRAM_HOME           "1"
#define DRAM_LOCALVIDEO     "2"
#define DRAM_BGMUSIC        "3"
#define DRAM_4KLOCALVIDEO   "4"

#define DRAM_PERF "1"
#define DRAM_AUTO "0"

/* gpu scene value defined */
#define GPU_NORMAL          "4\n"
#define GPU_HOME            "4\n"
#define GPU_LOCALVIDEO      "4\n"
#define GPU_BGMUSIC         "4\n"
#define GPU_4KLOCALVIDEO    "4\n"
#define GPU_PERF            "8\n"

#define CPUGOV_INTERACTIVE  "interactive"
#define CPUGOV_POWERSAVE    "powersave"

#define STATE_ON "state=1"
#define STATE_OFF "state=0"
#define STATE_HDR_ON "state=2"
#define STATE_HDR_OFF "state=3"

#define MAX_LENGTH         50

#define UEVENT_MSG_LEN 2048
#define TOTAL_CPUS 4
#define UEVENT_STRING "online@/devices/system/cpu/"

static int last_state = -1;

static struct pollfd pfd;
static bool low_power_mode = false;
static pthread_mutex_t low_power_mode_lock = PTHREAD_MUTEX_INITIALIZER;
static const char *saved_write[16];

static int sysfs_write(int index, const char *path, const char *s)
{
    if(saved_write[index] == s) {
      return 0;
    }
    saved_write[index] = s;

    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
      strerror_r(errno, buf, sizeof(buf));
      ALOGE("Error opening %s: %s\n", path, buf);
      return -1;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
      close(fd);
      strerror_r(errno, buf, sizeof(buf));
      ALOGE("Error writing %s to %s: %s:\n", s, path, buf);
      return -1;
    }

    close(fd);
    return 0;
}

static void set_state(const char *roomage, const char *cpu, const char *gpu)
{
  sysfs_write(0, ROOMAGE, roomage);
  sysfs_write(1, CPU0GOV, cpu);
  sysfs_write(2, GPUFREQ, gpu);
}

static int uevent_event()
{
    char msg[UEVENT_MSG_LEN];
    char *cp;
    int n, cpu, ret;

    n = recv(pfd.fd, msg, UEVENT_MSG_LEN, MSG_DONTWAIT);
    if (n <= 0) {
        return -1;
    }
    if (n >= UEVENT_MSG_LEN) {   /* overflow -- discard */
        return -1;
    }

    cp = msg;

    if (strstr(cp, UEVENT_STRING)) {
        n = strlen(cp);
        errno = 0;
        cpu = strtol(cp + n - 1, NULL, 10);

        if (errno == EINVAL || errno == ERANGE || cpu < 0 || cpu >= TOTAL_CPUS) {
            return -1;
        }

        pthread_mutex_lock(&low_power_mode_lock);
        // TODO: support this
        pthread_mutex_unlock(&low_power_mode_lock);
    }
    return 0;
}

void *thread_uevent(__attribute__((unused)) void *x)
{
    while (1) {
        int nevents, ret;

        nevents = poll(&pfd, 1, -1);

        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            ALOGE("powerhal: thread_uevent: poll_wait failed\n");
            break;
        }
        ret = uevent_event();
        if (ret < 0)
            ALOGE("Error processing the uevent event");
    }
    return NULL;
}

static void uevent_init()
{
    struct sockaddr_nl client;
    pthread_t tid;
    pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

    if (pfd.fd < 0) {
        ALOGE("%s: failed to open: %s", __func__, strerror(errno));
        return;
    }
    memset(&client, 0, sizeof(struct sockaddr_nl));
    pthread_create(&tid, NULL, thread_uevent, NULL);
    client.nl_family = AF_NETLINK;
    client.nl_pid = tid;
    client.nl_groups = -1;
    pfd.events = POLLIN;
    bind(pfd.fd, (void *)&client, sizeof(struct sockaddr_nl));
    return;
}

static void power_init(__attribute__((unused)) struct power_module *module)
{
    ALOGI("%s", __func__);
    uevent_init();
}

static void process_video_encode_hint(void *metadata)
{
    if (metadata) {
        if (!strncmp(metadata, STATE_ON, sizeof(STATE_ON))) {
          set_state(ROOMAGE_VIDEO, GPU_4KLOCALVIDEO, CPUGOV_INTERACTIVE);
        } else if (!strncmp(metadata, STATE_OFF, sizeof(STATE_OFF))) {
          set_state(ROOMAGE_NORMAL, GPU_NORMAL, CPUGOV_INTERACTIVE);
        }  else if (!strncmp(metadata, STATE_HDR_ON, sizeof(STATE_HDR_ON))) {
            /* HDR usecase started */
        } else if (!strncmp(metadata, STATE_HDR_OFF, sizeof(STATE_HDR_OFF))) {
            /* HDR usecase stopped */
        }else
            return;
    } else {
        return;
    }
}

static void power_set_interactive(__attribute__((unused)) struct power_module *module, int on)
{
    if (last_state == -1) {
        last_state = on;
    } else {
        if (last_state == on)
            return;
        else
            last_state = on;
    }

    ALOGV("%s %s", __func__, (on ? "ON" : "OFF"));
    if (on) {
      set_state(ROOMAGE_PERF, GPU_PERF, CPUGOV_INTERACTIVE);
    } else {
      set_state(ROOMAGE_NORMAL, GPU_NORMAL, CPUGOV_INTERACTIVE);
    }
}

static void power_hint( __attribute__((unused)) struct power_module *module,
                      power_hint_t hint, __attribute__((unused)) void *data)
{
    int cpu, ret;

    switch (hint) {
        case POWER_HINT_INTERACTION:
            ALOGV("POWER_HINT_INTERACTION");
            set_state(ROOMAGE_NORMAL, GPU_NORMAL, CPUGOV_INTERACTIVE);
            break;
#if 0
        case POWER_HINT_VSYNC:
            ALOGV("POWER_HINT_VSYNC %s", (data ? "ON" : "OFF"));
            break;
#endif
        case POWER_HINT_VIDEO_ENCODE:
            process_video_encode_hint(data);
            break;

        case POWER_HINT_LOW_POWER:
             pthread_mutex_lock(&low_power_mode_lock);
             set_state(ROOMAGE_LOWPOWER, GPU_NORMAL, CPUGOV_POWERSAVE);
             pthread_mutex_unlock(&low_power_mode_lock);
             break;
        default:
             break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_2,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "Tulip Power HAL",
        .author = "Kamil Trzciński <ayufan@ayufan.eu>",
        .methods = &power_module_methods,
    },

    .init = power_init,
    .setInteractive = power_set_interactive,
    .powerHint = power_hint,
};
