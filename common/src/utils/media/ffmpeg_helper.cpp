#include "ffmpeg_helper.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QBuffer>
#include <QtCore/QDebug>

#include "nalUnits.h"
#include "bitStream.h"
#include "core/resource/storage_resource.h"
#include <utils/media/codec_helper.h>

void QnFfmpegHelper::appendCtxField(QByteArray *dst, QnFfmpegHelper::CodecCtxField field, const char* data, int size)
{
    char f = (char) field;
    dst->append(&f, 1);
    int size1 = htonl(size);
    dst->append((const char*) &size1, 4);
    dst->append((const char*) data, size);
}

void QnFfmpegHelper::serializeCodecContext(const AVCodecContext *ctx, QByteArray *data)
{
    data->clear();
    data->append((const char*) &ctx->codec_id, 4);
    
    if (ctx->rc_eq) 
        appendCtxField(data, Field_RC_EQ, ctx->rc_eq, (int) strlen(ctx->rc_eq)+1); // +1 because of include \0 byte
    if (ctx->extradata) 
        appendCtxField(data, Field_EXTRADATA, (const char*) ctx->extradata, ctx->extradata_size);
    if (ctx->intra_matrix) 
        appendCtxField(data, Field_INTRA_MATRIX, (const char*) ctx->intra_matrix, 64 * sizeof(int16_t));
    if (ctx->inter_matrix)
        appendCtxField(data, Field_INTER_MATRIX, (const char*) ctx->inter_matrix, 64 * sizeof(int16_t));
    if (ctx->rc_override)
        appendCtxField(data, Field_OVERRIDE, (const char*) ctx->rc_override, ctx->rc_override_count * sizeof(*ctx->rc_override));

    if (ctx->channels > 0)
        appendCtxField(data, Field_Channels, (const char*) &ctx->channels, sizeof(int));
    if (ctx->sample_rate > 0)
        appendCtxField(data, Field_SampleRate, (const char*) &ctx->sample_rate, sizeof(int));
    if (ctx->sample_fmt != AV_SAMPLE_FMT_NONE)
        appendCtxField(data, Field_Sample_Fmt, (const char*) &ctx->sample_fmt, sizeof(AVSampleFormat));
    if (ctx->bits_per_coded_sample > 0)
        appendCtxField(data, Field_BitsPerSample, (const char*) &ctx->bits_per_coded_sample, sizeof(int));
    if (ctx->coded_width > 0)
        appendCtxField(data, Field_CodedWidth, (const char*) &ctx->coded_width, sizeof(int));
    if (ctx->coded_height > 0)
        appendCtxField(data, Field_CodedHeight, (const char*) &ctx->coded_height, sizeof(int));
}

AVCodecContext *QnFfmpegHelper::deserializeCodecContext(const char *data, int dataLen)
{
    AVCodec* codec = 0;
    // TODO: #vasilenko avoid using deprecated methods
    AVCodecContext* ctx = (AVCodecContext*) avcodec_alloc_context();

    QByteArray tmpArray(data, dataLen);
    QBuffer buffer(&tmpArray);
    buffer.open(QIODevice::ReadOnly);
    CodecID codecId = CODEC_ID_NONE;
    int readed = buffer.read((char*) &codecId, 4);
    if (readed < 4)
        goto error_label;
    codec = avcodec_find_decoder(codecId);

    if (codec == 0)
        goto error_label;
    
    char objectType;
    
    while (1)
    {
        int readed = buffer.read(&objectType, 1);
        if (readed < 1)
            break;
        CodecCtxField field = (CodecCtxField) objectType;
        int size;
        if (buffer.read((char*) &size, 4) != 4)
            goto error_label;
        size = ntohl(size);
        char* fieldData = (char*) av_malloc(size);
        readed = buffer.read(fieldData, size);
        if (readed != size) {
            av_free(fieldData);
            goto error_label;
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
                ctx->channels = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_SampleRate:
                ctx->sample_rate = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_Sample_Fmt:
                ctx->sample_fmt = *(AVSampleFormat*) fieldData;
                av_free(fieldData);
                break;
            case Field_BitsPerSample:
                ctx->bits_per_coded_sample = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_CodedWidth:
                ctx->coded_width = *(int*) fieldData;
                av_free(fieldData);
                break;
            case Field_CodedHeight:
                ctx->coded_height = *(int*) fieldData;
                av_free(fieldData);
                break;
        }
    }

    ctx->codec_id = codecId;
    ctx->codec_type = codec->type;
    return ctx;

error_label:
    qWarning() << Q_FUNC_INFO << __LINE__ << "Parse error in deserialize CodecContext";
    QnFfmpegHelper::deleteCodecContext(ctx);
    return nullptr;
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

void QnFfmpegHelper::deleteCodecContext(AVCodecContext* ctx)
{
    if (!ctx)
        return;
    avcodec_close(ctx);
    av_freep(&ctx->rc_override);
    av_freep(&ctx->intra_matrix);
    av_freep(&ctx->inter_matrix);
    av_freep(&ctx->extradata);
    av_freep(&ctx->rc_eq);
    av_freep(&ctx);
}

#endif // ENABLE_DATA_PROVIDERS
