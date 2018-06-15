#include "transaction_log_cache.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace data_sync_engine {

using VmsDataState = VmsTransactionLogCache::VmsDataState;

VmsTransactionLogCache::VmsTransactionLogCache():
    m_tranIdSequence(0)
{
    m_committedData.timestampSequence = 0;
}

bool VmsTransactionLogCache::isShouldBeIgnored(
    const nx::String& systemId,
    const CommandHeader& tran,
    const QByteArray& hash) const
{
    using namespace ::ec2;

    QnMutexLocker lock(&m_mutex);

    vms::api::PersistentIdData key(tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(tran.persistentInfo.sequence != 0);
    const auto currentSequence = m_committedData.transactionState.values.value(key);
    if (currentSequence >= tran.persistentInfo.sequence)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Ignoring transaction %2 (%3, hash %4)"
                "because of persistent sequence: %5 <= %6")
            .arg(systemId).arg(ApiCommand::toString(tran.command)).arg(tran)
            .arg(hash).arg(tran.persistentInfo.sequence).arg(currentSequence),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    const auto itr = m_committedData.transactionHashToUpdateAuthor.find(hash);
    if (itr == m_committedData.transactionHashToUpdateAuthor.cend())
        return false;   //< Transaction should be processed.

    const auto lastTime = itr->second.timestamp;
    bool rez = lastTime > tran.persistentInfo.timestamp;
    if (lastTime == tran.persistentInfo.timestamp)
        rez = key < itr->second.updatedBy;
    if (rez)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Ignoring transaction %2 (%3, hash %4)"
                "because of timestamp: %5 <= %6")
            .arg(systemId).arg(ApiCommand::toString(tran.command)).arg(tran)
            .arg(hash).arg(tran.persistentInfo.timestamp).arg(lastTime),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    return false;   //< Transaction should be processed.
}

void VmsTransactionLogCache::restoreTransaction(
    vms::api::PersistentIdData tranStateKey,
    int sequence,
    const nx::Buffer& tranHash,
    const ::ec2::Timestamp& timestamp)
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
}

VmsTransactionLogCache::TranId VmsTransactionLogCache::beginTran()
{
    QnMutexLocker lock(&m_mutex);
    const auto newTranId = ++m_tranIdSequence;
    if (!m_tranIdToContext.emplace(newTranId, TranContext()).second)
    {
        NX_CRITICAL(false);
    }
    return newTranId;
}

void VmsTransactionLogCache::commit(TranId tranId)
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
    }
}

void VmsTransactionLogCache::rollback(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    m_tranIdToContext.erase(tranId);
}

void VmsTransactionLogCache::updateTimestampSequence(TranId tranId, quint64 newValue)
{
    QnMutexLocker lock(&m_mutex);

    TranContext& tranContext = findTranContext(lock, tranId);
    tranContext.data.timestampSequence = newValue;
}

void VmsTransactionLogCache::insertOrReplaceTransaction(
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

const VmsDataState* VmsTransactionLogCache::state(TranId tranId) const
{
    const auto tranContext = findTranContext(tranId);
    return tranContext ? &tranContext->data : nullptr;
}

::ec2::Timestamp VmsTransactionLogCache::generateTransactionTimestamp(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    ::ec2::Timestamp timestamp;
    timestamp.sequence = timestampSequence(lock, tranId);
    timestamp.ticks = m_timestampCalculator.calculateNextTimeStamp().ticks;
    return timestamp;
}

int VmsTransactionLogCache::generateTransactionSequence(
    const vms::api::PersistentIdData& tranStateKey)
{
    QnMutexLocker lock(&m_mutex);
    int& currentSequence = m_committedData.transactionState.values[tranStateKey];
    ++currentSequence;
    return currentSequence;
}

void VmsTransactionLogCache::shiftTransactionSequence(
    const vms::api::PersistentIdData& tranStateKey,
    int delta)
{
    QnMutexLocker lock(&m_mutex);
    int& currentSequence = m_committedData.transactionState.values[tranStateKey];
    currentSequence += delta;
}

vms::api::TranState VmsTransactionLogCache::committedTransactionState() const
{
    QnMutexLocker lock(&m_mutex);
    return m_committedData.transactionState;
}

std::uint64_t VmsTransactionLogCache::committedTimestampSequence() const
{
    QnMutexLocker lock(&m_mutex);
    return *m_committedData.timestampSequence;
}

std::uint64_t VmsTransactionLogCache::timestampSequence(
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

VmsTransactionLogCache::TranContext&
    VmsTransactionLogCache::findTranContext(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    return findTranContext(lock, tranId);
}

VmsTransactionLogCache::TranContext&
    VmsTransactionLogCache::findTranContext(const QnMutexLockerBase& /*lock*/, TranId tranId)
{
    return m_tranIdToContext[tranId];
}

const VmsTransactionLogCache::TranContext*
    VmsTransactionLogCache::findTranContext(TranId tranId) const
{
    QnMutexLocker lock(&m_mutex);
    return findTranContext(lock, tranId);
}

const VmsTransactionLogCache::TranContext* VmsTransactionLogCache::findTranContext(
    const QnMutexLockerBase& /*lock*/,
    TranId tranId) const
{
    auto it = m_tranIdToContext.find(tranId);
    return it != m_tranIdToContext.end() ? &it->second : nullptr;
}

} // namespace data_sync_engine
} // namespace nx
