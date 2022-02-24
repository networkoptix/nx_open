// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_initializer.h"

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavfilter/avfilter.h>
}

#include <nx/utils/thread/mutex.h>

static int lockmgr(void **mtx, enum AVLockOp op)
{
    nx::Mutex** qMutex = (nx::Mutex**)mtx;
    switch (op)
    {
    case AV_LOCK_CREATE:
        NX_ASSERT(*qMutex == nullptr);
        *qMutex = new nx::Mutex();
        return 0;
    case AV_LOCK_OBTAIN:
        NX_ASSERT(*qMutex);
        (*qMutex)->lock();
        return 0;
    case AV_LOCK_RELEASE:
        NX_ASSERT(*qMutex);
        (*qMutex)->unlock();
        return 0;
    case AV_LOCK_DESTROY:
        NX_ASSERT(*qMutex);
        delete *qMutex;
        *qMutex = nullptr;
        return 0;
    }
    return 1;
}

QnFfmpegInitializer::QnFfmpegInitializer(QObject* parent) :
    QObject(parent)
{
    av_register_all();
    avfilter_register_all();
    auto errCode = av_lockmgr_register(lockmgr);
    NX_ASSERT(errCode == 0, "Failed to register ffmpeg lock manager");
}

QnFfmpegInitializer::~QnFfmpegInitializer()
{
    av_lockmgr_register(nullptr);
}
