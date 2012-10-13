#include <QBuffer>

#include "image_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "utils/common/util.h"
#include "core/resource/network_resource.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource/camera_resource.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "device_plugins/server_archive/server_archive_delegate.h"
#include "core/datapacket/media_data_packet.h"
#include "decoders/video/ffmpeg.h"
#include "libswscale/swscale.h"
#include "camera/camera_pool.h"

static const int MAX_GOP_LEN = 100;

QnImageHandler::QnImageHandler()
{

}

QnCompressedVideoDataPtr getNextArchiveVideoPacket(QnServerArchiveDelegate& serverDelegate)
{
    QnCompressedVideoDataPtr video;
    for (int i = 0; i < 20 && !video; ++i) 
    {
        QnAbstractMediaDataPtr media = serverDelegate.getNextData();
        if (!media || media->dataType == QnAbstractMediaData::EMPTY_DATA)
            break;
        video = media.dynamicCast<QnCompressedVideoData>();
    }
    return video;
}

int QnImageHandler::noVideoError(QByteArray& result, qint64 time)
{
    result.append("<root>\n");
    result.append(QString("No video for time %1").arg(time));
    result.append("</root>\n");
    return CODE_INVALID_PARAMETER;
}

int QnImageHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    QnVirtualCameraResourcePtr res;
    QString errStr;
    bool resParamFound = false;
    qint64 time = AV_NOPTS_VALUE;
    bool precise = false;
    QByteArray format("jpg");
    QSize dstSize;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "res_id" || params[i].first == "physicalId")
        {
            resParamFound = true;
            res = qSharedPointerDynamicCast<QnVirtualCameraResource> (QnResourcePool::instance()->getNetResourceByPhysicalId(params[i].second));
            if (!res)
                errStr = QString("Camera resource %1 not found").arg(params[i].second);
        }
        else if (params[i].first == "time")
            time = parseDateTime(params[i].second);
        else if (params[i].first == "precise") {
            QString val = params[i].second.toLower().trimmed(); 
            precise = (val == "true" || val == "1");
        }
        else if (params[i].first == "format") {
            format = params[i].second.toUtf8();
            if (format == "jpg")
                format = "jpeg";
            else if (format == "tif")
                format = "tiff";
        }
        else if (params[i].first == "height")
            dstSize.setHeight(params[i].second.toInt());
        else if (params[i].first == "width")
            dstSize.setWidth(params[i].second.toInt());
    }
    if (!resParamFound)
        errStr = QLatin1String("parameter 'physicalId' is absent");
    else if (time == AV_NOPTS_VALUE)
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

    QnServerArchiveDelegate serverDelegate;
    serverDelegate.open(res);
    serverDelegate.seek(time, true);
    bool useHQ = true;
    if (dstSize.width() > 0 && dstSize.width() <= 320 || dstSize.height() > 0 && dstSize.height() <= 240)
        useHQ = false;

    if (!useHQ)
        serverDelegate.setQuality(MEDIA_Quality_Low, true);

    QnCompressedVideoDataPtr video;
    CLVideoDecoderOutput outFrame;
    // read data from archive
    if (time == DATETIME_NOW) {
        // get live data
        QnVideoCamera* camera = qnCameraPool->getVideoCamera(res);
        if (camera)
            video = camera->getLastVideoFrame(useHQ);
    }
    else {
        video = getNextArchiveVideoPacket(serverDelegate);
    }
    if (!video) 
        return noVideoError(result, time);

    CLFFmpegVideoDecoder decoder(video->compressionType, video, false);
    bool gotFrame = false;

    if (time == DATETIME_NOW) {
        gotFrame = decoder.decode(video, &outFrame);
    }
    else {
        for (int i = 0; i < MAX_GOP_LEN; ++i)
        {
            gotFrame = decoder.decode(video, &outFrame) && (!precise || video->timestamp >= time);
            if (gotFrame)
                break;
            video = getNextArchiveVideoPacket(serverDelegate);
        }
    }
    if (!gotFrame)
        return noVideoError(result, time);

    double ar = decoder.getSampleAspectRatio() * outFrame.width / outFrame.height;
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
        dstSize = QSize(outFrame.width, outFrame.height);
    }
    dstSize.setWidth(qMin(dstSize.width(), outFrame.width));
    dstSize.setHeight(qMin(dstSize.height(), outFrame.height));

    if (dstSize.width() < 8 || dstSize.height() < 8)
    {
        // something wrong
        result.append("<root>\n");
        result.append("Internal server error");
        result.append("</root>\n");
        return CODE_INTERNAL_ERROR;
    }

    int numBytes = avpicture_get_size(PIX_FMT_RGBA, qPower2Ceil(static_cast<quint32>(dstSize.width()), 8), dstSize.height());
    uchar* scaleBuffer = static_cast<uchar*>(qMallocAligned(numBytes, 32));
    SwsContext* scaleContext = sws_getContext(outFrame.width, outFrame.height, PixelFormat(outFrame.format), 
        dstSize.width(), dstSize.height(), PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);

    int dstLineSize[4];
    quint8* dstBuffer[4];
    dstLineSize[0] = qPower2Ceil(static_cast<quint32>(dstSize.width() * 4), 32);
    dstLineSize[1] = dstLineSize[2] = dstLineSize[3] = 0;
    QImage image(scaleBuffer, dstSize.width(), dstSize.height(), dstLineSize[0], QImage::Format_ARGB32_Premultiplied);
    dstBuffer[0] = dstBuffer[1] = dstBuffer[2] = dstBuffer[3] = scaleBuffer;
    sws_scale(scaleContext, outFrame.data, outFrame.linesize, 0, outFrame.height, dstBuffer, dstLineSize);

    QBuffer output(&result);
    image.save(&output, format);

    sws_freeContext(scaleContext);
    qFreeAligned(scaleBuffer);

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

int QnImageHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
        return executeGet(path, params, result, contentType);
}

QString QnImageHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
        QString rez;
    rez += "Return image from camera <BR>";
    rez += "<BR>Param <b>physicalId</b> - camera physicalId.";
    rez += "<BR>Param <b>time</b> - required image time. Microseconds since 1970 UTC or string in format 'YYYY-MM-DDThh24:mi:ss.zzz'. format is auto detected.";
    rez += "<BR>Param <b>format</b> - Optional. image format. Allowed values: 'jpeg', 'png', 'bmp', 'tiff'. Default value 'jpeg";
    rez += "<BR>Param <b>precise</b> - Optional. Allowed values: 'true' or 'false'. If parameter is 'false' server returns nearest I-frame instead of exact frame. Default value 'false'. Parameter not used for Motion jpeg video codec";
    rez += "<BR>Param <b>height</b> - Optional. Required image height.";
    rez += "<BR>Param <b>width</b> - Optional. Required image width. If only width or height is specified other parameter is auto detected. Video aspect ratio is not changed";
    rez += "<BR>Returns image";
    return rez;
}
