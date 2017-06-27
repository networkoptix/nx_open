#pragma once

#include <chrono>
#include <functional>

#include <QtCore/QElapsedTimer>

#include <nx_ec/transaction_timestamp.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cdb {
namespace ec2 {

typedef ::ec2::Timestamp Timestamp;

class TransactionTimestampCalculator
{
public:
    /**
     * @param currentTimeSinceEpochFunc If not specified, current system time is used
     */
    TransactionTimestampCalculator(
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

} // namespace ec2
} // namespace cdb
} // namespace nx
