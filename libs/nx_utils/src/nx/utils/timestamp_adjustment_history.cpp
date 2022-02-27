// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "timestamp_adjustment_history.h"

namespace nx::utils {

using namespace std::literals;

TimestampAdjustmentHistory::TimestampAdjustmentHistory(std::chrono::microseconds maxSourceAge):
    m_maxSourceAge(maxSourceAge)
{
}

void TimestampAdjustmentHistory::record(
    std::chrono::microseconds sourceTimestamp,
    std::chrono::microseconds offset)
{
    NX_WRITE_LOCKER locker{&m_lock};

    m_records[sourceTimestamp] = offset;

    const auto cutoffSourceTimestamp = sourceTimestamp - m_maxSourceAge;
    if (const auto it = m_records.upper_bound(cutoffSourceTimestamp); it != m_records.begin())
        m_records.erase(m_records.begin(), std::prev(it));
}

std::optional<std::chrono::microseconds> TimestampAdjustmentHistory::getLatestSourceTimestamp() const
{
    std::optional<std::chrono::microseconds> timestamp;

    {
        NX_READ_LOCKER locker{&m_lock};
        if (const auto it = m_records.rbegin(); it != m_records.rend())
            timestamp = it->first;
    }

    return timestamp;
}

std::chrono::microseconds TimestampAdjustmentHistory::replay(
    std::chrono::microseconds sourceTimestamp) const
{
    std::chrono::microseconds offset{};

    {
        NX_READ_LOCKER locker{&m_lock};
        if (const auto it = m_records.upper_bound(sourceTimestamp); it != m_records.begin())
            offset = std::prev(it)->second;
    }

    return sourceTimestamp + offset;
}

void TimestampAdjustmentHistory::reset()
{
    NX_WRITE_LOCKER locker{&m_lock};
    m_records.clear();
}

} // namespace nx::utils
