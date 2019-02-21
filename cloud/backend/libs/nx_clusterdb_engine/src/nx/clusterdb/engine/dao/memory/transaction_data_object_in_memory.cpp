#include "transaction_data_object_in_memory.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

namespace nx::clusterdb::engine::dao::memory {

//-------------------------------------------------------------------------------------------------
// TransactionDataObject::TransactionSourceKey

bool TransactionDataObject::TransactionSourceKey::operator<(const TransactionSourceKey& rhs) const
{
    if (systemId < rhs.systemId)
        return true;
    if (systemId > rhs.systemId)
        return false;

    if (peerId < rhs.peerId)
        return true;
    if (peerId > rhs.peerId)
        return false;

    return dbInstanceId < rhs.dbInstanceId;
}

bool TransactionDataObject::TransactionKey::operator<(const TransactionKey& rhs) const
{
    if (sourceKey < rhs.sourceKey)
        return true;
    if (rhs.sourceKey < sourceKey)
        return false;

    return sequence < rhs.sequence;
}

bool TransactionDataObject::TransactionKey::operator<=(const TransactionKey& rhs) const
{
    return !(rhs < *this);
}

//-------------------------------------------------------------------------------------------------
// TransactionDataObject

TransactionDataObject::TransactionDataObject(int transactionFormatVersion):
    m_transactionFormatVersion(transactionFormatVersion)
{
}

nx::sql::DBResult TransactionDataObject::insertOrReplaceTransaction(
    nx::sql::QueryContext* /*queryContext*/,
    const dao::TransactionData& transactionData)
{
    TransactionSourceKey sourcePeerKey{
        transactionData.systemId,
        transactionData.header.peerID.toSimpleString(),
        transactionData.header.persistentInfo.dbID.toSimpleString()};
    Transaction transaction{
        {std::move(sourcePeerKey), transactionData.header.persistentInfo.sequence},
        transactionData.hash,
        transactionData.ubjsonSerializedTransaction};

    QnMutexLocker lk(&m_mutex);

    auto insertionPair = m_transactions.insert(transaction);
    if (!insertionPair.second)
        m_transactions.replace(insertionPair.first, std::move(transaction));

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TransactionDataObject::updateTimestampHiForSystem(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& /*systemId*/,
    quint64 /*newValue*/)
{
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TransactionDataObject::fetchTransactionsOfAPeerQuery(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId,
    const QString& peerId,
    const QString& dbInstanceId,
    std::int64_t minSequence,
    std::int64_t maxSequence,
    std::vector<dao::TransactionLogRecord>* const transactions)
{
    QnMutexLocker lk(&m_mutex);

    const TransactionKey from{{systemId, peerId, dbInstanceId}, minSequence};
    const TransactionKey to{{systemId, peerId, dbInstanceId}, maxSequence};
    const auto& indexBySourceAndSequence = m_transactions.get<kIndexBySourceAndSequence>();
    auto tranIter = indexBySourceAndSequence.upper_bound(from);
    for (;
         (tranIter != indexBySourceAndSequence.end()) && (tranIter->uniqueKey <= to);
         ++tranIter)
    {
        dao::TransactionLogRecord logRecord;
        logRecord.hash = tranIter->hash;
        logRecord.serializer = std::make_unique<UbjsonTransactionPresentation>(
            tranIter->ubjsonSerializedTransaction,
            m_transactionFormatVersion);
        transactions->push_back(std::move(logRecord));
    }

    return nx::sql::DBResult::ok;
}

void TransactionDataObject::saveRecentTransactionSequence(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId,
    const std::string& peerId,
    int sequence)
{
    QnMutexLocker lk(&m_mutex);
    m_recentSequences[peerId][systemId] = sequence;
}

std::map<std::string /*systemId*/, int /*sequence*/>
    TransactionDataObject::fetchRecentTransactionSequence(
        nx::sql::QueryContext* /*queryContext*/,
        const std::string& peerId)
{
    QnMutexLocker lk(&m_mutex);
    auto it = m_recentSequences.find(peerId);
    return it != m_recentSequences.end() ? it->second : std::map<std::string, int>();
}

} // namespace nx::clusterdb::engine::dao::memory
