#include "transaction_timestamp_calculator.h"

namespace nx {
namespace cdb {
namespace ec2 {

constexpr const int kTimeShiftDelta = 1000;

TransactionTimestampCalculator::TransactionTimestampCalculator(
    std::function<std::chrono::milliseconds()> currentTimeSinceEpochFunc)
    :
    m_currentTimeSinceEpochFunc(std::move(currentTimeSinceEpochFunc)),
    m_baseTime(0)
{
    if (!m_currentTimeSinceEpochFunc)
        m_currentTimeSinceEpochFunc =
            []()
            {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch());
            };
}

void TransactionTimestampCalculator::init(Timestamp initialValue)
{
    m_lastTimestamp = initialValue;
    m_baseTime = m_lastTimestamp.ticks;
    m_relativeTimer.restart();
}

Timestamp TransactionTimestampCalculator::calculateNextTimeStamp()
{
    const qint64 absoluteTime = m_currentTimeSinceEpochFunc().count();

    Timestamp newTime = Timestamp(m_lastTimestamp.sequence, absoluteTime);

    QnMutexLocker lock(&m_timeMutex);
    if (newTime > m_lastTimestamp)
    {
        m_lastTimestamp = newTime;
        m_baseTime = absoluteTime;
        m_relativeTimer.restart();
    }
    else
    {
        newTime.ticks = m_baseTime + m_relativeTimer.elapsed();
        if (newTime > m_lastTimestamp)
        {
            if (newTime > m_lastTimestamp + kTimeShiftDelta && newTime > absoluteTime + kTimeShiftDelta)
            {
                newTime -= kTimeShiftDelta; // try to reach absolute time
                m_baseTime = newTime.ticks;
                m_relativeTimer.restart();
            }
            m_lastTimestamp = newTime;
        }
        else
        {
            m_lastTimestamp++;
            m_baseTime = m_lastTimestamp.ticks;
            m_relativeTimer.restart();
        }
    }
    return m_lastTimestamp;
}

void TransactionTimestampCalculator::reset()
{
    m_baseTime = m_currentTimeSinceEpochFunc().count();
    m_lastTimestamp = Timestamp::fromInteger(m_baseTime);
    m_relativeTimer.restart();
}

void TransactionTimestampCalculator::shiftTimestampIfNeeded(
    const Timestamp& receivedTransactionTimestamp)
{
    QnMutexLocker lock(&m_timeMutex);
    m_lastTimestamp = qMax(m_lastTimestamp, receivedTransactionTimestamp);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
