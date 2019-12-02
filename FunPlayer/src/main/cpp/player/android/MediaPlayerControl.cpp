#include <player/AVMessageQueue.h>
#include "MediaPlayerControl.h"
#include "JniHelper.h"

MediaPlayerControl::MediaPlayerControl():
        mVideoDevice(nullptr),
    mMediaPlayerEx(nullptr),
    mListener(nullptr),
    abortRequest(true),
    mSeeking(false),
    mSeekingPosition(0),
    mPrepareSync(false),
    mPrepareStatus(NO_ERROR),
    mAudioSessionId(0)
{
}

MediaPlayerControl::~MediaPlayerControl()
{
}

void MediaPlayerControl::init()
{
    mMutex.lock();
    abortRequest = false;
    mMutex.unlock();

    mMutex.lock();

    mVideoDevice = new GLESDevice();

    mThread = std::thread(&MediaPlayerControl::run, this);

    mMutex.unlock();
}

void MediaPlayerControl::disconnect()
{
    mMutex.lock();
    abortRequest = true;
    mMutex.unlock();

    reset();

    mThread.join();

    SAFE_DELETE(mVideoDevice);
    SAFE_DELETE(mListener);
}

status_t MediaPlayerControl::setDataSource(const char *url, int64_t offset, const char *headers)
{
    if (url == nullptr)
    {
        return BAD_VALUE;
    }
    if (mMediaPlayerEx == nullptr)
    {
        mMediaPlayerEx = new MediaPlayerEx();
    }
    mMediaPlayerEx->setDataSource(url, offset, headers);
    mMediaPlayerEx->setVideoDevice(mVideoDevice);
    return NO_ERROR;
}

status_t MediaPlayerControl::setMetadataFilter(char **allow, char **block)
{
    // do nothing
    return NO_ERROR;
}

status_t MediaPlayerControl::getMetadata(bool update_only, bool apply_filter, AVDictionary **metadata)
{
    if (mMediaPlayerEx != nullptr)
    {
        return mMediaPlayerEx->getMetadata(metadata);
    }
    return NO_ERROR;
}

status_t MediaPlayerControl::setVideoSurface(ANativeWindow *native_window)
{
    if (mMediaPlayerEx == nullptr)
    {
        return NO_INIT;
    }
    if (native_window != nullptr)
    {
        mVideoDevice->surfaceCreated(native_window);
        return NO_ERROR;
    }
    return NO_ERROR;
}

status_t MediaPlayerControl::setListener(MediaPlayerListener *listener)
{
    SAFE_DELETE(mListener);
    mListener = listener;
    return NO_ERROR;
}

status_t MediaPlayerControl::prepare()
{
    if (mMediaPlayerEx == nullptr)
    {
        return NO_INIT;
    }
    if (mPrepareSync)
    {
        return -EALREADY;
    }
    mPrepareSync = true;
    status_t ret = mMediaPlayerEx->prepare();
    if (ret != NO_ERROR)
    {
        return ret;
    }
    if (mPrepareSync)
    {
        mPrepareSync = false;
    }
    return mPrepareStatus;
}

status_t MediaPlayerControl::prepareAsync()
{
    if (mMediaPlayerEx != nullptr)
    {
        return mMediaPlayerEx->prepareAsync();
    }
    return INVALID_OPERATION;
}

void MediaPlayerControl::start()
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->start();
    }
}

void MediaPlayerControl::stop()
{
    if (mMediaPlayerEx)
    {
        mMediaPlayerEx->stop();
    }
}

void MediaPlayerControl::pause()
{
    if (mMediaPlayerEx)
    {
        mMediaPlayerEx->pause();
    }
}

void MediaPlayerControl::resume()
{
    if (mMediaPlayerEx)
    {
        mMediaPlayerEx->resume();
    }
}

bool MediaPlayerControl::isPlaying()
{
    if (mMediaPlayerEx)
    {
        return (mMediaPlayerEx->isPlaying() != 0);
    }
    return false;
}

int MediaPlayerControl::getRotate()
{
    if (mMediaPlayerEx != nullptr)
    {
        return mMediaPlayerEx->getRotate();
    }
    return 0;
}

int MediaPlayerControl::getVideoWidth()
{
    if (mMediaPlayerEx != nullptr)
    {
        return mMediaPlayerEx->getVideoWidth();
    }
    return 0;
}

int MediaPlayerControl::getVideoHeight()
{
    if (mMediaPlayerEx != nullptr)
    {
        return mMediaPlayerEx->getVideoHeight();
    }
    return 0;
}

status_t MediaPlayerControl::seekTo(float msec)
{
    if (mMediaPlayerEx != nullptr)
    {
        // if in seeking state, put seek message in queue, to process after preview seeking.
        if (mSeeking)
        {
            mMediaPlayerEx->getMessageQueue()->postMessage(MSG_REQUEST_SEEK, msec);
        }
        else
        {
            mMediaPlayerEx->seekTo(msec);
            mSeekingPosition = (long) msec;
            mSeeking = true;
        }
    }
    return NO_ERROR;
}

long MediaPlayerControl::getCurrentPosition()
{
    if (mMediaPlayerEx != nullptr)
    {
        if (mSeeking)
        {
            return mSeekingPosition;
        }
        return mMediaPlayerEx->getCurrentPosition();
    }
    return 0;
}

long MediaPlayerControl::getDuration()
{
    if (mMediaPlayerEx != nullptr)
    {
        return mMediaPlayerEx->getDuration();
    }
    return -1;
}

status_t MediaPlayerControl::reset()
{
    mPrepareSync = false;
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->reset();
        delete mMediaPlayerEx;
        mMediaPlayerEx = nullptr;
    }
    return NO_ERROR;
}

status_t MediaPlayerControl::setAudioStreamType(int type)
{
    // TODO setAudioStreamType
    return NO_ERROR;
}

status_t MediaPlayerControl::setLooping(bool looping)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->setLooping(looping);
    }
    return NO_ERROR;
}

bool MediaPlayerControl::isLooping()
{
    if (mMediaPlayerEx != nullptr)
    {
        return (mMediaPlayerEx->isLooping() != 0);
    }
    return false;
}

status_t MediaPlayerControl::setVolume(float leftVolume, float rightVolume)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->setVolume(leftVolume, rightVolume);
    }
    return NO_ERROR;
}

void MediaPlayerControl::setMute(bool mute)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->setMute(mute);
    }
}

void MediaPlayerControl::setRate(float speed)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->setRate(speed);
    }
}

void MediaPlayerControl::setPitch(float pitch)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->setPitch(pitch);
    }
}

status_t MediaPlayerControl::setAudioSessionId(int sessionId)
{
    if (sessionId < 0)
    {
        return BAD_VALUE;
    }
    mAudioSessionId = sessionId;
    return NO_ERROR;
}

int MediaPlayerControl::getAudioSessionId()
{
    return mAudioSessionId;
}

void MediaPlayerControl::setOption(int category, const char *type, const char *option)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->getPlayerState()->setOption(category, type, option);
    }
}

void MediaPlayerControl::setOption(int category, const char *type, int64_t option)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->getPlayerState()->setOptionLong(category, type, option);
    }
}

void MediaPlayerControl::notify(int msg, int ext1, int ext2, void *obj, int len)
{
    if (mMediaPlayerEx != nullptr)
    {
        mMediaPlayerEx->getMessageQueue()->postMessage(msg, ext1, ext2, obj, len);
    }
}

void MediaPlayerControl::postEvent(int what, int arg1, int arg2, void *obj)
{
    if (mListener != nullptr)
    {
        mListener->notify(what, arg1, arg2, obj);
    }
}

void MediaPlayerControl::run()
{
    int retval;
    while (true)
    {
        if (abortRequest)
        {
            break;
        }

        // 如果此时播放器还没准备好，则睡眠10毫秒，等待播放器初始化
        if (!mMediaPlayerEx || !mMediaPlayerEx->getMessageQueue())
        {
            av_usleep(10 * 1000);
            continue;
        }

        AVMessage msg;
        retval = mMediaPlayerEx->getMessageQueue()->getMessage(&msg);
        if (retval < 0)
        {
            ALOGE("getMessage error");
            break;
        }

        assert(retval > 0);

        switch (msg.what)
        {
            case MSG_FLUSH:
            {
                ALOGD("MediaPlayerControl is flushing.\n");
                postEvent(MEDIA_NOP, 0, 0);
                break;
            }
            case MSG_ERROR:
            {
                ALOGD("MediaPlayerControl occurs error: %d\n", msg.arg1);
                if (mPrepareSync)
                {
                    mPrepareSync = false;
                    mPrepareStatus = msg.arg1;
                }
                postEvent(MEDIA_ERROR, msg.arg1, 0);
                break;
            }
            case MSG_PREPARED:
            {
                ALOGD("MediaPlayerControl is prepared.\n");
                if (mPrepareSync)
                {
                    mPrepareSync = false;
                    mPrepareStatus = NO_ERROR;
                }
                postEvent(MEDIA_PREPARED, 0, 0);
                break;
            }
            case MSG_STARTED:
            {
                ALOGD("MediaPlayerControl is started!");
                postEvent(MEDIA_STARTED, 0, 0);
                break;
            }
            case MSG_COMPLETED:
            {
                ALOGD("MediaPlayerControl is playback completed.\n");
                postEvent(MEDIA_PLAYBACK_COMPLETE, 0, 0);
                break;
            }
            case MSG_VIDEO_SIZE_CHANGED:
            {
                ALOGD("MediaPlayerControl is video size changing: %d, %d\n", msg.arg1, msg.arg2);
                postEvent(MEDIA_SET_VIDEO_SIZE, msg.arg1, msg.arg2);
                break;
            }
            case MSG_SAR_CHANGED:
            {
                ALOGD("MediaPlayerControl is sar changing: %d, %d\n", msg.arg1, msg.arg2);
                postEvent(MEDIA_SET_VIDEO_SAR, msg.arg1, msg.arg2);
                break;
            }
            case MSG_VIDEO_RENDERING_START:
            {
                ALOGD("MediaPlayerControl is video playing.\n");
                break;
            }
            case MSG_AUDIO_RENDERING_START:
            {
                ALOGD("MediaPlayerControl is audio playing.\n");
                break;
            }
            case MSG_VIDEO_ROTATION_CHANGED:
            {
                ALOGD("MediaPlayerControl's video rotation is changing: %d\n", msg.arg1);
                break;
            }
            case MSG_AUDIO_START:
            {
                ALOGD("MediaPlayerControl starts audio decoder.\n");
                break;
            }
            case MSG_VIDEO_START:
            {
                ALOGD("MediaPlayerControl starts video decoder.\n");
                break;
            }
            case MSG_OPEN_INPUT:
            {
                ALOGD("MediaPlayerControl is opening input file.\n");
                break;
            }
            case MSG_FIND_STREAM_INFO:
            {
                ALOGD("CanMediaPlayer is finding media stream info.\n");
                break;
            }
            case MSG_BUFFERING_START:
            {
                ALOGD("CanMediaPlayer is buffering start.\n");
                postEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_START, msg.arg1);
                break;
            }
            case MSG_BUFFERING_END:
            {
                ALOGD("MediaPlayerControl is buffering finish.\n");
                postEvent(MEDIA_INFO, MEDIA_INFO_BUFFERING_END, msg.arg1);
                break;
            }
            case MSG_BUFFERING_UPDATE:
            {
                ALOGD("MediaPlayerControl is buffering: %d, %d", msg.arg1, msg.arg2);
                postEvent(MEDIA_BUFFERING_UPDATE, msg.arg1, msg.arg2);
                break;
            }
            case MSG_BUFFERING_TIME_UPDATE:
            {
                ALOGD("MediaPlayerControl time text update");
                break;
            }
            case MSG_SEEK_COMPLETE:
            {
                ALOGD("MediaPlayerControl seeks completed!\n");
                mSeeking = false;
                postEvent(MEDIA_SEEK_COMPLETE, 0, 0);
                break;
            }
            case MSG_PLAYBACK_STATE_CHANGED:
            {
                ALOGD("MediaPlayerControl's playback state is changed.");
                break;
            }
            case MSG_TIMED_TEXT:
            {
                ALOGD("MediaPlayerControl is updating time text");
                postEvent(MEDIA_TIMED_TEXT, 0, 0, msg.obj);
                break;
            }
            case MSG_REQUEST_PREPARE:
            {
                ALOGD("MediaPlayerControl is preparing...");
                status_t ret = prepare();
                if (ret != NO_ERROR)
                {
                    ALOGE("MediaPlayerControl prepare error - '%d'", ret);
                }
                break;
            }
            case MSG_REQUEST_START:
            {
                ALOGD("MediaPlayerControl is waiting to start.");
                break;
            }
            case MSG_REQUEST_PAUSE:
            {
                ALOGD("MediaPlayerControl is pausing...");
                pause();
                break;
            }
            case MSG_REQUEST_SEEK:
            {
                ALOGD("MediaPlayerControl is seeking...");
                mSeeking = true;
                mSeekingPosition = (long) msg.arg1;
                if (mMediaPlayerEx != nullptr)
                {
                    mMediaPlayerEx->seekTo(mSeekingPosition);
                }
                break;
            }
            case MSG_CURRENT_POSITON:
            {
                postEvent(MEDIA_CURRENT, msg.arg1, msg.arg2);
                break;
            }
            default:
            {
                ALOGE("MediaPlayerControl unknown MSG_xxx(%d)\n", msg.what);
                break;
            }
        }
        message_free_resouce(&msg);
    }
}
