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
    if (auto it = m_lastPacketTimestamps.find(streamIndex); it != m_lastPacketTimestamps.end())
    {
        newTimestamp = streamIndex == 0
            ? processPrimary(timestamp, newTimestamp, it->second, equalAllowed, forceDiscontinuity)
            : processDependent(newTimestamp, it->second, equalAllowed);
    }
    m_lastPacketTimestamps[streamIndex] = newTimestamp;
    NX_VERBOSE(this,
        "Stream: %1 has source timestamp: %2, result pts: %3, base time: %4, offset: %5",
        streamIndex, timestamp, newTimestamp + m_offset, *m_baseTime, m_offset);
    return newTimestamp + m_offset;
}

std::chrono::microseconds TimestampCorrector::processPrimary(
    std::chrono::microseconds timestamp,
    std::chrono::microseconds newTimestamp,
    std::chrono::microseconds lastTimestamp,
    bool equalAllowed,
    bool forceDiscontinuity)
{
    auto timeDiff = newTimestamp - lastTimestamp;
    bool discontinuity = equalAllowed
        ? (timeDiff < std::chrono::microseconds::zero())
        : (timeDiff <= std::chrono::microseconds::zero());

    discontinuity |= timeDiff >= kMaxFrameDurationUs;
    if (discontinuity || forceDiscontinuity)
    {
        // Recalculate m_baseTime after seek by new frame timestamp. Timestamps should increase
        // monotonously.
        NX_DEBUG(this, "Discontinuity on packet timestamp %1, forceDiscontinuity: %3",
            timestamp, forceDiscontinuity);
        m_baseTime = timestamp - lastTimestamp - kDefaultFrameDurationUs;
        newTimestamp = timestamp - *m_baseTime;
    }

    return newTimestamp;
}

std::chrono::microseconds TimestampCorrector::processDependent(
    std::chrono::microseconds newTimestamp,
    std::chrono::microseconds lastTimestamp,
    bool equalAllowed)
{
    auto timeDiff = newTimestamp - lastTimestamp;
    bool discontinuity = equalAllowed
        ? (timeDiff < std::chrono::microseconds::zero())
        : (timeDiff <= std::chrono::microseconds::zero());

    if (discontinuity)
    {
        // Timestamps should increase monotonously.
        NX_DEBUG(this, "Packet has inconsistent timestamp %1, last timestamp %2",
            newTimestamp, lastTimestamp);
        return lastTimestamp + kDefaultFrameDurationUs;
    }
    return newTimestamp;
}
