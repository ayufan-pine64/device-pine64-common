#ifndef __LOG_H__
#define __LOG_H__

#ifdef _ANDROID_
#include <android/log.h>

#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else // _ANDROID_
#include <stdio.h>

#define ALOG(fmt, ...) fprintf(stderr, "%s: " fmt "\n", LOG_TAG, ##__VA_ARGS__)
#define ALOGD ALOG
#define ALOGV ALOG
#define ALOGI ALOG
#define ALOGW ALOG
#define ALOGE ALOG
#endif // _ANDROID_

#endif // __LOG_H__
