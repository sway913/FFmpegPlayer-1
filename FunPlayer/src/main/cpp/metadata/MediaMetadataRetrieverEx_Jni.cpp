#include <unistd.h>
#include <jni.h>
#include <AndroidLog.h>
#include <string.h>

#include "MediaMetadataRetriever.h"
#include "../player/android/JniHelper.h"

extern "C" {
#include <libavcodec/jni.h>
};

#define JNI_CLASS_RETRIEVER     "com/ffmpeg/media/MediaMetadataRetrieverEx"
#define JNI_CLASS_INIT          "<init>"
#define JNI_CLASS_MAP_OBJ       "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"

static jfieldID mjfieldID;
static Mutex sLock;

static jstring charTojstring(JNIEnv* env, const char* ptr)
{
    jclass cls = env->FindClass("java/lang/String");
    jmethodID mId = env->GetMethodID(cls, JNI_CLASS_INIT, "([BLjava/lang/String;)V");
    jbyteArray bytes = env->NewByteArray((jsize)strlen(ptr));
    env->SetByteArrayRegion(bytes, 0, (jsize)strlen(ptr), (jbyte*)ptr);
    jstring encoding = env->NewStringUTF("UTF-8");
    jstring str = (jstring)env->NewObject(cls, mId, bytes, encoding);
    env->DeleteLocalRef(encoding);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(cls);
    return str;
}

static void throwException(JNIEnv* env, const char* name, const char* msg)
{
    jclass cls = env->FindClass(name);
    /* if cls is NULL, an exception has already been thrown */
    if (cls != NULL)
    {
        env->ThrowNew(cls, msg);
    }
    /* free the local ref */
    env->DeleteLocalRef(cls);
}

static int getFDFromFileDescriptor(JNIEnv* env, jobject obj)
{
    jint fd = -1;

    jclass cls = env->FindClass("java/io/FileDescriptor");
    if (cls != NULL)
    {
        jfieldID mId = env->GetFieldID(cls, "descriptor", "I");
        if (mId != NULL && obj != NULL)
        {
            fd = env->GetIntField(obj, mId);
        }
    }
    env->DeleteLocalRef(cls);
    return fd;
}

static void process_media_retriever_call(JNIEnv* env, status_t opStatus, const char* exception, const char* message)
{
    if (opStatus == (status_t)INVALID_OPERATION)
    {
        throwException(env, "java/lang/IllegalStateException", NULL);
    }
    else if (opStatus != (status_t)OK)
    {
        if (strlen(message) > 230)
        {
            throwException(env, exception, message);
        }
        else
        {
            char msg[256] = {'\0'};
            sprintf(msg, "%s: status = 0x%X", message, opStatus);
            throwException(env, exception, msg);
        }
    }
}

static MediaMetadataRetriever *getRetriever(JNIEnv* env, jobject thiz)
{
    MediaMetadataRetriever *retriever = (MediaMetadataRetriever *) env->GetLongField(thiz, mjfieldID);
    return retriever;
}

static void setRetriever(JNIEnv *env, jobject thiz, long retriever)
{
    env->GetLongField(thiz, mjfieldID);
    env->SetLongField(thiz, mjfieldID, retriever);
}

static void MediaMetadataRetriever_setDataSourceAndHeaders(JNIEnv *env, jobject thiz, jstring path_, jobjectArray keys, jobjectArray values)
{
    ALOGV("setDataSource");
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return;
    }

    if (!path_)
    {
        throwException(env, "java/lang/IllegalArgumentException", "Null pointer");
        return;
    }

    const char *path = env->GetStringUTFChars(path_, NULL);
    if (!path)
    {
        return;
    }

    if (strncmp("mem://", path, 6) == 0)
    {
        throwException(env, "java/lang/IllegalArgumentException", "Invalid pathname");
        return;
    }

    const char *restrict = strstr(path, "mms://");
    char *restrict_to = restrict ? strdup(restrict) : NULL;
    if (restrict_to != NULL)
    {
        strncpy(restrict_to, "mmsh://", 6);
        puts(path);
    }

    char *headers = NULL;
    if (keys && values != NULL)
    {
        int keysCount = env->GetArrayLength(keys);
        int valuesCount = env->GetArrayLength(values);

        if (keysCount != valuesCount)
        {
            ALOGE("keys and values arrays have different length");
            throwException(env, "java/lang/IllegalArgumentException", NULL);
            return;
        }

        const char *rawString = NULL;
        char hdrs[2048] = {'\0'};

        for (int i = 0; i < keysCount; i++)
        {
            jstring key = (jstring) env->GetObjectArrayElement(keys, i);
            rawString = env->GetStringUTFChars(key, NULL);
            strcat(hdrs, rawString);
            strcat(hdrs, ": ");
            env->ReleaseStringUTFChars(key, rawString);

            jstring value = (jstring) env->GetObjectArrayElement(values, i);
            rawString = env->GetStringUTFChars(value, NULL);
            strcat(hdrs, rawString);
            strcat(hdrs, "\r\n");
            env->ReleaseStringUTFChars(value, rawString);
        }

        headers = &hdrs[0];
    }

    status_t opStatus = retriever->setDataSource(path, 0, headers);
    process_media_retriever_call(env, opStatus, "java/io/IOException", "setDataSource failed.");

    env->ReleaseStringUTFChars(path_, path);
}

static void MediaMetadataRetriever_setDataSource(JNIEnv *env, jobject thiz, jstring path_)
{
    MediaMetadataRetriever_setDataSourceAndHeaders(env, thiz, path_, NULL, NULL);
}

static void MediaMetadataRetriever_setDataSourceFD(JNIEnv *env, jobject thiz, jobject fileDescriptor, jlong offset, jlong length)
{
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return;
    }

    if (!fileDescriptor)
    {
        throwException(env, "java/lang/IllegalStateException", NULL);
        return;
    }

    int fd = getFDFromFileDescriptor(env, fileDescriptor);
    if (offset < 0 || length < 0 || fd < 0)
    {
        if (offset < 0)
        {
            ALOGE("negative offset (%lld)", offset);
        }
        if (length < 0)
        {
            ALOGE("negative length (%lld)", length);
        }
        if (fd < 0)
        {
            ALOGE("invalid file descriptor");
        }
        throwException(env, "java/lang/IllegalArgumentException", NULL);
        return;
    }

    char path[256]{'\0'};
    char str[20]{'\0'};

    int myfd = dup(fd);
    sprintf(str, "pipe:%d", myfd);
    strcat(path, str);

    status_t opStatus = retriever->setDataSource(path, offset, NULL);
    process_media_retriever_call(env, opStatus, "java/io/IOException", "setDataSourceFD failed.");
}

static jbyteArray MediaMetadataRetriever_getFrameAtTime(JNIEnv *env, jobject thiz, jlong timeUs, jint option)
{

    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;

    if (retriever->getFrame(timeUs, &packet) == 0)
    {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);

        jbyte *bytes = env->GetByteArrayElements(array, NULL);
        if (bytes != NULL)
        {
            memcpy(bytes, data, size);
            env->ReleaseByteArrayElements(array, bytes, 0);
        }
    }
    av_packet_unref(&packet);

    return array;
}

static jbyteArray MediaMetadataRetriever_getScaleFrameAtTime(JNIEnv *env, jobject thiz, jlong timeUs, jint option, jint width, jint height)
{

    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;
    if (retriever->getFrame(timeUs, &packet, width, height) == 0)
    {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);

        jbyte *bytes = env->GetByteArrayElements(array, NULL);
        if (bytes != NULL)
        {
            memcpy(bytes, data, size);
            env->ReleaseByteArrayElements(array, bytes, 0);
        }
    }
    av_packet_unref(&packet);
    return array;
}

static jbyteArray MediaMetadataRetriever_getEmbeddedPicture(JNIEnv *env, jobject thiz, jint pictureType)
{

    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVPacket packet;
    av_init_packet(&packet);
    jbyteArray array = NULL;
    if (retriever->getEmbeddedPicture(&packet) == 0)
    {
        int size = packet.size;
        uint8_t *data = packet.data;
        array = env->NewByteArray(size);

        jbyte *bytes = env->GetByteArrayElements(array, NULL);
        if (bytes != NULL)
        {
            memcpy(bytes, data, size);
            env->ReleaseByteArrayElements(array, bytes, 0);
        }
    }
    av_packet_unref(&packet);
    return array;
}

static jstring MediaMetadataRetriever_extractMetadata(JNIEnv *env, jobject thiz, jstring key_)
{

    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    if (!key_)
    {
        throwException(env, "java/lang/IllegalArgumentException", "Null pointer");
        return NULL;
    }

    const char *key = env->GetStringUTFChars(key_, 0);
    if (!key)
    {
        return NULL;
    }

    const char *value = retriever->getMetadata(key);
    if (!value)
    {
        return NULL;
    }

    env->ReleaseStringUTFChars(key_, key);

    return charTojstring(env, value);
}

static jstring MediaMetadataRetriever_extractMetadataFromChapter(JNIEnv *env, jobject thiz, jstring key_, jint chapter)
{
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    if (!key_)
    {
        throwException(env, "java/lang/IllegalArgumentException", "Null pointer");
        return NULL;
    }

    const char *key = env->GetStringUTFChars(key_, 0);
    if (!key)
    {
        return NULL;
    }

    const char *value = retriever->getMetadata(key, chapter);
    if (!value)
    {
        return NULL;
    }

    env->ReleaseStringUTFChars(key_, key);

    return charTojstring(env, value);
}

static jobject MediaMetadataRetriever_getAllMetadata(JNIEnv *env, jobject thiz)
{
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    if (retriever == NULL)
    {
        throwException(env, "java/lang/IllegalStateException", "No retriever available");
        return NULL;
    }

    AVDictionary *metadata = NULL;
    int ret = retriever->getMetadata(&metadata);
    if (ret == 0)
    {
        jclass cls = env->FindClass("java/util/HashMap");
        jmethodID init = env->GetMethodID(cls, JNI_CLASS_INIT, "()V");
        jobject obj = env->NewObject(cls, init);
        jmethodID put = env->GetMethodID(cls, "put", JNI_CLASS_MAP_OBJ);
        for (int i = 0; i < metadata->count; ++i)
        {
            jstring key = charTojstring(env, metadata->elements[i].key);
            jstring value = charTojstring(env, metadata->elements[i].value);
            env->CallObjectMethod(obj, put, key, value);
        }

        if (metadata)
        {
            av_dict_free(&metadata);
        }

        return obj;
    }

    return NULL;
}

static void MediaMetadataRetriever_setup(JNIEnv *env, jobject thiz)
{
    MediaMetadataRetriever* retriever = new MediaMetadataRetriever();
    if (retriever == NULL)
    {
        throwException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }

    setRetriever(env, thiz, (long)retriever);
}

static void MediaMetadataRetriever_release(JNIEnv *env, jobject thiz)
{
    Mutex::Autolock lock(sLock);
    MediaMetadataRetriever *retriever = getRetriever(env, thiz);
    delete retriever;
    setRetriever(env, thiz, 0);
}

static void MediaMetadataRetriever_native_finalize(JNIEnv *env, jobject thiz)
{
    ALOGV("native_finalize");
    MediaMetadataRetriever_release(env, thiz);
}

static void MediaMetadataRetriever_native_init(JNIEnv *env)
{
    jclass cls = env->FindClass(JNI_CLASS_RETRIEVER);
    if (cls == NULL)
    {
        return;
    }
    mjfieldID = env->GetFieldID(cls, "mNativeContext", "J");
    if (mjfieldID == NULL)
    {
        return;
    }
    env->DeleteLocalRef(cls);
}

static JNINativeMethod g_methods[] = {
        {
            "_setDataSource",
            "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",
            (void *)MediaMetadataRetriever_setDataSourceAndHeaders
        },
        {"setDataSource",                   "(Ljava/lang/String;)V",                    (void *)MediaMetadataRetriever_setDataSource},
        {"setDataSource",                   "(Ljava/io/FileDescriptor;JJ)V",            (void *)MediaMetadataRetriever_setDataSourceFD},
        {"_getFrameAtTime",                 "(JI)[B",                                   (void *)MediaMetadataRetriever_getFrameAtTime},
        {"_getScaledFrameAtTime",           "(JIII)[B",                                 (void *)MediaMetadataRetriever_getScaleFrameAtTime},
        {"getEmbeddedPicture",              "(I)[B",                                    (void *)MediaMetadataRetriever_getEmbeddedPicture},
        {"extractMetadata",                 "(Ljava/lang/String;)Ljava/lang/String;",   (void *)MediaMetadataRetriever_extractMetadata},
        {"extractMetadataFromChapter",      "(Ljava/lang/String;I)Ljava/lang/String;",  (void *)MediaMetadataRetriever_extractMetadataFromChapter},
        {"_getAllMetadata",                 "()Ljava/util/HashMap;",                    (void *)MediaMetadataRetriever_getAllMetadata},
        {"release",                         "()V",                                      (void *)MediaMetadataRetriever_release},
        {"native_setup",                    "()V",                                      (void *)MediaMetadataRetriever_setup},
        {"native_init",                     "()V",                                      (void *)MediaMetadataRetriever_native_init},
        {"native_finalize",                 "()V",                                      (void *)MediaMetadataRetriever_native_finalize},

};

static int media_meta_data_retriever_register(JNIEnv *env)
{
    jclass clazz = env->FindClass(JNI_CLASS_RETRIEVER);
    if (clazz == NULL)
    {
        ALOGE("Native registration unable to find class '%s'", JNI_CLASS_RETRIEVER);
        return JNI_ERR;
    }

    if (env->RegisterNatives(clazz, g_methods, NELEM(g_methods)) < 0)
    {
        ALOGE("Native registration unable to find class '%s'", JNI_CLASS_RETRIEVER);
        return JNI_ERR;
    }

    env->DeleteLocalRef(clazz);

    return JNI_OK;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    av_jni_set_java_vm(vm, NULL);

    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        return JNI_ERR;
    }

    if (media_meta_data_retriever_register(env) != JNI_OK)
    {
        return JNI_ERR;
    }

    return JNI_VERSION_1_4;
}