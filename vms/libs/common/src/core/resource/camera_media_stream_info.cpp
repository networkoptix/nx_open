#include "camera_media_stream_info.h"

#include <nx/fusion/model_functions.h>

const QLatin1String CameraMediaStreamInfo::anyResolution("*");

QString CameraMediaStreamInfo::resolutionToString(const QSize& resolution)
{
    if (!resolution.isValid())
        return anyResolution;

    return QString::fromLatin1("%1x%2").arg(resolution.width()).arg(resolution.height());
}

bool CameraMediaStreamInfo::operator==(const CameraMediaStreamInfo& rhs) const
{
    return transcodingRequired == rhs.transcodingRequired
        && codec == rhs.codec
        && encoderIndex == rhs.encoderIndex
        && resolution == rhs.resolution
        && transports == rhs.transports
        && customStreamParams == rhs.customStreamParams;
}

bool CameraMediaStreamInfo::operator!=(const CameraMediaStreamInfo& rhs) const
{
    return !(*this == rhs);
}

QSize CameraMediaStreamInfo::getResolution() const
{
    QStringList tmp = resolution.split(L'x');
    if (tmp.size() == 2)
        return QSize(tmp[0].toInt(), tmp[1].toInt());
    else
        return QSize();
}

nx::vms::api::MotionStreamType CameraMediaStreamInfo::getEncoderIndex() const
{
    return static_cast<nx::vms::api::MotionStreamType>(encoderIndex);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
(CameraMediaStreamInfo)(CameraMediaStreams)(CameraBitrateInfo)(CameraBitrates),
(json),
_Fields)
