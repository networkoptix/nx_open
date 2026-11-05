// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_data_helper.h"

#include <core/resource/resource.h>
#include <nx/vms/text/human_readable.h>

using namespace Qt::StringLiterals;
using nx::vms::text::HumanReadable;

namespace nx::vms::client::mobile {
namespace timeline {

ObjectData MotionDataHelper::makeObjectData(
    const QnTimePeriod& motion, const QnResourcePtr& resource)
{
    return ObjectData{.startTimeMs = motion.startTimeMs,
        .durationMs = motion.durationMs,
        .title = tr("Motion detected"),
        .description = HumanReadable::timeSpan(motion.duration(),
            HumanReadable::Hours | HumanReadable::Minutes | HumanReadable::Seconds,
            HumanReadable::SuffixFormat::Short,
            " "),
        .imagePath = makeImageRequest(resource, previewTimeMs(motion), kHighImageResolution),
        .iconPath = iconPath(),
        .source = QVariant::fromValue(motion)};
}

qint64 MotionDataHelper::previewTimeMs(const QnTimePeriod& motion)
{
    return motion.startTimeMs + motion.durationMs / 2;
}

QString MotionDataHelper::iconPath()
{
    return u"image://skin/20x20/Solid/motion.svg"_s;
}

} // namespace timeline
} // namespace nx::vms::client::mobile
