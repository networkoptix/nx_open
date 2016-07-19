#include "ffmpeg_helper.h"

#if !defined(DISABLE_FFMPEG)

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
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
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
    av->frame_size = media->getFrameSize();
}

AVCodec* QnFfmpegHelper::findAvCodec(AVCodecID codecId)
{
    AVCodec* codec = avcodec_find_decoder(codecId);

    if (!codec || codec->id != codecId)
        return &StaticHolder::instance.avCodec;

    return codec;
}

AVCodecContext* QnFfmpegHelper::createAvCodecContext(AVCodecID codecId)
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

//-------------------------------------------------------------------------------------------------
// Backwards compatibility with v2.5.

namespace {

enum CodecCtxField { Field_RC_EQ, Field_EXTRADATA, Field_INTRA_MATRIX, Field_INTER_MATRIX,
    Field_OVERRIDE, Field_Channels, Field_SampleRate, Field_Sample_Fmt, Field_BitsPerSample,
    Field_CodedWidth, Field_CodedHeight };

/**
 * Copied from v2.5 ffmpeg_helper.cpp.
 * @return nullptr on deserialization error.
 */
static AVCodecContext* deserializeCodecContextFromDepricatedFormat(const char* data, int dataLen)
{
    static const char* const kError = "ERROR deserializing MediaContext 2.5:";
    AVCodec* codec = nullptr;
    AVCodecContext* ctx = nullptr;

    QByteArray tmpArray(data, dataLen);
    QBuffer buffer(&tmpArray);
    buffer.open(QIODevice::ReadOnly);
    AVCodecID codecId = AV_CODEC_ID_NONE;
    int bytesRead = (int) buffer.read((char*) &codecId, 4);
    if (bytesRead < 4)
    {
        qWarning() << kError << "Less than 4 bytes";
        goto error;
    }

    codec = avcodec_find_decoder(codecId);
    if (codec == nullptr)
    {
        qWarning() << kError << "Codec not found:" << codecId;
        goto error;
    }
    ctx = avcodec_alloc_context3(codec);

    char objectType;

    for (;;)
    {
        int bytesRead = (int) buffer.read(&objectType, 1);
        if (bytesRead < 1)
            break;
        CodecCtxField field = (CodecCtxField) objectType;
        int size;
        if (buffer.read((char*) &size, 4) != 4)
        {
            qWarning() << kError << "Unable to read 4 bytes";
            goto error;
        }
        size = ntohl(size);
        char* fieldData = (char*) av_malloc(size);
        bytesRead = (int) buffer.read(fieldData, size);
        if (bytesRead != size)
        {
            av_free(fieldData);
            qWarning() << kError << "Not enough bytes: expected" << size << "but got" << bytesRead;
            goto error;
        }

        switch (field)
        {
            case Field_RC_EQ:
                ctx->rc_eq = fieldData;
                break;
            case Field_EXTRADATA:
                ctx->extradata = (quint8*) fieldData;
                ctx->extradata_size = size;
                break;
            case Field_INTRA_MATRIX:
                ctx->intra_matrix = (quint16*) fieldData;
                break;
            case Field_INTER_MATRIX:
                ctx->inter_matrix = (quint16*) fieldData;
                break;
            case Field_OVERRIDE:
                ctx->rc_override = (RcOverride*) fieldData;
                ctx->rc_override_count = size / sizeof(*ctx->rc_override);
                break;
            case Field_Channels:
                ctx->channels = *((int*) fieldData);
                av_free(fieldData);
                break;
            case Field_SampleRate:
                ctx->sample_rate = *((int*) fieldData);
                av_free(fieldData);
                break;
            case Field_Sample_Fmt:
                ctx->sample_fmt = *((AVSampleFormat*) fieldData);
                av_free(fieldData);
                break;
            case Field_BitsPerSample:
                ctx->bits_per_coded_sample = *((int*) fieldData);
                av_free(fieldData);
                break;
            case Field_CodedWidth:
                ctx->coded_width = *((int*) fieldData);
                av_free(fieldData);
                break;
            case Field_CodedHeight:
                ctx->coded_height = *((int*) fieldData);
                av_free(fieldData);
                break;
        }
    }

    ctx->codec_id = codecId;
    ctx->codec_type = codec->type;
    return ctx;

error:
    QnFfmpegHelper::deleteAvCodecContext(ctx);
    return nullptr;
}

} // namespace

bool QnFfmpegHelper::deserializeMediaContextFromDepricatedFormat(
    QnMediaContextSerializableData* context, const char* data, int dataLen)
{
    AVCodecContext* avCodecContext = deserializeCodecContextFromDepricatedFormat(data, dataLen);
    if (!avCodecContext)
        return false;

    context->initializeFrom(avCodecContext);

    QnFfmpegHelper::deleteAvCodecContext(avCodecContext);

    return true;
}

//-------------------------------------------------------------------------------------------------

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

QString QnFfmpegHelper::getErrorStr(int errnum)
{
    QByteArray result(AV_ERROR_MAX_STRING_SIZE, '\0');
    if (av_strerror(errnum, result.data(), result.size()) != 0)
        return QString(QString::fromLatin1("Unknown FFMPEG error with code %d")).arg(errnum);
    return QString::fromLatin1(result);
}

int QnFfmpegHelper::audioSampleSize(AVCodecContext* ctx)
{
    return ctx->channels * av_get_bytes_per_sample(ctx->sample_fmt);
}

QnFfmpegAudioHelper::QnFfmpegAudioHelper(AVCodecContext* decoderContex):
    m_swr(swr_alloc())
{
    av_opt_set_int(m_swr, "in_channel_layout", decoderContex->channel_layout, 0);
    av_opt_set_int(m_swr, "out_channel_layout", decoderContex->channel_layout, 0);
    av_opt_set_int(m_swr, "in_sample_rate", decoderContex->sample_rate, 0);
    av_opt_set_int(m_swr, "out_sample_rate", decoderContex->sample_rate, 0);
    av_opt_set_sample_fmt(m_swr, "in_sample_fmt", decoderContex->sample_fmt, 0);
    av_opt_set_sample_fmt(m_swr, "out_sample_fmt", av_get_packed_sample_fmt(decoderContex->sample_fmt), 0);
    swr_init(m_swr);
}

QnFfmpegAudioHelper::~QnFfmpegAudioHelper()
{
    swr_free(&m_swr);
}

void QnFfmpegAudioHelper::copyAudioSamples(quint8* dst, const AVFrame* src)
{
    quint8* tmpData[4];
    tmpData[0] = dst;
    swr_convert(m_swr,
        tmpData, src->nb_samples,
        (const quint8**) src->data, src->nb_samples);
}

#endif // !defined(DISABLE_FFMPEG)