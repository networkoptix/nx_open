#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/sql/sql_query_execution_helper.h>
#include <transaction/transaction.h>

#include "remove_camera_advanced_params_transactions.h"

using namespace nx::sql;

namespace ec2::db {

static const QString kSearchSql = "SELECT tran_guid, tran_data from transaction_log";
static const QString kDeleteSql = "DELETE FROM transaction_log WHERE tran_guid = ?";
static const QString kCameraAdvancedParams = "cameraAdvancedParams";

bool removeCameraAdvancedParamsTransactions(const QSqlDatabase& database)
{
    QSqlQuery searchQuery(database);
    searchQuery.setForwardOnly(true);
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&searchQuery, kSearchSql, Q_FUNC_INFO))
        return false;
    if (!SqlQueryExecutionHelper::execSQLQuery(&searchQuery, Q_FUNC_INFO))
        return false;

    QSqlQuery deleteQuery(database);
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&deleteQuery, kDeleteSql, Q_FUNC_INFO))
        return false;

    while (searchQuery.next())
    {
        QnAbstractTransaction transaction;
        const QByteArray srcData = searchQuery.value(1).toByteArray();
        QnUbjsonReader<QByteArray> stream(&srcData);
        if (!NX_ASSERT(QnUbjson::deserialize(&stream, &transaction),
                "Failed to deserialize transaction from transaction log"))
        {
            return false;
        }

        if (transaction.command != ApiCommand::setResourceParam)
            continue;

        nx::vms::api::ResourceParamWithRefData data;
        if (!NX_ASSERT(QnUbjson::deserialize(&stream, &data),
                "Failed to deserialize transaction data from transaction log"))
        {
            return false;
        }

        if (data.name == kCameraAdvancedParams)
        {
            const QnUuid tranGuid = QnSql::deserialized_field<QnUuid>(searchQuery.value(0));
            deleteQuery.addBindValue(QnSql::serialized_field(tranGuid));
            if (!SqlQueryExecutionHelper::execSQLQuery(&deleteQuery, Q_FUNC_INFO))
                return false;
        }
    }

    return true;
}

} // namespace ec2::db
