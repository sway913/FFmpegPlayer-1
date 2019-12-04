#ifndef NATIVE_LOG_H
#define NATIVE_LOG_H

#include <android/log.h>

#define JNI_TAG "MediaPlayer"

#if defined(__ANDROID__)

#define ALOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, JNI_TAG, format, ##__VA_ARGS__)
#define ALOGI(format, ...) __android_log_print(ANDROID_LOG_INFO,  JNI_TAG, format, ##__VA_ARGS__)
#define ALOGD(format, ...) __android_log_print(ANDROID_LOG_DEBUG, JNI_TAG, format, ##__VA_ARGS__)
#define ALOGW(format, ...) __android_log_print(ANDROID_LOG_WARN,  JNI_TAG, format, ##__VA_ARGS__)
#define ALOGV(format, ...) __android_log_print(ANDROID_LOG_VERBOSE,  JNI_TAG, format, ##__VA_ARGS__)
#else

#define ALOGE(format, ...) {}
#define ALOGI(format, ...) {}
#define ALOGD(format, ...) {}
#define ALOGW(format, ...) {}
#define ALOGV(format, ...) {}
#endif

#define SAFE_DELETE(x) 	            { if (x) { delete x; x = NULL; } }
#define	SAFE_FREE(p)				{ if (p) { free((p)); (p) = NULL; } }
#define	SAFE_DELETE_ARRAY(p)		{ if (p) { delete [](p); (p) = NULL; } }
#define	NUM_ARRAY_ELEMENTS(p)		{ ((int) sizeof(p) / sizeof(p[0])) }

#endif //NATIVE_LOG_H
