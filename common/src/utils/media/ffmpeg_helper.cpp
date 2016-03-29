#include "ffmpeg_helper.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>

#include "nalUnits.h"
#include "bitStream.h"
#include "core/resource/storage_resource.h"
#include <utils/media/av_codec_helper.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/basic_media_context.h>

extern "C"
{
#include <libavutil/error.h>
}

QnFfmpegHelper::StaticHolder QnFfmpegHelper::StaticHolder::instance;

void QnFfmpegHelper::copyAvCodecContextField(void **fieldPtr, const void* data, size_t size)
{
    NX_ASSERT(fieldPtr);

    av_freep(fieldPtr);

    if (size > 0)
    {
        NX_ASSERT(data);
        *fieldPtr = av_malloc(size);
        NX_ASSERT(*fieldPtr);
        memcpy(*fieldPtr, data, size);
    }
}

void QnFfmpegHelper::mediaContextToAvCodecContext(
    AVCodecContext* av, const QnConstMediaContextPtr& media)
{
    NX_ASSERT(av);

    // NOTE: Here explicit runtime type dispatching is used instead of a more complex visitor-based pattern.
    // Generation of AVCodecContext from QnMediaContext cannot be done in QnMediaContext because that class
    // should not depend on ffmpeg's implementation which is required to do this job.
    auto avCodecMediaContext = std::dynamic_pointer_cast<const QnAvCodecMediaContext>(media);
    if (avCodecMediaContext)
    {
        int r = avcodec_copy_context(av, avCodecMediaContext->getAvCodecContext());
        NX_ASSERT(r == 0);
        return;
    }

    AVCodec* const codec = findAvCodec(media->getCodecId());
    NX_ASSERT(codec);
    int r = avcodec_get_context_defaults3(av, codec);
    NX_ASSERT(r == 0);
    av->codec = codec;
    // codec_type is copied by avcodec_get_context_defaults3() above.
    NX_ASSERT(av->codec_type == media->getCodecType(), Q_FUNC_INFO, "codec_type is not the same.");

    copyMediaContextFieldsToAvCodecContext(av, media);
}

void QnFfmpegHelper::copyMediaContextFieldsToAvCodecContext(
    AVCodecContext* av, const QnConstMediaContextPtr& media)
{
    av->codec_id = media->getCodecId();
    av->codec_type = media->getCodecType();

    copyAvCodecContextField((void**) &av->rc_eq, media->getRcEq(),
        media->getRcEq() == nullptr? 0 :
        strlen(media->getRcEq()) + 1);

    copyAvCodecContextField((void**) &av->extradata, media->getExtradata(),
        media->getExtradataSize());
    av->extradata_size = media->getExtradataSize();

    copyAvCodecContextField((void**) &av->intra_matrix, media->getIntraMatrix(),
        media->getIntraMatrix() == nullptr? 0 : 
        sizeof(av->intra_matrix[0]) * QnAvCodecHelper::kMatrixLength);

    copyAvCodecContextField((void**) &av->inter_matrix, media->getInterMatrix(),
        media->getInterMatrix() == nullptr? 0 : 
        sizeof(av->inter_matrix[0]) * QnAvCodecHelper::kMatrixLength);

    copyAvCodecContextField((void**) &av->rc_override, media->getRcOverride(),
        sizeof(av->rc_override[0]) * media->getRcOverrideCount());
    av->rc_override_count = media->getRcOverrideCount();

    av->channels = media->getChannels();
    av->sample_rate = media->getSampleRate();
    av->sample_fmt = media->getSampleFmt();
    av->bits_per_coded_sample = media->getBitsPerCodedSample();
    av->coded_width = media->getCodedWidth();
    av->coded_height = media->getCodedHeight();
    av->width = media->getWidth();
    av->height = media->getHeight();
    av->bit_rate = media->getBitRate();
    av->channel_layout = media->getChannelLayout();
    av->block_align = media->getBlockAlign();
}

AVCodec* QnFfmpegHelper::findAvCodec(CodecID codecId)
{
    AVCodec* codec = avcodec_find_decoder(codecId);

    if (!codec || codec->id != codecId)
        return &StaticHolder::instance.avCodec;

    return codec;
}

AVCodecContext* QnFfmpegHelper::createAvCodecContext(CodecID codecId)
{
    AVCodec* codec = findAvCodec(codecId);
    NX_ASSERT(codec);
    AVCodecContext* context = avcodec_alloc_context3(codec);
    NX_ASSERT(context);
    context->codec = codec;
    context->codec_id = codecId;
    return context;
}

AVCodecContext* QnFfmpegHelper::createAvCodecContext(const AVCodecContext* context)
{
    NX_ASSERT(context);

    AVCodecContext* newContext = avcodec_alloc_context3(nullptr);
    NX_ASSERT(newContext);
    int r = avcodec_copy_context(newContext, context);
    NX_ASSERT(r == 0);
    return newContext;
}

void QnFfmpegHelper::deleteAvCodecContext(AVCodecContext* context)
{
    if (!context)
        return;

    avcodec_close(context);
    av_freep(&context->rc_override);
    av_freep(&context->intra_matrix);
    av_freep(&context->inter_matrix);
    av_freep(&context->extradata);
    av_freep(&context->rc_eq);
    av_freep(&context);
}

static qint32 ffmpegReadPacket(void *opaque, quint8* buf, int size)
{
    QIODevice* reader = reinterpret_cast<QIODevice*> (opaque);
    return reader->read((char*) buf, size);
}

static qint32 ffmpegWritePacket(void *opaque, quint8* buf, int size)
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


AVIOContext* QnFfmpegHelper::createFfmpegIOContext(QnStorageResourcePtr resource, const QString& url, QIODevice::OpenMode openMode, int ioBlockSize)
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

AVIOContext* QnFfmpegHelper::createFfmpegIOContext(QIODevice* ioDevice, int ioBlockSize)
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

qint64 QnFfmpegHelper::getFileSizeByIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        return ioDevice->size();
    }
    return 0;
}

void QnFfmpegHelper::closeFfmpegIOContext(AVIOContext* ioContext)
{
    if (ioContext)
    {
        QIODevice* ioDevice = (QIODevice*) ioContext->opaque;
        delete ioDevice;
        ioContext->opaque = 0;
        avio_close(ioContext);
    }
}

QString QnFfmpegHelper::getErrorStr(int errnum)
{
    static const int AV_ERROR_MAX_STRING_SIZE = 64; //< Taken from newer Ffmpeg impl.
    QByteArray result(AV_ERROR_MAX_STRING_SIZE, '\0');
    if (av_strerror(errnum, result.data(), result.size()) != 0)
        return QString(QString::fromLatin1("Unknown FFMPEG error with code %d")).arg(errnum);
    return QString::fromLatin1(result);
}
