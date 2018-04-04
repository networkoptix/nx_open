#include "add_transaction_type.h"

#include <nx/fusion/model_functions.h>

#include "upgrade_serialized_transactions.h"

namespace ec2 {
namespace migration {
namespace addTransactionType {
namespace after {

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
    persistentInfo = right.persistentInfo;
    if (right.isLocal)
        transactionType = TransactionType::Local;
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
