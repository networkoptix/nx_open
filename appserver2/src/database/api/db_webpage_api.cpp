#include "db_webpage_api.h"

#include <QtSql/QSqlQuery>

#include <database/api/db_resource_api.h>

#include <nx_ec/data/api_webpage_data.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace database {
namespace api {

bool insertOrReplaceWebPage(
    const QSqlDatabase& database,
    qint32 internalId)
{
    QSqlQuery query(database);

    const QString queryStr(R"sql(
        INSERT OR REPLACE
        INTO vms_webpage
        (
            resource_ptr_id
        ) VALUES (
            :internalId
        )
    )sql");

    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.bindValue(":internalId", internalId);
    return nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool saveWebPage(ec2::database::api::Context* resourceContext, const ApiWebPageData& webPage)
{
    qint32 internalId;
    if (!insertOrReplaceResource(resourceContext, webPage, &internalId))
        return false;

    return insertOrReplaceWebPage(resourceContext->database, internalId);
}

} // namespace api
} // namespace database
} // namespace ec2
