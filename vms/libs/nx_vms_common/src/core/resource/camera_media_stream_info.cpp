// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_media_stream_info.h"

#include <nx/fusion/model_functions.h>

const QLatin1String CameraMediaStreamInfo::anyResolution("*");

QString CameraMediaStreamInfo::resolutionToString(const QSize& resolution)
{
    if (!resolution.isValid())
        return anyResolution;

    return QString::fromLatin1("%1x%2").arg(resolution.width()).arg(resolution.height());
}

QSize CameraMediaStreamInfo::getResolution() const
{
    const auto delimiter = resolution.indexOf('x');
    if (delimiter == -1)
        return QSize();
    return QSize(resolution.left(delimiter).toInt(), resolution.mid(delimiter+1).toInt());
}

std::optional<double> CameraMediaStreamInfo::getAspectRatio() const
{
    if (const auto resolution = getResolution(); resolution.isValid())
        return (double)(resolution.width()) / resolution.height();
    return std::nullopt;
}

nx::vms::api::StreamIndex CameraMediaStreamInfo::getEncoderIndex() const
{
    return static_cast<nx::vms::api::StreamIndex>(encoderIndex);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraMediaStreamInfo, (json), CameraMediaStreamInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraMediaStreams, (json), CameraMediaStreams_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraBitrateInfo, (json), CameraBitrateInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraBitrates, (json), CameraBitrates_Fields)
