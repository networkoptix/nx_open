#include "transaction_data_object_in_memory.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {
namespace memory {

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


//-------------------------------------------------------------------------------------------------
// TransactionDataObject

nx::utils::db::DBResult TransactionDataObject::insertOrReplaceTransaction(
    nx::utils::db::QueryContext* /*queryContext*/,
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

    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult TransactionDataObject::updateTimestampHiForSystem(
    nx::utils::db::QueryContext* /*queryContext*/,
    const nx::String& /*systemId*/,
    quint64 /*newValue*/)
{
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult TransactionDataObject::fetchTransactionsOfAPeerQuery(
    nx::utils::db::QueryContext* /*queryContext*/,
    const nx::String& systemId,
    const QString& peerId,
    const QString& dbInstanceId,
    std::int64_t minSequence,
    std::int64_t maxSequence,
    std::vector<dao::TransactionLogRecord>* const transactions)
{
    QnMutexLocker lk(&m_mutex);

    const TransactionKey from{systemId, peerId, dbInstanceId, minSequence};
    const TransactionKey to{systemId, peerId, dbInstanceId, maxSequence};
    const auto& indexBySourceAndSequence = m_transactions.get<kIndexBySourceAndSequence>();
    auto tranIter = indexBySourceAndSequence.lower_bound(from);
    for (;
         (tranIter != indexBySourceAndSequence.end()) && (tranIter->uniqueKey < to);
         ++tranIter)
    {
        dao::TransactionLogRecord logRecord;
        logRecord.hash = tranIter->hash;
        logRecord.serializer = std::make_unique<UbjsonTransactionPresentation>(
            tranIter->ubjsonSerializedTransaction,
            nx_ec::EC2_PROTO_VERSION);
        transactions->push_back(std::move(logRecord));
    }

    return nx::utils::db::DBResult::ok;
}

} // namespace memory
} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
