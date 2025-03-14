// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timestamp_corrector.h"

#include <nx/utils/log/log.h>

TimestampCorrector::TimestampCorrector(
    std::chrono::microseconds maxFrameDurationUs,
    std::chrono::microseconds defaultFrameDurationUs)
    :
    kMaxFrameDurationUs(maxFrameDurationUs),
    kDefaultFrameDurationUs(defaultFrameDurationUs)
{}

void TimestampCorrector::setOffset(std::chrono::microseconds value)
{
    m_offset = value;
}

void TimestampCorrector::clear()
{
    m_lastPacketTimestamps.clear();
    m_baseTime.reset();
}

std::chrono::microseconds TimestampCorrector::process(
    std::chrono::microseconds timestamp, int streamIndex, bool equalAllowed, bool forceDiscontinuity)
{
    if (!m_baseTime.has_value())
        m_baseTime = timestamp;

    auto newTimestamp = timestamp - *m_baseTime;
    if (auto it = m_lastPacketTimestamps.find(streamIndex);
        it != m_lastPacketTimestamps.end())
    {
        auto timeDiff = newTimestamp - it->second;
        bool discontinuity = equalAllowed
            ? (timeDiff < std::chrono::microseconds::zero())
            : (timeDiff <= std::chrono::microseconds::zero());

        discontinuity |= timeDiff >= kMaxFrameDurationUs;
        if (discontinuity || forceDiscontinuity)
        {
            // Recalculate m_baseTime after seek by new frame timestamp. Timestamps should increase
            // monotonously.
            NX_DEBUG(this, "Discontinuity on packet %1, last timestamp %2, stream: %3, baseTime: %4",
                newTimestamp, it->second, streamIndex, m_baseTime);
            m_baseTime = timestamp - it->second - kDefaultFrameDurationUs;
            newTimestamp = timestamp - *m_baseTime;
        }
    }
    m_lastPacketTimestamps[streamIndex] = newTimestamp;
    return newTimestamp + m_offset;
}
