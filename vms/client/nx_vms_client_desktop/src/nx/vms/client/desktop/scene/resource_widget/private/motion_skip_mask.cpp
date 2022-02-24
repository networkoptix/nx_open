// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "motion_skip_mask.h"

#include <nx/streaming/media_data_packet.h>

namespace nx::vms::client::desktop {

MotionSkipMask::MotionSkipMask(QnRegion region):
    m_region(std::move(region))
{
    QnMetaDataV1::createMask(m_region, reinterpret_cast<char*>(&m_bitMask));
}

QRegion MotionSkipMask::region() const
{
    return m_region;
}

const char* const MotionSkipMask::bitMask() const
{
    return reinterpret_cast<const char*>(&m_bitMask);
}

} // namespace nx::vms::client::desktop
