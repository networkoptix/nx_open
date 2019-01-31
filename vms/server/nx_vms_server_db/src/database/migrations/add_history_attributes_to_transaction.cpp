#include "add_history_attributes_to_transaction.h"

#include <nx/fusion/model_functions.h>

#include <database/migrations/upgrade_serialized_transactions.h>

namespace ec2 {
namespace migration {
namespace add_history {

namespace after {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    HistoryAttributes,
    (ubjson),
    (author),
    (optional, false))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnAbstractTransaction,
    (ubjson),
    (command)(peerID)(persistentInfo)(transactionType)(historyAttributes),
    (optional, false))

QnAbstractTransaction& QnAbstractTransaction::operator=(
    const before::QnAbstractTransaction& right)
{
    before::QnAbstractTransaction::operator=(right);
    return *this;
}

} // namespace after

bool migrate(QSqlDatabase* const sdb)
{
    return upgradeSerializedTransactions<
        before::QnAbstractTransaction, after::QnAbstractTransaction>(sdb);
}

} // namespace add_history
} // namespace migration
} // namespace ec2
