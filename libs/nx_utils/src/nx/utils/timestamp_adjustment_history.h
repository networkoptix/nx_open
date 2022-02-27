// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>

#include <nx/utils/thread/mutex.h>

namespace nx::utils {

/**
 * Piecewise constant approximation of recent history of arbitrary mapping from abstract "source
 * clock" to "destination clock". Source clock is assumed to be generally ticking forward
 * (its timestamps increase) though it doesn't need to be actually monotonic.
 */
class NX_UTILS_API TimestampAdjustmentHistory
{
public:
    /**
     * @param maxSourceAge Records older than this (by source clock) are removed.
     */
    explicit TimestampAdjustmentHistory(std::chrono::microseconds maxSourceAge);

    /**
     * @param sourceTimestamp Source clock timestamp.
     * @param offset Offset that should be applied to sourceTimestamp to obtain
     *     corresponding target clock timestamp.
     */
    void record(
        std::chrono::microseconds sourceTimestamp,
        std::chrono::microseconds offset);

    /**
     * @return Latest known source timestamp, if any.
     */
    std::optional<std::chrono::microseconds> getLatestSourceTimestamp() const;

    /**
     * @param sourceTimestamp Source clock timestamp.
     * @return Target clock timestamp corresponding to sourceTimestamp.
     */
    std::chrono::microseconds replay(std::chrono::microseconds sourceTimestamp) const;

    void reset();

private:
    const std::chrono::microseconds m_maxSourceAge;
    mutable nx::ReadWriteLock m_lock;
    std::map<std::chrono::microseconds, std::chrono::microseconds> m_records;
};

} // namespace nx::utils
