#pragma once

#include <chrono>
#include <functional>

#include <nx/utils/elapsed_timer.h>
#include <nx/utils/thread/mutex.h>

#include "timestamp.h"

namespace nx::clusterdb::engine {

class CommandTimestampCalculator
{
public:
    /**
     * @param currentTimeSinceEpochFunc If not specified, current system time is used
     */
    CommandTimestampCalculator(
        std::function<std::chrono::milliseconds()> currentTimeSinceEpochFunc = nullptr);

    /** Should be initiailized with max timestamp present in transaction log. */
    void init(Timestamp initialValue);
    Timestamp calculateNextTimeStamp();
    void reset();
    void shiftTimestampIfNeeded(const Timestamp& receivedTransactionTimestamp);

private:
    std::function<std::chrono::milliseconds()> m_currentTimeSinceEpochFunc;
    std::chrono::milliseconds m_baseTime = std::chrono::milliseconds::zero();
    Timestamp m_lastTimestamp;
    nx::utils::ElapsedTimer m_relativeTimer;
    mutable QnMutex m_timeMutex;
};

} // namespace nx::clusterdb::engine
