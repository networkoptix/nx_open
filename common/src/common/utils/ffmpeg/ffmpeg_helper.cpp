#include "ffmpeg_helper.h"
#include "libavcodec/avcodec.h"

#include <QtCore/QDebug>

void QnFfmpegHelper::serializeCodecContext(const AVCodecContext* ctx, QByteArray* data)
{
    data->clear();

    // serialize context in way as avcodec_copy_context is works
    AVCodecContext dest;
    memcpy(&dest, ctx, sizeof(AVCodecContext));
    dest.priv_data       = NULL;
    dest.codec           = NULL;
    dest.palctrl         = NULL;
    dest.slice_offset    = NULL;
    dest.internal_buffer = NULL;
    dest.hwaccel         = NULL;
    dest.thread_opaque   = NULL;

    data->append((const char*) &dest, sizeof(AVCodecContext));
    if (ctx->rc_eq)
        data->append(ctx->rc_eq);
    if (ctx->extradata)
        data->append((const char*) ctx->extradata, ctx->extradata_size);
    if (ctx->intra_matrix)
        data->append((const char*) ctx->intra_matrix, 64 * sizeof(int16_t));
    if (ctx->inter_matrix)
        data->append((const char*) ctx->inter_matrix, 64 * sizeof(int16_t));
    if (ctx->rc_override)
        data->append((const char*) ctx->rc_override, ctx->rc_override_count * sizeof(*ctx->rc_override));
}

AVCodecContext* QnFfmpegHelper::deserializeCodecContext(const char* data, int dataLen)
{
    if (dataLen < sizeof(AVCodecContext))
    {
        qWarning() << Q_FUNC_INFO << __LINE__ << "Too few data for deserialize CodecContext";
        return 0;
    }
    AVCodecContext* ctx = (AVCodecContext*) av_malloc(sizeof(AVCodecContext));
    memcpy(ctx, data, sizeof(AVCodecContext));
    dataLen -= sizeof(AVCodecContext);
    data += sizeof(AVCodecContext);

    if (ctx->rc_eq && dataLen > 0)
    {
        int len = qstrnlen(data, dataLen);
        if (data[len] != 0)
        {
            qWarning() << Q_FUNC_INFO << __LINE__ << "Too few data for deserialize CodecContext";
            av_free(ctx);
            return 0;
        }
        ctx->rc_eq = av_strdup(data);
        data += len;
        dataLen -= len;
    }

    if (ctx->extradata)
    {
        if (dataLen >= ctx->extradata_size) {
            ctx->extradata = (quint8*) av_malloc(ctx->extradata_size);
            memcpy(ctx->extradata, data, ctx->extradata_size);
            data += ctx->extradata_size;
            dataLen -=  ctx->extradata_size;;
        }
        else {
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }

    if (ctx->intra_matrix)
    {
        if (dataLen >= 64 * sizeof(int16_t)) {
            ctx->intra_matrix = (quint16*) av_malloc(64 * sizeof(int16_t));
            memcpy(ctx->intra_matrix, data, 64 * sizeof(int16_t));
            data += 64 * sizeof(int16_t);
            dataLen -= 64 * sizeof(int16_t);
        }
        else {
            av_free(ctx->extradata);
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }

    if (ctx->inter_matrix)
    {
        if (dataLen >= 64 * sizeof(int16_t)) {
            ctx->inter_matrix = (quint16*) av_malloc(64 * sizeof(int16_t));
            memcpy(ctx->inter_matrix, data, 64 * sizeof(int16_t));
            data += 64 * sizeof(int16_t);
            dataLen -= 64 * sizeof(int16_t);
        }
        else {
            av_free(ctx->intra_matrix);
            av_free(ctx->extradata);
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }
    int ovSize = ctx->rc_override_count * sizeof(*ctx->rc_override);
    if (ctx->rc_override)
    {
        if (dataLen >= ovSize) {
            ctx->rc_override = (RcOverride*) av_malloc(ovSize);
            memcpy(ctx->rc_override, data, ovSize);
        }
        else {
            av_free(ctx->inter_matrix);
            av_free(ctx->intra_matrix);
            av_free(ctx->extradata);
            av_free((void*) ctx->rc_eq);
            av_free(ctx);
            return 0;
        }
    }
    return ctx;
}
