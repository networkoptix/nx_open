// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timestamp_stitcher.h"

#include <utils/common/util.h>

namespace nx::recording::helpers {

std::chrono::microseconds TimestampStitcher::process(std::chrono::microseconds timestamp)
{
    if (m_firstFrame)
    {
        m_firstFrame = false;
        m_lastInputTimestamp = timestamp;
        m_lastOutputTimestamp = timestamp;
        return m_lastOutputTimestamp;
    }

    if (timestamp > m_lastInputTimestamp && timestamp - m_lastInputTimestamp < MAX_FRAME_DURATION)
        m_duration = timestamp - m_lastInputTimestamp;

    m_lastInputTimestamp = timestamp;
    m_lastOutputTimestamp += m_duration;
    return m_lastOutputTimestamp;
}

} // namespace nx::recording::helpers
