// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_stream_settings.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const QString MediaStreamSettings::kMpjpegBoundary = "mjpeg_frame";

QByteArray MediaStreamSettings::getMimeType(const QString& format)
{
    if (format == "mkv")
        return "video/x-matroska";
    else if (format == "webm")
        return "video/webm";
    else if (format == "mpegts")
        return "video/mp2t";
    else if (format == "mp4")
        return "video/mp4";
    else if (format == "3gp" || format == "_3gp")
        return "video/3gp";
    else if (format == "rtp")
        return "video/3gp";
    else if (format == "flv")
        return "video/x-flv";
    else if (format == "f4v")
        return "video/x-f4v";
    else if (format == "mpjpeg")
        return "multipart/x-mixed-replace;boundary=" + kMpjpegBoundary.toUtf8();
    else
        return QByteArray();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MediaStreamSettings, (json), MediaStreamSettings_Fields)

} // namespace nx::vms::api
