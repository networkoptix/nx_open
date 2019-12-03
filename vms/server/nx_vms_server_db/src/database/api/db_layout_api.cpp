#include "db_layout_api.h"

#include <QtSql/QSqlQuery>

#include <nx/vms/api/data/layout_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <utils/db/db_helper.h>

#include <utils/common/id.h>

namespace ec2 {
namespace database {
namespace api {

namespace {

struct LayoutItemWithRefData: nx::vms::api::LayoutItemData
{
    QnUuid layoutId;
};
#define LayoutItemWithRefData_Fields LayoutItemData_Fields (layoutId)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LayoutItemWithRefData, (sql_record),
    LayoutItemWithRefData_Fields)

bool deleteLayoutInternal(const QSqlDatabase& database, int internalId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layout WHERE resource_ptr_id = ?
    )sql");

    QSqlQuery query(database);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool insertOrReplaceLayout(
    const QSqlDatabase& database,
    const nx::vms::api::LayoutData& layout,
    qint32 internalId)
{
    QSqlQuery query(database);
    const QString queryStr(R"sql(
        INSERT OR REPLACE
        INTO vms_layout
        (
            resource_ptr_id,
            locked,
            cell_aspect_ratio,
            cell_spacing,
            fixed_width,
            fixed_height,
            logical_id,
            background_width,
            background_height,
            background_image_filename,
            background_opacity
        ) VALUES (
            :internalId,
            :locked,
            :cellAspectRatio,
            :cellSpacing,
            :fixedWidth,
            :fixedHeight,
            :logicalId,
            :backgroundWidth,
            :backgroundHeight,
            :backgroundImageFilename,
            :backgroundOpacity
        )
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    QnSql::bind(layout, &query);
    query.bindValue(":internalId", internalId);
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool removeItems(const QSqlDatabase& database, qint32 internalId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layoutitem WHERE layout_id = ?
    )sql");

    QSqlQuery query(database);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool cleanupVideoWalls(const QSqlDatabase& database, const QnUuid &layoutId)
{
    const QString queryStr(R"sql(
        UPDATE vms_videowall_item set layout_guid = :empty_id WHERE layout_guid = :layout_id
    )sql");

    QByteArray emptyId = QnUuid().toRfc4122();

    QSqlQuery query(database);
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.bindValue(":empty_id", emptyId);
    query.bindValue(":layout_id", layoutId.toRfc4122());
    return nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool isValidItem(const nx::vms::api::LayoutItemData& item)
{
    const bool resourceIsPresent = !item.resourceId.isNull()
        || !item.resourcePath.isEmpty();

    return resourceIsPresent && !item.id.isNull();
}

bool updateItems(
    const QSqlDatabase& database,
    const nx::vms::api::LayoutData& layout,
    qint32 internalId)
{
    if (!removeItems(database, internalId))
        return false;

    QSqlQuery query(database);
    const QString queryStr(R"sql(
        INSERT INTO vms_layoutitem (
            uuid,
            resource_guid,
            path,
            layout_id,
            left,
            right,
            top,
            bottom,
            zoom_left,
            zoom_right,
            zoom_top,
            zoom_bottom,
            zoom_target_uuid,
            flags,
            rotation,
            contrast_params,
            dewarping_params,
            display_info
        ) VALUES (
            :id,
            :resourceId,
            :resourcePath,
            :layoutId,
            :left,
            :right,
            :top,
            :bottom,
            :zoomLeft,
            :zoomRight,
            :zoomTop,
            :zoomBottom,
            :zoomTargetId,
            :flags,
            :rotation,
            :contrastParams,
            :dewarpingParams,
            :displayInfo
        )
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    for (const auto& item: layout.items)
    {
        if (!NX_ASSERT(isValidItem(item), "Invalid null id item inserting"))
            continue;

        QnSql::bind(item, &query);
        query.bindValue(":layoutId", internalId);
        if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return true;
}

} // namespace

bool fetchLayouts(
    const QSqlDatabase& database,
    const QnUuid& id,
    nx::vms::api::LayoutDataList& layouts)
{
    QSqlQuery query(database);
    QString filterStr;
    if (!id.isNull())
        filterStr = lit("WHERE r.guid = %1").arg(guidToSqlString(id));
    query.setForwardOnly(true);

    QString queryStr(R"sql(
        SELECT
            r.guid as id,
            r.guid,
            r.xtype_guid as typeId,
            r.parent_guid as parentId,
            r.name,
            r.url,
            l.resource_ptr_id as id,
            l.locked,
            l.cell_aspect_ratio as cellAspectRatio,
            l.cell_spacing as cellSpacing,
            l.fixed_width as fixedWidth,
            l.fixed_height as fixedHeight,
            l.logical_id as logicalId,
            l.background_width as backgroundWidth,
            l.background_height as backgroundHeight,
            l.background_image_filename as backgroundImageFilename,
            l.background_opacity as backgroundOpacity
        FROM vms_layout l
        JOIN vms_resource r on r.id = l.resource_ptr_id
        %1
        ORDER BY r.guid
    )sql");
    queryStr = queryStr.arg(filterStr);

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QSqlQuery queryItems(database);
    queryItems.setForwardOnly(true);
    QString queryItemsStr(R"sql(
        SELECT
            r.guid as layoutId,
            li.uuid as id,
            li.resource_guid as resourceId,
            li.path as resourcePath,
            li.left,
            li.right,
            li.top,
            li.bottom,
            li.zoom_left as zoomLeft,
            li.zoom_right as zoomRight,
            li.zoom_top as zoomTop,
            li.zoom_bottom as zoomBottom,
            li.zoom_target_uuid as zoomTargetId,
            li.flags,
            li.rotation,
            li.contrast_params as contrastParams,
            li.dewarping_params as dewarpingParams,
            li.display_info as displayInfo
        FROM vms_layoutitem li
        JOIN vms_resource r on r.id = li.layout_id
        ORDER BY r.guid
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&queryItems, queryItemsStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&queryItems, Q_FUNC_INFO))
        return false;

    QnSql::fetch_many(query, &layouts);
    std::vector<LayoutItemWithRefData> items;
    QnSql::fetch_many(queryItems, &items);
    QnDbHelper::mergeObjectListData(
        layouts,
        items,
        &nx::vms::api::LayoutData::items,
        &LayoutItemWithRefData::layoutId);

    return true;
}

bool saveLayout(
    ec2::database::api::QueryContext* resourceContext,
    const nx::vms::api::LayoutData& layout)
{
    qint32 internalId;
    if (!insertOrReplaceResource(resourceContext, layout, &internalId))
        return false;

    if (!insertOrReplaceLayout(resourceContext->database(), layout, internalId))
        return false;

    return updateItems(resourceContext->database(), layout, internalId);
}

bool removeLayout(
    ec2::database::api::QueryContext* resourceContext,
    const QnUuid& id)
{
    const int internalId = api::getResourceInternalId(resourceContext, id);
    if (internalId == 0)
        return true;

    if (!removeItems(resourceContext->database(), internalId))
        return false;

    if (!cleanupVideoWalls(resourceContext->database(), id))
        return false;

    if (!deleteLayoutInternal(resourceContext->database(), internalId))
        return false;

    return deleteResourceInternal(resourceContext, internalId);
}

} // namespace api
} // namespace database
} // namespace ec2
