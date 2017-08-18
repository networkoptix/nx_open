#pragma once

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>
#include <transcoding/filters/filter_helper.h>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings
{
    QnMediaResourcePtr mediaResource;
    QnTimePeriod timePeriod;
    QString fileName;
    QnImageFilterHelper imageParameters;
    qint64 serverTimeZoneMs = 0;
    qint64 timelapseFrameStepMs = 0; //< 0 means disabled timelapse.
};

} // namespace desktop
} // namespace client
} // namespace nx
