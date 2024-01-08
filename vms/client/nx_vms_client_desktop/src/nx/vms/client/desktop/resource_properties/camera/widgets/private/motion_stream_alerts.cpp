// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_stream_alerts.h"

#include <core/resource/camera_resource.h>

namespace nx::vms::client::desktop {

QString MotionStreamAlerts::resolutionAlert(std::optional<MotionStreamAlert> alert)
{
    return alert == MotionStreamAlert::resolutionTooHigh
        ? tr("Current stream has high resolution. Analyzing it for motion increases CPU load.")
        : QString();
}

QString MotionStreamAlerts::implicitlyDisabledAlert(
    std::optional<MotionStreamAlert> alert,
    bool multiSelection)
{
    if (!alert)
        return {};

    switch (*alert)
    {
        case MotionStreamAlert::implicitlyDisabled:
            return nx::format(tr("Motion detection is currently disabled for this camera because "
                "the video stream exceed the %1 * %2 resolution.\n"
                "You can force motion detection, but it may lead to a "
                "substantial increase in CPU load.",
                "\"\\n\" is a line break, don't change it."))
                .args(QnVirtualCameraResource::kMaximumSecondaryStreamResolution.width(),
                QnVirtualCameraResource::kMaximumSecondaryStreamResolution.height());

        case MotionStreamAlert::willBeImplicitlyDisabled:
            if (multiSelection)
            {
                return tr("Motion detection for some cameras will not be working because the "
                    "remaining stream has too high resolution.\n"
                    "You can force it, but it may significantly increase CPU load.",
                    "\"\\n\" is a line break, don't change it.");
            }

            return tr("Motion detection for this camera will not be working because the "
                "remaining stream has too high resolution.\n"
                "You can force it, but it may significantly increase CPU load.",
                "\"\\n\" is a line break, don't change it.");

        default:
            return {};
    }
}

} // namespace nx::vms::client::desktop
