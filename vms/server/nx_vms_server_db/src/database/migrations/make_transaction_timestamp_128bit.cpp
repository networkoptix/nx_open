#include "make_transaction_timestamp_128bit.h"

#include <nx/fusion/model_functions.h>

#include <database/migrations/upgrade_serialized_transactions.h>

namespace ec2 {
namespace migration {
namespace timestamp128bit {
namespace after {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Timestamp,
    (ubjson),
    (sequence)(ticks),
    (optional, false))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PersistentInfo,
    (ubjson),
    (dbID)(sequence)(timestamp),
    (optional, false))

//-------------------------------------------------------------------------------------------------
// QnAbstractTransaction

QnAbstractTransaction::QnAbstractTransaction():
    command(ApiCommand::NotDefined),
    transactionType(TransactionType::Regular)
{
}

QnAbstractTransaction& QnAbstractTransaction::operator=(
    const before::QnAbstractTransaction& right)
{
    command = right.command;
    peerID = right.peerID;
    transactionType = right.transactionType;

    persistentInfo.dbID = right.persistentInfo.dbID;
    persistentInfo.sequence = right.persistentInfo.sequence;
    persistentInfo.timestamp =
        Timestamp::fromInteger(right.persistentInfo.timestamp);

    return *this;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnAbstractTransaction,
    (ubjson),
    (command)(peerID)(persistentInfo)(transactionType),
    (optional, false))

} // namespace after

bool migrate(QSqlDatabase* const sdb)
{
    return upgradeSerializedTransactions<
        before::QnAbstractTransaction, after::QnAbstractTransaction>(sdb);
}

} // namespace add_history
} // namespace migration
} // namespace ec2
