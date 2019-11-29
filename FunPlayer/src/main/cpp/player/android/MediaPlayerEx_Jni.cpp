#include <jni.h>
#include <Mutex.h>
#include <Condition.h>
#include <Errors.h>
#include <JniHelper.h>
#include <MediaPlayerEx.h>

extern "C" {
#include <libavcodec/jni.h>
}

#define JNI_CLASS_MEDIA_PLAYER "com/ffmpeg/media/MediaPlayerEx"

struct fields_t
{
    jfieldID context;
    jmethodID post_event;
};

static fields_t fields;
static JavaVM *javaVM = NULL;

static JNIEnv *getJNIEnv()
{
    JNIEnv *env;
    assert(javaVM != NULL);
    if (javaVM->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        return NULL;
    }
    return env;
}

class JniMediaPlayerListener : public MediaPlayerListener
{
public:
    JniMediaPlayerListener() = delete;

    JniMediaPlayerListener(JNIEnv *env, jobject thiz, jobject weak_thiz);

    ~JniMediaPlayerListener();

    void notify(int msg, int ext1, int ext2, void *obj) override;

private:
    jclass mClass;
    jobject mObject;
};

JniMediaPlayerListener::JniMediaPlayerListener(JNIEnv *env, jobject thiz, jobject weak_thiz)
{
    // Hold onto the MediaPlayer class for use in calling the static method
    // that posts events to the application thread.
    jclass clazz = env->GetObjectClass(thiz);
    if (clazz == NULL)
    {
        ALOGE("Can't find com/cgfay/media/MediaPlayerEx");
        jniThrowException(env, "java/lang/Exception");
        return;
    }
    mClass = (jclass) env->NewGlobalRef(clazz);

    // We use a weak reference so the MediaPlayer object can be garbage collected.
    // The reference is only used as a proxy for callbacks.
    mObject = env->NewGlobalRef(weak_thiz);
}

JniMediaPlayerListener::~JniMediaPlayerListener()
{
    JNIEnv *env = getJNIEnv();
    env->DeleteGlobalRef(mObject);
    env->DeleteGlobalRef(mClass);
}

void JniMediaPlayerListener::notify(int msg, int ext1, int ext2, void *obj)
{
    JNIEnv *env = getJNIEnv();

    bool status = (javaVM->AttachCurrentThread(&env, NULL) >= 0);

    // TODO obj needs changing into jobject
    env->CallStaticVoidMethod(mClass, fields.post_event, mObject, msg, ext1, ext2, obj);

    if (env->ExceptionCheck())
    {
        ALOGW("An exception occurred while notifying an event.");
        env->ExceptionClear();
    }

    if (status)
    {
        javaVM->DetachCurrentThread();
    }
}

static MediaPlayerEx *getMediaPlayer(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *const mp = (MediaPlayerEx *) env->GetLongField(thiz, fields.context);
    return mp;
}

static MediaPlayerEx *setMediaPlayer(JNIEnv *env, jobject thiz, long mediaPlayer)
{
    MediaPlayerEx *old = (MediaPlayerEx *) env->GetLongField(thiz, fields.context);
    env->SetLongField(thiz, fields.context, mediaPlayer);
    return old;
}

// If exception is NULL and opStatus is not OK, this method sends an error
// event to the client application; otherwise, if exception is not NULL and
// opStatus is not OK, this method throws the given exception to the client
// application.
static void process_media_player_call(JNIEnv *env, jobject thiz, int opStatus, const char *exception, const char *message)
{
    if (exception == NULL)
    {  // Don't throw exception. Instead, send an event.
        if (opStatus != (int) OK)
        {
            MediaPlayerEx *mp = getMediaPlayer(env, thiz);
            if (mp != 0) mp->notify(MEDIA_ERROR, opStatus, 0);
        }
    }
    else
    {  // Throw exception!
        if (opStatus == (int) INVALID_OPERATION)
        {
            jniThrowException(env, "java/lang/IllegalStateException");
        }
        else if (opStatus == (int) PERMISSION_DENIED)
        {
            jniThrowException(env, "java/lang/SecurityException");
        }
        else if (opStatus != (int) OK)
        {
            if (strlen(message) > 230)
            {
                // if the message is too long, don't bother displaying the status code
                jniThrowException(env, exception, message);
            }
            else
            {
                char msg[256] = {'\0'};
                // append the status code to the message
                sprintf(msg, "%s: status=0x%X", message, opStatus);
                jniThrowException(env, exception, msg);
            }
        }
    }
}

void MediaPlayerEx_setDataSourceAndHeaders(JNIEnv *env, jobject thiz, jstring path_, jobjectArray keys, jobjectArray values)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }

    if (path_ == NULL)
    {
        jniThrowException(env, "java/lang/IllegalArgumentException");
        return;
    }

    const char *path = env->GetStringUTFChars(path_, 0);
    if (path == NULL)
    {
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
            jniThrowException(env, "java/lang/IllegalArgumentException");
            return;
        }

        const char *rawString = NULL;
        char hdrs[2048]{'\0'};

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

    status_t opStatus = mp->setDataSource(path, 0, headers);
    process_media_player_call(env, thiz, opStatus, "java/io/IOException", "setDataSource failed.");

    env->ReleaseStringUTFChars(path_, path);
}

void MediaPlayerEx_setDataSource(JNIEnv *env, jobject thiz, jstring path_)
{
    MediaPlayerEx_setDataSourceAndHeaders(env, thiz, path_, NULL, NULL);
}

void MediaPlayerEx_setDataSourceFD(JNIEnv *env, jobject thiz, jobject fileDescriptor, jlong offset, jlong length)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }

    if (fileDescriptor == NULL)
    {
        jniThrowException(env, "java/lang/IllegalArgumentException");
        return;
    }

    int fd = jniGetFDFromFileDescriptor(env, fileDescriptor);
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
        jniThrowException(env, "java/lang/IllegalArgumentException");
        return;
    }

    char path[256] {'\0'};
    char str[32] {'\0'};
    int myfd = dup(fd);
    sprintf(str, "pipe:%d", myfd);
    strcat(path, str);

    status_t opStatus = mp->setDataSource(path, offset, NULL);
    process_media_player_call(env, thiz, opStatus, "java/io/IOException", "setDataSourceFD failed.");
}

void MediaPlayerEx_init(JNIEnv *env)
{
    jclass clazz = env->FindClass(JNI_CLASS_MEDIA_PLAYER);
    if (clazz == NULL)
    {
        return;
    }

    fields.context = env->GetFieldID(clazz, "mNativeContext", "J");
    if (fields.context == NULL)
    {
        return;
    }

    fields.post_event = env->GetStaticMethodID(clazz, "postEventFromNative",
            "(Ljava/lang/Object;IIILjava/lang/Object;)V");
    if (fields.post_event == NULL)
    {
        return;
    }
    env->DeleteLocalRef(clazz);
}

void MediaPlayerEx_setup(JNIEnv *env, jobject thiz, jobject mediaplayer_this)
{
    MediaPlayerEx *mp = new MediaPlayerEx();
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/RuntimeException", "Out of memory");
        return;
    }

    // 这里似乎存在问题
    // init MediaPlayerEx
    mp->init();

    // create new listener and give it to MediaPlayer
    JniMediaPlayerListener *listener = new JniMediaPlayerListener(env, thiz, mediaplayer_this);
    mp->setListener(listener);

    // Stow our new C++ MediaPlayer in an opaque field in the Java object.
    setMediaPlayer(env, thiz, (long) mp);
}

void MediaPlayerEx_release(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp != nullptr)
    {
        mp->disconnect();
        delete mp;
        setMediaPlayer(env, thiz, 0);
    }
}

void MediaPlayerEx_reset(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->reset();
}

void MediaPlayerEx_finalize(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp != NULL)
    {
        ALOGW("MediaPlayer finalized without being released");
    }
    MediaPlayerEx_release(env, thiz);
}

void MediaPlayerEx_setVideoSurface(JNIEnv *env, jobject thiz, jobject surface)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    ANativeWindow *window = NULL;
    if (surface != NULL)
    {
        window = ANativeWindow_fromSurface(env, surface);
    }
    mp->setVideoSurface(window);
}

void MediaPlayerEx_setLooping(JNIEnv *env, jobject thiz, jboolean looping)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->setLooping(looping);
}

jboolean MediaPlayerEx_isLooping(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return JNI_FALSE;
    }
    return (jboolean) (mp->isLooping() ? JNI_TRUE : JNI_FALSE);
}

void MediaPlayerEx_prepare(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }

    mp->prepare();
}

void MediaPlayerEx_prepareAsync(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->prepareAsync();
}

void MediaPlayerEx_start(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->start();
}

void MediaPlayerEx_pause(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->pause();
}

void MediaPlayerEx_resume(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->resume();

}

void MediaPlayerEx_stop(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->stop();
}

void MediaPlayerEx_seekTo(JNIEnv *env, jobject thiz, jfloat timeMs)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->seekTo(timeMs);
}

void MediaPlayerEx_setMute(JNIEnv *env, jobject thiz, jboolean mute)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->setMute(mute);
}

void MediaPlayerEx_setVolume(JNIEnv *env, jobject thiz, jfloat leftVolume, jfloat rightVolume)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->setVolume(leftVolume, rightVolume);
}

void MediaPlayerEx_setRate(JNIEnv *env, jobject thiz, jfloat speed)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->setRate(speed);
}

void MediaPlayerEx_setPitch(JNIEnv *env, jobject thiz, jfloat pitch)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    mp->setPitch(pitch);
}

jlong MediaPlayerEx_getCurrentPosition(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return 0L;
    }
    return mp->getCurrentPosition();
}

jlong MediaPlayerEx_getDuration(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return 0L;
    }
    return mp->getDuration();
}

jboolean MediaPlayerEx_isPlaying(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return JNI_FALSE;
    }
    return (jboolean) (mp->isPlaying() ? JNI_TRUE : JNI_FALSE);
}

jint MediaPlayerEx_getRotate(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return 0;
    }
    return mp->getRotate();
}

jint MediaPlayerEx_getVideoWidth(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return 0;
    }
    return mp->getVideoWidth();
}

jint MediaPlayerEx_getVideoHeight(JNIEnv *env, jobject thiz)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return 0;
    }
    return mp->getVideoHeight();
}

void MediaPlayerEx_setOption(JNIEnv *env, jobject thiz,
                               int category, jstring type_, jstring option_)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    const char *type = env->GetStringUTFChars(type_, 0);
    const char *option = env->GetStringUTFChars(option_, 0);
    if (type == NULL || option == NULL)
    {
        return;
    }

    mp->setOption(category, type, option);

    env->ReleaseStringUTFChars(type_, type);
    env->ReleaseStringUTFChars(option_, option);
}

void MediaPlayerEx_setOptionLong(JNIEnv *env, jobject thiz,
                                   int category, jstring type_, jlong option_)
{
    MediaPlayerEx *mp = getMediaPlayer(env, thiz);
    if (mp == NULL)
    {
        jniThrowException(env, "java/lang/IllegalStateException");
        return;
    }
    const char *type = env->GetStringUTFChars(type_, 0);
    if (type == NULL)
    {
        return;
    }
    mp->setOption(category, type, option_);

    env->ReleaseStringUTFChars(type_, type);
}

static const JNINativeMethod gMethods[] = {
        {
         "_setDataSource",
         "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;)V",
         (void *) MediaPlayerEx_setDataSourceAndHeaders
        },
        {"_setDataSource",      "(Ljava/lang/String;)V",                    (void *) MediaPlayerEx_setDataSource},
        {"_setDataSource",      "(Ljava/io/FileDescriptor;JJ)V",            (void *) MediaPlayerEx_setDataSourceFD},
        {"_setVideoSurface",    "(Landroid/view/Surface;)V",                (void *) MediaPlayerEx_setVideoSurface},
        {"_prepare",            "()V",                                      (void *) MediaPlayerEx_prepare},
        {"_prepareAsync",       "()V",                                      (void *) MediaPlayerEx_prepareAsync},
        {"_start",              "()V",                                      (void *) MediaPlayerEx_start},
        {"_stop",               "()V",                                      (void *) MediaPlayerEx_stop},
        {"_resume",             "()V",                                      (void *) MediaPlayerEx_resume},
        {"_getRotate",          "()I",                                      (void *) MediaPlayerEx_getRotate},
        {"_getVideoWidth",      "()I",                                      (void *) MediaPlayerEx_getVideoWidth},
        {"_getVideoHeight",     "()I",                                      (void *) MediaPlayerEx_getVideoHeight},
        {"_seekTo",             "(F)V",                                     (void *) MediaPlayerEx_seekTo},
        {"_pause",              "()V",                                      (void *) MediaPlayerEx_pause},
        {"_isPlaying",          "()Z",                                      (void *) MediaPlayerEx_isPlaying},
        {"_getCurrentPosition", "()J",                                      (void *) MediaPlayerEx_getCurrentPosition},
        {"_getDuration",        "()J",                                      (void *) MediaPlayerEx_getDuration},
        {"_release",            "()V",                                      (void *) MediaPlayerEx_release},
        {"_reset",              "()V",                                      (void *) MediaPlayerEx_reset},
        {"_setLooping",         "(Z)V",                                     (void *) MediaPlayerEx_setLooping},
        {"_isLooping",          "()Z",                                      (void *) MediaPlayerEx_isLooping},
        {"_setVolume",          "(FF)V",                                    (void *) MediaPlayerEx_setVolume},
        {"_setMute",            "(Z)V",                                     (void *) MediaPlayerEx_setMute},
        {"_setRate",            "(F)V",                                     (void *) MediaPlayerEx_setRate},
        {"_setPitch",           "(F)V",                                     (void *) MediaPlayerEx_setPitch},
        {"native_init",         "()V",                                      (void *) MediaPlayerEx_init},
        {"native_setup",        "(Ljava/lang/Object;)V",                    (void *) MediaPlayerEx_setup},
        {"native_finalize",     "()V",                                      (void *) MediaPlayerEx_finalize},
        {"_setOption",          "(ILjava/lang/String;Ljava/lang/String;)V", (void *) MediaPlayerEx_setOption},
        {"_setOption",          "(ILjava/lang/String;J)V",                  (void *) MediaPlayerEx_setOptionLong}
};

static int register_com_ffmpeg_media_MediaPlayerEx(JNIEnv *env)
{
    jclass cls = env->FindClass(JNI_CLASS_MEDIA_PLAYER);
    if (cls == NULL)
    {
        return JNI_ERR;
    }
    if (env->RegisterNatives(cls, gMethods, NELEM(gMethods)) < 0)
    {
        return JNI_ERR;
    }
    env->DeleteLocalRef(cls);
    return JNI_OK;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    av_jni_set_java_vm(vm, NULL);
    javaVM = vm;
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        return JNI_ERR;
    }
    if (register_com_ffmpeg_media_MediaPlayerEx(env) != JNI_OK)
    {
        return JNI_ERR;
    }
    return JNI_VERSION_1_4;
}