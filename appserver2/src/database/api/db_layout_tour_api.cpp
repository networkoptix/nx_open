#include "db_layout_tour_api.h"

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_layout_tour_data.h>

#include <nx/fusion/model_functions.h>

#include <utils/db/db_helper.h>

namespace ec2 {

struct ApiLayoutTourItemWithRefData: ApiLayoutTourItemData
{
    QnUuid tourId;
};
#define ApiLayoutTourItemWithRefData_Fields ApiLayoutTourItemData_Fields (tourId)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiLayoutTourItemWithRefData, (sql_record),
    ApiLayoutTourItemWithRefData_Fields)

namespace database {
namespace api {

bool fetchLayoutTours(const QSqlDatabase& database, ApiLayoutTourDataList& tours)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    QString queryStr(R"sql(
        SELECT
            id,
            name
        FROM vms_layout_tours
        ORDER BY id
    )sql");

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QSqlQuery queryItems(database);
    queryItems.setForwardOnly(true);
    QString queryItemsStr(R"sql(
        SELECT
            tourId,
            layoutId,
            delayMs
        FROM vms_layout_tour_items
        ORDER BY tourId
    )sql");

    if (!QnDbHelper::prepareSQLQuery(&queryItems, queryItemsStr, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&queryItems, Q_FUNC_INFO))
        return false;

    QnSql::fetch_many(query, &tours);
    std::vector<ApiLayoutTourItemWithRefData> items;
    QnSql::fetch_many(queryItems, &items);
    QnDbHelper::mergeObjectListData(
        tours,
        items,
        &ApiLayoutTourData::items,
        &ApiLayoutTourItemWithRefData::layoutId);

    return true;
}

} // namespace api
} // namespace database
} // namespace ec2
