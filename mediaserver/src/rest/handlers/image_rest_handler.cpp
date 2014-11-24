#include "image_rest_handler.h"

extern "C"
{
    #include <libswscale/swscale.h>
    #ifdef WIN32
        #define AVPixFmtDescriptor __declspec(dllimport) AVPixFmtDescriptor
    #endif
    #include <libavutil/pixdesc.h>
    #ifdef WIN32
        #undef AVPixFmtDescriptor
    #endif
}

#include <QtCore/QBuffer>

#include "utils/network/tcp_connection_priv.h"
#include <utils/math/math.h>
#include "core/resource/network_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/datapacket/media_data_packet.h"
#include "decoders/video/ffmpeg.h"
#include "qmath.h"
#include "camera/get_image_helper.h"

int QnImageRestHandler::noVideoError(QByteArray& result, qint64 time)
{
    result.append("<root>\n");
    result.append("No video for time ");
    if (time == DATETIME_NOW)
        result.append("now");
    else
        result.append(QByteArray::number(time));
    result.append("</root>\n");
    return CODE_INVALID_PARAMETER;
}

PixelFormat updatePixelFormat(PixelFormat fmt)
{
    switch(fmt)
    {
    case PIX_FMT_YUV420P:
        return PIX_FMT_YUVJ420P;
    case PIX_FMT_YUV422P:
        return PIX_FMT_YUVJ422P;
    case PIX_FMT_YUV444P:
        return PIX_FMT_YUVJ444P;
    default:
        return fmt;
    }
}

int QnImageRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)

    QnVirtualCameraResourcePtr res;
    QString errStr;
    bool resParamFound = false;
    qint64 time = AV_NOPTS_VALUE;
    QnGetImageHelper::RoundMethod roundMethod = QnGetImageHelper::IFrameBeforeTime;
    QByteArray format("jpg");
    QByteArray colorSpace("argb");
    QSize dstSize;
    int rotate = -1;

    //validating input parameters
    auto widthIter = params.find( "width" );
    auto heightIter = params.find( "height" );
    if( (widthIter != params.end()) && (heightIter == params.end() || heightIter->second.toInt() < 1) )
        return nx_http::StatusCode::badRequest; //width cannot be specified without height, but only height is acceptable

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "res_id" || params[i].first == "physicalId")
        {
            resParamFound = true;
            res = qSharedPointerDynamicCast<QnVirtualCameraResource> (QnResourcePool::instance()->getNetResourceByPhysicalId(params[i].second));
            if (!res)
                errStr = QString("Camera resource %1 not found").arg(params[i].second);
        }
        else if (params[i].first == "time") {
            if (params[i].second.toLower().trimmed() == "latest")
                time = QnGetImageHelper::LATEST_IMAGE;
            else
                time = parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "rotate") {
            rotate = params[i].second.toInt();
        }
        else if (params[i].first == "method") {
            QString val = params[i].second.toLower().trimmed(); 
            if (val == lit("before"))
                roundMethod = QnGetImageHelper::IFrameBeforeTime;
            else if (val == lit("precise") || val == lit("exact")) {
#               ifdef EDGE_SERVER
                    roundMethod = QnGetImageHelper::IFrameBeforeTime;
                    qWarning() << "Get image performance hint: Ignore precise round method to reduce CPU usage";
#               else
                    roundMethod = QnGetImageHelper::Precise;
#               endif
            }
            else if (val == lit("after"))
                roundMethod = QnGetImageHelper::IFrameAfterTime;
        }
        else if (params[i].first == "format") {
            format = params[i].second.toUtf8();
            if (format == "jpg")
                format = "jpeg";
            else if (format == "tif")
                format = "tiff";
        }
        else if (params[i].first == "colorspace") 
        {
            colorSpace = params[i].second.toUtf8();
        }
        else if (params[i].first == "height")
            dstSize.setHeight(params[i].second.toInt());
        else if (params[i].first == "width")
            dstSize.setWidth(params[i].second.toInt());
    }
    if (!resParamFound)
        errStr = QLatin1String("parameter 'physicalId' is absent");
    else if (time == (qint64)AV_NOPTS_VALUE)
        errStr = QLatin1String("parameter 'time' is absent");
    else if (dstSize.height() >= 0 && dstSize.height() < 8)
        errStr = QLatin1String("Parameter height must be >= 8");
    else if (dstSize.width() >= 0 && dstSize.width() < 8)
        errStr = QLatin1String("Parameter width must be >= 8");

    if (!errStr.isEmpty()) {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    CLVideoDecoderOutputPtr outFrame = QnGetImageHelper::getImage(res, time, dstSize, roundMethod, rotate);
    if (!outFrame)
        return noVideoError(result, time);

    if (format == "jpg" || format == "jpeg")
    {
        AVCodecContext* videoEncoderCodecCtx = avcodec_alloc_context3(0);
        videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        videoEncoderCodecCtx->codec_id = (format == "jpg" || format == "jpeg") ? CODEC_ID_MJPEG : CODEC_ID_PNG;
        videoEncoderCodecCtx->pix_fmt = updatePixelFormat((PixelFormat) outFrame->format);
        videoEncoderCodecCtx->width = outFrame->width;
        videoEncoderCodecCtx->height = outFrame->height;
        videoEncoderCodecCtx->bit_rate = outFrame->width * outFrame->height;
        videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        videoEncoderCodecCtx->time_base.num = 1;
        videoEncoderCodecCtx->time_base.den = 30;
    
        AVCodec* codec = avcodec_find_encoder_by_name(format == "jpg" || format == "jpeg" ? "mjpeg" : format.constData());
        if (avcodec_open2(videoEncoderCodecCtx, codec, NULL) < 0)
        {
            qWarning() << "Can't initialize ffmpeg encoder for encoding image to format " << format;
        }
        else {
            const int MAX_VIDEO_FRAME = outFrame->width * outFrame->height * 3 / 2;
            quint8* m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
            int encoded = avcodec_encode_video(videoEncoderCodecCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, outFrame.data());
            result.append((const char*) m_videoEncodingBuffer, encoded);
            qFreeAligned(m_videoEncodingBuffer);
            avcodec_close(videoEncoderCodecCtx);
        }
        av_freep(&videoEncoderCodecCtx);
    }
    else
    {
        // prepare image using QT
        QImage image = outFrame->toImage();
        QBuffer output(&result);
        image.save(&output, format);
    }

    if (result.isEmpty())
    {
        result.append("<root>\n");
        result.append(QString("Invalid image format '%1'").arg(QString(format)));
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    contentType = QByteArray("image/") + format;
    return CODE_OK;

}

int QnImageRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                    QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
