#include "add_history_to_transaction.h"

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include "upgrade_serialized_transactions.h"

namespace nx {
namespace data_sync_engine {
namespace migration {
namespace addHistoryToTransaction {

nx::utils::db::DBResult migrate(nx::utils::db::QueryContext* const queryContext)
{
    return detail::upgradeSerializedTransactions<
        before::QnAbstractTransaction,
        after::QnAbstractTransaction>(queryContext);
}

} // namespace addHistoryToTransaction
} // namespace migration
} // namespace data_sync_engine
} // namespace nx
