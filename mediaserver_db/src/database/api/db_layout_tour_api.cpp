#include "db_layout_tour_api.h"

#include <QtSql/QSqlQuery>

#include <nx/vms/api/data/layout_tour_data.h>

#include <nx/fusion/model_functions.h>

#include <utils/db/db_helper.h>
#include <utils/common/id.h>

using namespace nx::vms::api;

namespace ec2 {

struct LayoutTourItemWithRefData: nx::vms::api::LayoutTourItemData
{
    LayoutTourItemWithRefData(){}

    LayoutTourItemWithRefData(const nx::vms::api::LayoutTourItemData& item, const QnUuid& tourId):
        nx::vms::api::LayoutTourItemData(item),
        tourId(tourId)
    {
    }

    QnUuid tourId;
};
#define LayoutTourItemWithRefData_Fields LayoutTourItemData_Fields (tourId)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (LayoutTourItemWithRefData),
    (sql_record),
    _Fields)

namespace {

bool insertOrReplaceTour(const QSqlDatabase& database, const LayoutTourData& tour)
{
    QSqlQuery query(database);
    const QString queryStr(R"sql(
        INSERT OR REPLACE
        INTO vms_layout_tours
        (
            id,
            parentId,
            name,
            settings
        ) VALUES (
            :id,
            :parentId,
            :name,
            :settings
        )
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    QnSql::bind(tour, &query);
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool removeTourInternal(const QSqlDatabase& database, const QnUuid& tourId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layout_tours WHERE id = ?
    )sql");

    QSqlQuery query(database);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(tourId.toRfc4122());
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool removeItems(const QSqlDatabase& database, const QnUuid& tourId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layout_tour_items WHERE tourId = ?
    )sql");

    QSqlQuery query(database);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(tourId.toRfc4122());
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool updateItems(const QSqlDatabase& database, const LayoutTourData& tour)
{
    if (!removeItems(database, tour.id))
        return false;

    QSqlQuery query(database);
    const QString queryStr(R"sql(
        INSERT INTO vms_layout_tour_items (
            tourId,
            resourceId,
            delayMs
        ) VALUES (
            :tourId,
            :resourceId,
            :delayMs
        )
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    for (const auto& item: tour.items)
    {
        LayoutTourItemWithRefData ref(item, tour.id);
        QnSql::bind(ref, &query);
        if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return true;
}

} // namespace

namespace database {
namespace api {

bool fetchLayoutTours(const QSqlDatabase& database, const QnUuid& id, LayoutTourDataList& tours)
{
    QString filterStr;
    if (!id.isNull())
        filterStr = lit("WHERE r.guid = %1").arg(guidToSqlString(id));

    QSqlQuery query(database);
    query.setForwardOnly(true);

    QString queryStr(R"sql(
        SELECT *
        FROM vms_layout_tours
        %1
        ORDER BY id
    )sql");
    queryStr = queryStr.arg(filterStr);

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QSqlQuery queryItems(database);
    queryItems.setForwardOnly(true);
    QString queryItemsStr(R"sql(
        SELECT *
        FROM vms_layout_tour_items
        ORDER BY tourId
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&queryItems, queryItemsStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&queryItems, Q_FUNC_INFO))
        return false;

    QnSql::fetch_many(query, &tours);

    std::vector<LayoutTourItemWithRefData> items;
    QnSql::fetch_many(queryItems, &items);
    QnDbHelper::mergeObjectListData(
        tours,
        items,
        &LayoutTourData::items,
        &LayoutTourItemWithRefData::tourId);

    return true;
}

bool saveLayoutTour(const QSqlDatabase& database, const LayoutTourData& tour)
{
    if (!insertOrReplaceTour(database, tour))
        return false;

    return updateItems(database, tour);
}

bool removeLayoutTour(const QSqlDatabase& database, const QnUuid& id)
{
    if (!removeItems(database, id))
        return false;

    return removeTourInternal(database, id);
}

} // namespace api
} // namespace database
} // namespace ec2
