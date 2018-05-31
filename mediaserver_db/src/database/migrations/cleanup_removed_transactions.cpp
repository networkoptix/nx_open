#include "cleanup_removed_transactions.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <database/migrations/legacy_transaction_migration.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/db/sql_query_execution_helper.h>

using namespace nx::utils::db;

namespace {

static const QSet<int> kRemovedTransactions{
    4001 /*getClientInfoList*/,
    4002 /*saveClientInfo*/,
};

} // namespace

namespace ec2 {
namespace db {

namespace detail {

struct AbstractTransactionCommand
{
    ApiCommand::Value command = ApiCommand::NotDefined;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AbstractTransactionCommand, (ubjson), (command))

}


bool cleanupClientInfoList(const QSqlDatabase& database)
{
    QSqlQuery searchQuery(database);
    searchQuery.setForwardOnly(true);

    const QString searchSqlText = "SELECT tran_guid, tran_data from transaction_log";
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&searchQuery, searchSqlText, Q_FUNC_INFO))
        return false;
    if (!SqlQueryExecutionHelper::execSQLQuery(&searchQuery, Q_FUNC_INFO))
        return false;

    QSqlQuery deleteQuery(database);
    const QString deleteSqlText = "DELETE FROM transaction_log WHERE tran_guid = ?";
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&deleteQuery, deleteSqlText, Q_FUNC_INFO))
        return false;

    while (searchQuery.next())
    {
        detail::AbstractTransactionCommand abstractTran;
        QByteArray srcData = searchQuery.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!QnUbjson::deserialize(&stream, &abstractTran))
        {
            qWarning() << Q_FUNC_INFO << "Can' deserialize transaction from transaction log";
            return false;
        }

        if (!kRemovedTransactions.contains(abstractTran.command))
            continue;

        QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(searchQuery.value(0));
        deleteQuery.addBindValue(QnSql::serialized_field(tranGuid));
        if (!SqlQueryExecutionHelper::execSQLQuery(&deleteQuery, Q_FUNC_INFO))
            return false;
    }

    return true;
}

} // namespace db
} // namespace ec2
