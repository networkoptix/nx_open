#include "db_layout_tour_api.h"

#include <QtSql/QSqlQuery>

#include <nx_ec/data/api_layout_tour_data.h>

#include <nx/fusion/model_functions.h>

#include <utils/db/db_helper.h>

namespace ec2 {

struct ApiLayoutTourItemWithRefData: ApiLayoutTourItemData
{
    ApiLayoutTourItemWithRefData(){}

    ApiLayoutTourItemWithRefData(const ApiLayoutTourItemData& item, const QnUuid& tourId):
        ApiLayoutTourItemData(item),
        tourId(tourId)
    {
    }

    QnUuid tourId;
};
#define ApiLayoutTourItemWithRefData_Fields ApiLayoutTourItemData_Fields (tourId)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiLayoutTourItemWithRefData),
    (sql_record),
    _Fields)

namespace {

bool insertOrReplaceTour(const QSqlDatabase& database, const ApiLayoutTourData& tour)
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

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    QnSql::bind(tour, &query);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool removeTourInternal(const QSqlDatabase& database, const QnUuid& tourId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layout_tours WHERE id = ?
    )sql");

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(tourId.toRfc4122());
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool removeItems(const QSqlDatabase& database, const QnUuid& tourId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layout_tour_items WHERE tourId = ?
    )sql");

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(tourId.toRfc4122());
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool updateItems(const QSqlDatabase& database, const ApiLayoutTourData& tour)
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

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    for (const ApiLayoutTourItemData& item: tour.items)
    {
        ApiLayoutTourItemWithRefData ref(item, tour.id);
        QnSql::bind(ref, &query);
        if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return true;
}

} // namespace

namespace database {
namespace api {

bool fetchLayoutTours(const QSqlDatabase& database, ApiLayoutTourDataList& tours)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    QString queryStr(R"sql(
        SELECT *
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
        SELECT *
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
        &ApiLayoutTourItemWithRefData::tourId);

    return true;
}

bool saveLayoutTour(const QSqlDatabase& database, const ApiLayoutTourData& tour)
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
