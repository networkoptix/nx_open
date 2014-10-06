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
#include "core/resource/camera_resource.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"
#include "core/datapacket/media_data_packet.h"
#include "decoders/video/ffmpeg.h"
#include "camera/camera_pool.h"
#include "qmath.h"

static const int MAX_GOP_LEN = 100;
static const qint64 LATEST_IMAGE = -1;

QnCompressedVideoDataPtr getNextArchiveVideoPacket(QnServerArchiveDelegate& serverDelegate, qint64 ceilTime)
{
    QnCompressedVideoDataPtr video;
    for (int i = 0; i < 20 && !video; ++i) 
    {
        QnAbstractMediaDataPtr media = serverDelegate.getNextData();
        if (!media || media->timestamp == DATETIME_NOW)
            break;
        video = media.dynamicCast<QnCompressedVideoData>();
    }

    // if ceilTime specified try frame with time > requested time (round time to ceil)
    if (ceilTime != (qint64)AV_NOPTS_VALUE && video && video->timestamp < ceilTime - 1000ll)
    {
        for (int i = 0; i < MAX_GOP_LEN; ++i) 
        {
            QnAbstractMediaDataPtr media2 = serverDelegate.getNextData();
            if (!media2 || media2->timestamp == DATETIME_NOW)
                break;
            QnCompressedVideoDataPtr video2 = media2.dynamicCast<QnCompressedVideoData>();
            if (video2 && (video2->flags & AV_PKT_FLAG_KEY))
                return video2;
        }
    }

    return video;
}

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

/*
* Rotate image. only values 90, 180, 270 is supported
*/
uchar* rotateImage(AVFrame& pict, int rotate)
{
    if (rotate > 180)
        rotate = 270;
    else if (rotate > 90)
        rotate = 180;
    else
        rotate = 90;

    int dstWidth = pict.width;
    int dstHeight = pict.height;
    if (rotate != 180)
        qSwap(dstWidth, dstHeight);

    int numBytes = avpicture_get_size((PixelFormat) pict.format, dstWidth, dstHeight);
    uchar* newBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));
    AVFrame dstPict = pict;
    avpicture_fill((AVPicture*) &dstPict, newBuffer, (PixelFormat) pict.format, dstWidth, dstHeight);
    dstPict.width = dstWidth;
    dstPict.height = dstHeight;

    const AVPixFmtDescriptor* descr = &av_pix_fmt_descriptors[pict.format];
    for (int i = 0; i < descr->nb_components && pict.data[i]; ++i) 
    {
        int filler = (i == 0 ? 0x0 : 0x80);
        int numButes = dstPict.linesize[i] * dstHeight;
        if (i > 0)
            numButes >>= descr->log2_chroma_h;
        memset(dstPict.data[i], filler, numButes);

        int w = pict.width;
        int h = pict.height;
        if (i > 0) {
            w >>= descr->log2_chroma_w;
            h >>= descr->log2_chroma_h;
        }

        if (rotate == 90) 
        {
            for (int y = 0; y < h; ++y) {
                quint8* src = pict.data[i] + pict.linesize[i] * y;
                quint8* dst = dstPict.data[i] + h -1 - y;
                for (int x = 0; x < w; ++x) {
                    *dst = *src++;
                    dst += dstPict.linesize[i];
                }
            }
        }
        else if (rotate == 180) 
        {
            for (int y = 0; y < h; ++y) {
                quint8* src = pict.data[i] + pict.linesize[i] * y;
                quint8* dst = dstPict.data[i] + dstPict.linesize[i] * (h-1 - y) + w-1;
                for (int x = 0; x < w; ++x) {
                    *dst-- = *src++;
                }
            }
        }
        else {
            for (int y = 0; y < h; ++y) {
                quint8* src = pict.data[i] + pict.linesize[i] * y;
                quint8* dst = dstPict.data[i] + dstPict.linesize[i] * (w - 1) + y;
                for (int x = 0; x < w; ++x) {
                    *dst = *src++;
                    dst -= dstPict.linesize[i];
                }
            }
        }
    }
    pict = dstPict;
    return newBuffer;
}

int QnImageRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)

    QnVirtualCameraResourcePtr res;
    QString errStr;
    bool resParamFound = false;
    qint64 time = AV_NOPTS_VALUE;
    RoundMethod roundMethod = IFrameBeforeTime;
    QByteArray format("jpg");
    QByteArray colorSpace("argb");
    QSize dstSize;
    int rotate = 0;
    bool rotateDefined = false;

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
                time = LATEST_IMAGE;
            else
                time = parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "rotate") {
            rotate = params[i].second.toInt();
            rotateDefined = true;
        }
        else if (params[i].first == "method") {
            QString val = params[i].second.toLower().trimmed(); 
            if (val == lit("before"))
                roundMethod = IFrameBeforeTime;
            else if (val == lit("precise") || val == lit("exact")) {
#               ifdef EDGE_SERVER
                    roundMethod = IFrameBeforeTime;
                    qWarning() << "Get image performance hint: Ignore precise round method to reduce CPU usage";
#               else
                    roundMethod = Precise;
#               endif
            }
            else if (val == lit("after"))
                roundMethod = IFrameAfterTime;
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

    if (!rotateDefined)
        rotate = res->getProperty(QnMediaResource::rotationKey()).toInt();

#ifdef EDGE_SERVER
    if (dstSize.height() < 1)
        dstSize.setHeight(360); //on edge instead of full-size image we return 360p
#endif

    bool useHQ = true;
    if ((dstSize.width() > 0 && dstSize.width() <= 480) || (dstSize.height() > 0 && dstSize.height() <= 316))
        useHQ = false;

    QnServerArchiveDelegate serverDelegate;
    if (!useHQ)
        serverDelegate.setQuality(MEDIA_Quality_Low, true);

    QnConstCompressedVideoDataPtr video;
    QSharedPointer<CLVideoDecoderOutput> outFrame( new CLVideoDecoderOutput() );
    QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);

    if (time == DATETIME_NOW) 
    {
        // get live data
        if (camera)
            video = camera->getLastVideoFrame(useHQ);
    }
    else if (time == LATEST_IMAGE)
    {
        // get latest data
        if (camera) {
            video = camera->getLastVideoFrame(useHQ);
            if (!video)
                video = camera->getLastVideoFrame(!useHQ);
        }
        if (!video) {
            video = camera->getLastVideoFrame(!useHQ);

            serverDelegate.open(res);
            serverDelegate.seek(serverDelegate.endTime()-1000*100, true);
            video = getNextArchiveVideoPacket(serverDelegate, AV_NOPTS_VALUE);
        }
        else {
            time = DATETIME_NOW;
        }
    }
    else {
        // get archive data
        serverDelegate.open(res);
        serverDelegate.seek(time, true);
        video = getNextArchiveVideoPacket(serverDelegate, roundMethod == IFrameAfterTime ? time : AV_NOPTS_VALUE);

        if (!video) {
            video = camera->getFrameByTime(useHQ, time, roundMethod == IFrameAfterTime); // try approx frame from GOP keeper
            time = DATETIME_NOW;
        }
        if (!video)
            video = camera->getFrameByTime(!useHQ, time, roundMethod == IFrameAfterTime); // try approx frame from GOP keeper
    }
    if (!video) 
        return noVideoError(result, time);

    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
    bool gotFrame = false;

    if (time == DATETIME_NOW) {
        if (res->getStatus() == Qn::Online || res->getStatus() == Qn::Recording)
        {
            gotFrame = decoder.decode(video, &outFrame);
            if (!gotFrame)
                gotFrame = decoder.decode(video, &outFrame); // decode twice
        }
    }
    else {
        bool precise = roundMethod == Precise;
        for (int i = 0; i < MAX_GOP_LEN && !gotFrame && video; ++i)
        {
            gotFrame = decoder.decode(video, &outFrame) && (!precise || video->timestamp >= time);
            if (gotFrame)
                break;
            video = getNextArchiveVideoPacket(serverDelegate, AV_NOPTS_VALUE);
        }
    }
    if (!gotFrame)
        return noVideoError(result, time);

    double sar = decoder.getSampleAspectRatio();
    double ar = sar * outFrame->width / outFrame->height;
    if (!dstSize.isEmpty()) {
        dstSize.setHeight(qPower2Ceil((unsigned) dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned) dstSize.width(), 4));
    }
    else if (dstSize.height() > 0) {
        dstSize.setHeight(qPower2Ceil((unsigned) dstSize.height(), 4));
        dstSize.setWidth(qPower2Ceil((unsigned) (dstSize.height()*ar), 4));
    }
    else if (dstSize.width() > 0) {
        dstSize.setWidth(qPower2Ceil((unsigned) dstSize.width(), 4));
        dstSize.setHeight(qPower2Ceil((unsigned) (dstSize.width()/ar), 4));
    }
    else {
        dstSize = QSize(outFrame->width * sar, outFrame->height);
    }
    dstSize.setWidth(qMin(dstSize.width(), outFrame->width * sar));
    dstSize.setHeight(qMin(dstSize.height(), outFrame->height));

    if (dstSize.width() < 8 || dstSize.height() < 8)
    {
        // something wrong
        result.append("<root>\n");
        result.append("Internal server error");
        result.append("</root>\n");
        return CODE_INTERNAL_ERROR;
    }

    int roundedWidth = qPower2Ceil((unsigned) dstSize.width(), 8);
    int roundedHeight = qPower2Ceil((unsigned) dstSize.height(), 2);

    if (format == "jpg" || format == "jpeg")
    {
        // prepare image using ffmpeg encoder
        AVFrame dstPict;
        dstPict.width = dstSize.width();
        dstPict.height = dstSize.height();
        dstPict.format = outFrame->format;
        if (dstPict.format == PIX_FMT_YUV422P)
            dstPict.format = PIX_FMT_YUV420P; // avoid different pixel dense for X and Y. It's avoid rotation problem

        int numBytes = avpicture_get_size((PixelFormat) dstPict.format, roundedWidth, roundedHeight);
        uchar* scaleBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));
        SwsContext* scaleContext = sws_getContext(outFrame->width, outFrame->height, PixelFormat(outFrame->format), 
            dstPict.width, dstPict.height, (PixelFormat)dstPict.format, SWS_BICUBIC, NULL, NULL, NULL);

        avpicture_fill((AVPicture*) &dstPict, scaleBuffer, (PixelFormat) dstPict.format, roundedWidth, roundedHeight);
        sws_scale(scaleContext, outFrame->data, outFrame->linesize, 0, outFrame->height, dstPict.data, dstPict.linesize);
        sws_freeContext(scaleContext);

        if (rotate != 0) {
            uchar* newbuffer = rotateImage(dstPict, rotate);
            qFreeAligned(scaleBuffer);
            scaleBuffer = newbuffer;
        }

        AVCodecContext* videoEncoderCodecCtx = avcodec_alloc_context3(0);
        videoEncoderCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
        videoEncoderCodecCtx->codec_id = (format == "jpg" || format == "jpeg") ? CODEC_ID_MJPEG : CODEC_ID_PNG;
        videoEncoderCodecCtx->pix_fmt = updatePixelFormat((PixelFormat) dstPict.format);
        videoEncoderCodecCtx->width = dstPict.width;
        videoEncoderCodecCtx->height = dstPict.height;
        videoEncoderCodecCtx->bit_rate = dstPict.width*dstPict.height;
        videoEncoderCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
        videoEncoderCodecCtx->time_base.num = 1;
        videoEncoderCodecCtx->time_base.den = 30;
    
        AVCodec* codec = avcodec_find_encoder_by_name(format == "jpg" || format == "jpeg" ? "mjpeg" : format.constData());
        if (avcodec_open2(videoEncoderCodecCtx, codec, NULL) < 0)
        {
            qWarning() << "Can't initialize ffmpeg encoder for encoding image to format " << format;
        }
        else {
            const int MAX_VIDEO_FRAME = dstPict.width * dstPict.height * 3 / 2;
            quint8* m_videoEncodingBuffer = (quint8*) qMallocAligned(MAX_VIDEO_FRAME, 32);
            int encoded = avcodec_encode_video(videoEncoderCodecCtx, m_videoEncodingBuffer, MAX_VIDEO_FRAME, &dstPict);
            result.append((const char*) m_videoEncodingBuffer, encoded);
            qFreeAligned(m_videoEncodingBuffer);
            avcodec_close(videoEncoderCodecCtx);
        }
        av_freep(&videoEncoderCodecCtx);
        qFreeAligned(scaleBuffer);
    }
    else
    {
        // prepare image using QT

        PixelFormat ffmpegColorFormat = PIX_FMT_BGRA;
        QImage::Format qtColorFormat = QImage::Format_ARGB32_Premultiplied;
        if (colorSpace == "gray8") {
            ffmpegColorFormat = PIX_FMT_GRAY8;
            qtColorFormat = QImage::Format_Indexed8;
        }


        int numBytes = avpicture_get_size(ffmpegColorFormat, roundedWidth, roundedHeight);
        uchar* scaleBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));
        SwsContext* scaleContext = sws_getContext(outFrame->width, outFrame->height, PixelFormat(outFrame->format), 
                                   dstSize.width(), dstSize.height(), ffmpegColorFormat, SWS_BICUBIC, NULL, NULL, NULL);

        AVPicture dstPict;
        avpicture_fill(&dstPict, scaleBuffer, (PixelFormat) ffmpegColorFormat, roundedWidth, roundedHeight);
    
        QImage image(scaleBuffer, dstSize.width(), dstSize.height(), dstPict.linesize[0], qtColorFormat);
        sws_scale(scaleContext, outFrame->data, outFrame->linesize, 0, outFrame->height, dstPict.data, dstPict.linesize);

        QBuffer output(&result);
        
        if (rotate != 0) {
            QTransform transform;
            transform.rotate(rotate);
            QImage rotatedImage =  image.transformed(transform);
            rotatedImage.save(&output, format);
        }
        else {
            image.save(&output, format);
        }

        sws_freeContext(scaleContext);
        qFreeAligned(scaleBuffer);
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

