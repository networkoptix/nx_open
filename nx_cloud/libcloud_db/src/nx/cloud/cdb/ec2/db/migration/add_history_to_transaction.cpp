#include "add_history_to_transaction.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "upgrade_serialized_transactions.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace migration {
namespace addHistoryToTransaction {

nx::db::DBResult migrate(nx::db::QueryContext* const queryContext)
{
    return detail::upgradeSerializedTransactions<
        before::QnAbstractTransaction,
        after::QnAbstractTransaction>(queryContext);
}

} // namespace addHistoryToTransaction
} // namespace migration
} // namespace ec2
} // namespace cdb
} // namespace nx
