#include "transaction_log_cache.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace cdb {
namespace ec2 {

using VmsDataState = VmsTransactionLogCache::VmsDataState;

VmsTransactionLogCache::VmsTransactionLogCache():
    timestampCalculator(std::make_unique<TransactionTimestampCalculator>()),
    m_tranIdSequence(0)
{
    committedData.timestampSequence = 0;
}

bool VmsTransactionLogCache::isShouldBeIgnored(
    const nx::String& systemId,
    const ::ec2::QnAbstractTransaction& tran,
    const QByteArray& hash) const
{
    using namespace ::ec2;

    QnMutexLocker lock(&m_mutex);

    QnTranStateKey key(tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(tran.persistentInfo.sequence != 0);
    const auto currentSequence = committedData.transactionState.values.value(key);
    if (currentSequence >= tran.persistentInfo.sequence)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lm("systemId %1. Ignoring transaction %2 (%3, hash %4)"
                "because of persistent sequence: %5 <= %6")
            .arg(systemId).arg(ApiCommand::toString(tran.command)).str(tran)
            .arg(hash).str(tran.persistentInfo.sequence).arg(currentSequence),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    const auto itr = committedData.transactionHashToUpdateAuthor.find(hash);
    if (itr == committedData.transactionHashToUpdateAuthor.cend())
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
            .arg(systemId).arg(ApiCommand::toString(tran.command)).str(tran)
            .arg(hash).str(tran.persistentInfo.timestamp).str(lastTime),
            cl_logDEBUG1);
        return true;    //< Transaction should be ignored.
    }

    return false;   //< Transaction should be processed.
}

void VmsTransactionLogCache::restoreTransaction(
    ::ec2::QnTranStateKey tranStateKey,
    int sequence,
    const nx::Buffer& tranHash,
    std::uint64_t settingsTimestampHi,
    const ::ec2::Timestamp& timestamp)
{
    QnMutexLocker lock(&m_mutex);

    *committedData.timestampSequence =
        std::max(*committedData.timestampSequence, settingsTimestampHi);
    qint32& persistentSequence = committedData.transactionState.values[tranStateKey];
    if (persistentSequence < sequence)
        persistentSequence = sequence;
    committedData.transactionHashToUpdateAuthor[tranHash] =
        UpdateHistoryData{ tranStateKey, timestamp };

    if (timestamp > m_maxTimestamp)
    {
        m_maxTimestamp = timestamp;
        timestampCalculator->init(timestamp);
    }
}

VmsTransactionLogCache::TranId VmsTransactionLogCache::beginTran()
{
    QnMutexLocker lock(&m_mutex);
    const auto newTranId = m_tranIdSequence++;
    if (!m_tranIdToContext.emplace(newTranId, TranContext()).second)
    {
        NX_CRITICAL(false);
    }
    return newTranId;
}

void VmsTransactionLogCache::commit(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);

    TranContext& tranContext = getTranContext(lock, tranId);

    for (const auto& elem: tranContext.data.transactionHashToUpdateAuthor)
        committedData.transactionHashToUpdateAuthor[elem.first] = elem.second;

    for (auto it = tranContext.data.transactionState.values.cbegin();
         it != tranContext.data.transactionState.values.cend();
         ++it)
    {
        committedData.transactionState.values[it.key()] = it.value();
    }

    if (tranContext.data.timestampSequence)
        committedData.timestampSequence = tranContext.data.timestampSequence;
}

void VmsTransactionLogCache::rollback(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    m_tranIdToContext.erase(tranId);
}

void VmsTransactionLogCache::updateTimestampSequence(TranId tranId, quint64 newValue)
{
    QnMutexLocker lock(&m_mutex);

    TranContext& tranContext = getTranContext(lock, tranId);
    tranContext.data.timestampSequence = newValue;
}

void VmsTransactionLogCache::insertOrReplaceTransaction(
    TranId tranId,
    const ::ec2::QnAbstractTransaction& transaction,
    const QByteArray& transactionHash)
{
    TranContext& tranContext = getTranContext(tranId);

    const ::ec2::QnTranStateKey tranKey(transaction.peerID, transaction.persistentInfo.dbID);

    tranContext.data.transactionHashToUpdateAuthor[transactionHash] =
        UpdateHistoryData{
            tranKey,
            transaction.persistentInfo.timestamp};
    tranContext.data.transactionState.values[tranKey] =
        transaction.persistentInfo.sequence;
}

const VmsDataState* VmsTransactionLogCache::getState(TranId tranId) const
{
    const auto tranContext = getTranContext(tranId);
    return tranContext ? &tranContext->data : nullptr;
}

::ec2::Timestamp VmsTransactionLogCache::generateNewTransactionTimestamp(TranId tranId)
{
    ::ec2::Timestamp timestamp;
    timestamp.sequence = timestampSequence(tranId);
    timestamp.ticks = timestampCalculator->calculateNextTimeStamp().ticks;
    return timestamp;
}

std::uint64_t VmsTransactionLogCache::timestampSequence(TranId tranId) const
{
    QnMutexLocker lock(&m_mutex);
    const auto tranContext = getTranContext(lock, tranId);
    NX_ASSERT(tranContext != nullptr);
    return tranContext->data.timestampSequence
        ? *tranContext->data.timestampSequence
        : *committedData.timestampSequence;
}

VmsTransactionLogCache::TranContext&
    VmsTransactionLogCache::getTranContext(TranId tranId)
{
    QnMutexLocker lock(&m_mutex);
    return getTranContext(lock, tranId);
}

VmsTransactionLogCache::TranContext& 
    VmsTransactionLogCache::getTranContext(const QnMutexLockerBase& /*lock*/, TranId tranId)
{
    return m_tranIdToContext[tranId];
}

const VmsTransactionLogCache::TranContext*
    VmsTransactionLogCache::getTranContext(TranId tranId) const
{
    QnMutexLocker lock(&m_mutex);
    return getTranContext(lock, tranId);
}

const VmsTransactionLogCache::TranContext* VmsTransactionLogCache::getTranContext(
    const QnMutexLockerBase& /*lock*/,
    TranId tranId) const
{
    auto it = m_tranIdToContext.find(tranId);
    return it != m_tranIdToContext.end() ? &it->second : nullptr;
}

} // namespace ec2
} // namespace cdb
} // namespace nx
