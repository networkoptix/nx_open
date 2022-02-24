// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtGui/QRegion>

#include <core/resource/motion_window.h>
#include <nx/vms/client/core/motion/motion_grid.h>
#include <motion/motion_detection.h>

namespace nx::vms::client::desktop {

/**
 * Information about region where motion must not be detected.
 */
class MotionSkipMask
{
public:
    MotionSkipMask(QnRegion region);

    QRegion region() const;
    const char* const bitMask() const;

private:
    QnRegion m_region;
    nx::vms::client::core::MotionGridBitMask m_bitMask;
};

} // namespace nx::vms::client::desktop
