#include "transaction_log_cache.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx::clusterdb::engine {

using VmsDataState = CommandLogCache::VmsDataState;

CommandLogCache::CommandLogCache():
    m_tranIdSequence(0)
{
    m_committedData.timestampSequence = 0;
    m_rawData.timestampSequence = 0;
}

bool CommandLogCache::isShouldBeIgnored(
    const std::string& systemId,
    const CommandHeader& commandHeader,
    const QByteArray& hash) const
{
    using namespace ::ec2;

    QnMutexLocker lock(&m_mutex);

    vms::api::PersistentIdData key(commandHeader.peerID, commandHeader.persistentInfo.dbID);
    NX_ASSERT(commandHeader.persistentInfo.sequence != 0);
    const auto currentSequence = m_committedData.transactionState.values.value(key);
    if (currentSequence >= commandHeader.persistentInfo.sequence)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG, lm("systemId %1. Ignoring transaction (%2, hash %3) "
            "because of persistent sequence: %4 <= %5")
            .args(systemId, engine::toString(commandHeader), hash,
                commandHeader.persistentInfo.sequence, currentSequence));
        return true;    //< Transaction should be ignored.
    }

    const auto itr = m_committedData.transactionHashToUpdateAuthor.find(hash);
    if (itr == m_committedData.transactionHashToUpdateAuthor.cend())
        return false;   //< Transaction should be processed.

    const auto lastTime = itr->second.timestamp;
    bool rez = lastTime > commandHeader.persistentInfo.timestamp;
    if (lastTime == commandHeader.persistentInfo.timestamp)
        rez = key < itr->second.updatedBy;
    if (rez)
    {
        NX_DEBUG(QnLog::EC2_TRAN_LOG, lm("systemId %1. Ignoring transaction (%2 hash %3) "
            "because of timestamp: %4 <= %5").args(systemId, engine::toString(commandHeader), hash,
                commandHeader.persistentInfo.timestamp, lastTime));
        return true;    //< Transaction should be ignored.
    }

    return false;   //< Transaction should be processed.
}

void CommandLogCache::restoreTransaction(
    vms::api::PersistentIdData tranStateKey,
    int sequence,
    const nx::Buffer& tranHash,
    const vms::api::Timestamp& timestamp)
{
    QnMutexLocker lock(&m_mutex);

    if (m_committedData.timestampSequence)
    {
        m_committedData.timestampSequence =
            std::max<std::uint64_t>(*m_committedData.timestampSequence, timestamp.sequence);
    }
    else
    {
        m_committedData.timestampSequence = timestamp.sequence;
    }

    qint32& persistentSequence = m_committedData.transactionState.values[tranStateKey];
    if (persistentSequence < sequence)
        persistentSequence = sequence;
    m_committedData.transactionHashToUpdateAuthor[tranHash] =
        UpdateHistoryData{ tranStateKey, timestamp };

    if (timestamp > m_maxTimestamp)
    {
        m_maxTimestamp = timestamp;
        m_timestampCalculator.init(timestamp);
    }

    NX_ASSERT(m_tranIdToContext.empty());
    m_rawData = m_committedData;
}

CommandLogCache::TranId CommandLogCache::beginTran()
{
    QnMutexLocker lock(&m_mutex);
    const auto newTranId = ++m_tranIdSequence;
    if (!m_tranIdToContext.emplace(newTranId, TranContext()).second)
    {
        NX_CRITICAL(false);
    }
    return newTranId;
}

void CommandLogCache::commit(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);

    TranContext& tranContext = findTranContext(lock, tranId);

    for (const auto& elem: tranContext.data.transactionHashToUpdateAuthor)
        m_committedData.transactionHashToUpdateAuthor[elem.first] = elem.second;

    for (auto it = tranContext.data.transactionState.values.cbegin();
         it != tranContext.data.transactionState.values.cend();
         ++it)
    {
        auto& currentSequence = m_committedData.transactionState.values[it.key()];
        if (currentSequence < it.value())
            currentSequence = it.value();
    }

    if (tranContext.data.timestampSequence)
    {
        m_committedData.timestampSequence = std::max(
            m_committedData.timestampSequence,
            tranContext.data.timestampSequence);

        m_timestampCalculator.shiftTimestampIfNeeded(Timestamp(
            *m_committedData.timestampSequence,
            m_timestampCalculator.calculateNextTimeStamp().ticks));
    }

    m_tranIdToContext.erase(tranId);
}

void CommandLogCache::rollback(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    m_tranIdToContext.erase(tranId);
}

void CommandLogCache::updateTimestampSequence(TranId tranId, quint64 newValue)
{
    QnMutexLocker lock(&m_mutex);

    TranContext& tranContext = findTranContext(lock, tranId);
    tranContext.data.timestampSequence = newValue;
}

void CommandLogCache::insertOrReplaceTransaction(
    TranId tranId,
    const CommandHeader& transaction,
    const QByteArray& transactionHash)
{
    QnMutexLocker lock(&m_mutex);
    TranContext& tranContext = findTranContext(lock, tranId);

    m_timestampCalculator.shiftTimestampIfNeeded(transaction.persistentInfo.timestamp);

    const vms::api::PersistentIdData tranKey(
        transaction.peerID,
        transaction.persistentInfo.dbID);

    tranContext.data.timestampSequence = transaction.persistentInfo.timestamp.sequence;
    tranContext.data.transactionHashToUpdateAuthor[transactionHash] =
        UpdateHistoryData{
            tranKey,
            transaction.persistentInfo.timestamp};
    tranContext.data.transactionState.values[tranKey] =
        transaction.persistentInfo.sequence;
}

const VmsDataState* CommandLogCache::state(TranId tranId) const
{
    const auto tranContext = findTranContext(tranId);
    return tranContext ? &tranContext->data : nullptr;
}

vms::api::Timestamp CommandLogCache::generateTransactionTimestamp(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    vms::api::Timestamp timestamp = m_timestampCalculator.calculateNextTimeStamp();
    timestamp.sequence = std::max<decltype(timestamp.sequence)>(
        timestamp.sequence,
        timestampSequence(lock, tranId)); //< Increased timestamp sequence can still be not commited.
    return timestamp;
}

int CommandLogCache::generateTransactionSequence(
    const vms::api::PersistentIdData& tranStateKey)
{
    QnMutexLocker lock(&m_mutex);
    int& currentSequence = m_rawData.transactionState.values[tranStateKey];
    ++currentSequence;
    return currentSequence;
}

void CommandLogCache::shiftTransactionSequence(
    const vms::api::PersistentIdData& tranStateKey,
    int delta)
{
    // TODO: #ak Get rid of this method.

    QnMutexLocker lock(&m_mutex);

    int& currentSequence = m_committedData.transactionState.values[tranStateKey];
    currentSequence += delta;

    m_rawData = m_committedData;
}

vms::api::TranState CommandLogCache::committedTransactionState() const
{
    QnMutexLocker lock(&m_mutex);
    return m_committedData.transactionState;
}

std::uint64_t CommandLogCache::committedTimestampSequence() const
{
    QnMutexLocker lock(&m_mutex);
    return *m_committedData.timestampSequence;
}

int CommandLogCache::activeTransactionCount() const
{
    QnMutexLocker lock(&m_mutex);
    return (int) m_tranIdToContext.size();
}

std::uint64_t CommandLogCache::timestampSequence(
    const QnMutexLockerBase& lock, TranId tranId) const
{
    if (tranId == InvalidTranId)
        return *m_committedData.timestampSequence;

    const auto tranContext = findTranContext(lock, tranId);
    NX_ASSERT(tranContext != nullptr);
    return tranContext->data.timestampSequence
        ? *tranContext->data.timestampSequence
        : *m_committedData.timestampSequence;
}

CommandLogCache::TranContext&
    CommandLogCache::findTranContext(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    return findTranContext(lock, tranId);
}

CommandLogCache::TranContext&
    CommandLogCache::findTranContext(const QnMutexLockerBase& /*lock*/, TranId tranId)
{
    return m_tranIdToContext[tranId];
}

const CommandLogCache::TranContext*
    CommandLogCache::findTranContext(TranId tranId) const
{
    QnMutexLocker lock(&m_mutex);
    return findTranContext(lock, tranId);
}

const CommandLogCache::TranContext* CommandLogCache::findTranContext(
    const QnMutexLockerBase& /*lock*/,
    TranId tranId) const
{
    auto it = m_tranIdToContext.find(tranId);
    return it != m_tranIdToContext.end() ? &it->second : nullptr;
}

} // namespace nx::clusterdb::engine
