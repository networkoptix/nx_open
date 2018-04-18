#include "access_rights_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx/utils/db/sql_query_execution_helper.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <database/db_manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log_main.h>
#include <common/common_module.h>

using namespace nx::utils::db;

namespace ec2 {
namespace db {

bool loadOldAccessRightList(const QSqlDatabase& database, ApiAccessRightsDataList& accessRightsList)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    const QString queryStr = R"(
        SELECT rights.guid as userId, resource.guid as resourceId
        FROM vms_access_rights_tmp rights
        JOIN vms_resource resource on resource.id = rights.resource_ptr_id
        ORDER BY rights.guid
    )";
    
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    ApiAccessRightsData current;
    while (query.next())
    {
        QnUuid userId = QnUuid::fromRfc4122(query.value(0).toByteArray());
        if (userId != current.userId)
        {
            if (!current.userId.isNull())
                accessRightsList.push_back(current);

            current.userId = userId;
            current.resourceIds.clear();
        }

        QnUuid resourceId = QnUuid::fromRfc4122(query.value(1).toByteArray());
        current.resourceIds.push_back(resourceId);
    }
    if (!current.userId.isNull())
        accessRightsList.push_back(current);
    
    return true;
}

bool migrateAccessRightsToUbjsonFormat(QSqlDatabase& database, detail::QnDbManager* db)
{
    ApiAccessRightsDataList accessRightsList;
    if (!loadOldAccessRightList(database, accessRightsList))
        return false;

    for (const auto& data: accessRightsList)
    {
        if (db->setAccessRights(data) != ErrorCode::ok)
            return false;
    }

    return SqlQueryExecutionHelper::execSQLScript(
        "DROP TABLE vms_access_rights_tmp;",
        database);
}

} // namespace db
} // namespace ec2
