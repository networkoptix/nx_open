// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_io_context.h"

#include <core/resource/storage_resource.h>

extern "C" {
#include <libavutil/mem.h>
#include <libavutil/opt.h>
} // extern "C"

namespace {

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    auto bytesRead = reader->read((char*) buf, size);
    return bytesRead == 0 ? AVERROR_EOF : bytesRead;
}

static qint32 ffmpegWritePacket(void *opaque, const quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    if (reader)
        return reader->write((char*) buf, size);
    else
        return 0;
}

static int64_t ffmpegSeek(void* opaque, int64_t pos, int whence)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);

    qint64 absolutePos = pos;
    switch (whence)
    {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        absolutePos = reader->pos() + pos;
        break;
    case SEEK_END:
        absolutePos = reader->size() + pos;
        break;
    case 65536:
        return reader->size();
    default:
        return AVERROR(ENOENT);
    }

    return reader->seek(absolutePos);
}

} // namespace

namespace nx::utils::media {

AVIOContext* createFfmpegIOContext(
    QnStorageResourcePtr resource,
    const QString& url,
    QIODevice::OpenMode openMode,
    int ioBlockSize)
{
    QString path = url;

    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    QIODevice* ioDevice = resource->open(path, openMode);
    if (ioDevice == 0)
        return 0;

    ioBuffer = (quint8*) av_malloc(ioBlockSize);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        ioBlockSize,
        (openMode & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

AVIOContext* createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize)
{
    quint8* ioBuffer;
    AVIOContext* ffmpegIOContext;

    ioBuffer = (quint8*) av_malloc(ioBlockSize);
    ffmpegIOContext = avio_alloc_context(
        ioBuffer,
        ioBlockSize,
        (ioDevice->openMode() & QIODevice::WriteOnly) ? 1 : 0,
        ioDevice,
        &ffmpegReadPacket,
        &ffmpegWritePacket,
        &ffmpegSeek);

    return ffmpegIOContext;
}

void closeFfmpegIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        avio_flush(ioContext);

        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        delete ioDevice;
        ioContext->opaque = 0;
        //avio_close2(ioContext);

        av_freep(&ioContext->buffer);
        av_opt_free(ioContext);
        av_free(ioContext);
    }
}

} // namespace nx::utils::media
