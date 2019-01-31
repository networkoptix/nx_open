#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QElapsedTimer>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/timestamp.h>

namespace nx::clusterdb::engine {

using Timestamp = nx::vms::api::Timestamp;

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
    quint64 m_baseTime;
    Timestamp m_lastTimestamp;
    QElapsedTimer m_relativeTimer;
    mutable QnMutex m_timeMutex;
};

} // namespace nx::clusterdb::engine
