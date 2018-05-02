#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/core/transcoding/filters/transcoding_settings.h>

#include <recording/time_period.h>

#include <nx/client/desktop/common/utils/filesystem.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings
{
    QnMediaResourcePtr mediaResource;
    QnTimePeriod timePeriod;
    Filename fileName;
    qint64 timelapseFrameStepMs = 0; //< 0 means disabled timelapse.
    qint64 serverTimeZoneMs = 0;

    nx::core::transcoding::Settings transcodingSettings;
};

using ExportOverlaySettings = nx::core::transcoding::OverlaySettings;
using ExportImageOverlaySettings = nx::core::transcoding::ImageOverlaySettings;
using ExportTimestampOverlaySettings = nx::core::transcoding::TimestampOverlaySettings;

} // namespace desktop
} // namespace client
} // namespace nx
