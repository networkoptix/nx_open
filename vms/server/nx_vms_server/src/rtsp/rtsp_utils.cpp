#include "rtsp_utils.h"

#include <map>
#include <vector>

#include <streaming/streaming_params.h>
#include <transcoding/transcoding_utils.h>

namespace nx::rtsp {

QSize parseResolution(const QString& str)
{
    QSize resolution;
    if (str.endsWith('p'))
    {
        resolution = QSize(0, str.left(str.length() - 1).trimmed().toInt());
        return resolution;
    }

    QStringList strList = str.split('x');
    if (strList.size() != 2)
        return QSize();

    resolution = QSize(strList[0].trimmed().toInt(), strList[1].trimmed().toInt());
    if ((resolution.width() < 16 && resolution.width() != 0) || resolution.height() < 16)
        return QSize();

    return resolution;
}

bool UrlParams::parse(const QUrlQuery& query)
{
    const QString positionStr = query.queryItemValue(StreamingParams::START_POS_PARAM_NAME);
    if (!positionStr.isEmpty())
        position = utils::parseDateTime(positionStr);

    QString codecStr = query.queryItemValue("codec");
    if (!codecStr.isEmpty())
    {
        codec = nx::transcoding::findEncoderCodecId(codecStr);
        if (codec == AV_CODEC_ID_NONE)
        {
            error = QString("Requested codec is not supported: [%1]").arg(codecStr);
            return false;
        }
    };

    QString resolutionStr = query.queryItemValue("resolution");
    if (!resolutionStr.isEmpty())
    {
        resolution = parseResolution(resolutionStr);
        if (!resolution->isValid())
        {
            error = QString("Invalid resolution specified: [%1]").arg(resolutionStr);
            return false;
        }
    }

    const QString& streamIndexStr = query.queryItemValue("stream");
    if (!streamIndexStr.isEmpty())
    {
        const int streamIndex = streamIndexStr.toInt();
        switch (streamIndex)
        {
        case -1: //< Default value. Same as the primary stream.
        case 0:
            quality = MEDIA_Quality_High;
            break;
        case 1:
            quality = MEDIA_Quality_Low;
            break;
        default:
            error = QString("Invalid stream specified: [%1]").arg(streamIndexStr);
            return false;
        }
    }
    if (query.hasQueryItem("onvif_replay"))
        onvifReplay = true;

    if (query.hasQueryItem("disable_fast_channel_zapping"))
        disableFastChannelZapping = true;

    return true;
}

} // namespace nx::rtsp
