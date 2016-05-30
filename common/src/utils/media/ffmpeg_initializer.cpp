#include "ffmpeg_initializer.h"

extern "C"
{
    #include <libavformat/avformat.h>
}

#include <nx/utils/thread/mutex.h>

static int lockmgr(void **mtx, enum AVLockOp op)
{
    QnMutex** qMutex = (QnMutex**)mtx;
    switch (op) {
    case AV_LOCK_CREATE:
        *qMutex = new QnMutex();
        return 0;
    case AV_LOCK_OBTAIN:
        (*qMutex)->lock();
        return 0;
    case AV_LOCK_RELEASE:
        (*qMutex)->unlock();
        return 0;
    case AV_LOCK_DESTROY:
        delete *qMutex;
        return 0;
    }
    return 1;
}

QnFfmpegInitializer::QnFfmpegInitializer(QObject* parent) :
    QObject(parent)
{
    av_register_all();
    auto errCode = av_lockmgr_register(lockmgr);
    NX_ASSERT(errCode == 0, "Failed to register ffmpeg lock manager");
}

QnFfmpegInitializer::~QnFfmpegInitializer()
{
    av_lockmgr_register(nullptr);
}
