#include "db_layout_api.h"

#include <QtSql/QSqlQuery>

#include <database/api/db_resource_api.h>

#include <nx_ec/data/api_layout_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <utils/db/db_helper.h>

namespace ec2 {

struct ApiLayoutItemWithRefData: ApiLayoutItemData
{
    QnUuid layoutId;
};
#define ApiLayoutItemWithRefData_Fields ApiLayoutItemData_Fields (layoutId)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ApiLayoutItemWithRefData, (sql_record),
    ApiLayoutItemWithRefData_Fields)

namespace database {
namespace api {

namespace {

bool deleteLayoutInternal(const QSqlDatabase& database, int internalId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layout WHERE resource_ptr_id = ?
    )sql");

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool insertOrReplaceLayout(const QSqlDatabase& database, const ApiLayoutData& layout, qint32 internalId)
{
    QSqlQuery query(database);
    const QString queryStr(R"sql(
        INSERT OR REPLACE
        INTO vms_layout
        (
            resource_ptr_id,
            locked,
            cell_aspect_ratio,
            cell_spacing_width,
            cell_spacing_height,
            background_width,
            background_height,
            background_image_filename,
            background_opacity
        ) VALUES (
            :internalId,
            :locked,
            :cellAspectRatio,
            :horizontalSpacing,
            :verticalSpacing,
            :backgroundWidth,
            :backgroundHeight,
            :backgroundImageFilename,
            :backgroundOpacity
        )
    )sql");

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    QnSql::bind(layout, &query);
    query.bindValue(":internalId", internalId);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool removeItems(const QSqlDatabase& database, qint32 internalId)
{
    const QString queryStr(R"sql(
        DELETE FROM vms_layoutitem WHERE layout_id = ?
    )sql");

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool cleanupVideoWalls(const QSqlDatabase& database, const QnUuid &layoutId)
{
    const QString queryStr(R"sql(
        UPDATE vms_videowall_item set layout_guid = :empty_id WHERE layout_guid = :layout_id
    )sql");

    QByteArray emptyId = QnUuid().toRfc4122();

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.bindValue(":empty_id", emptyId);
    query.bindValue(":layout_id", layoutId.toRfc4122());
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool updateItems(const QSqlDatabase& database, const ApiLayoutData& layout, qint32 internalId)
{
    if (!removeItems(database, internalId))
        return false;

    QSqlQuery query(database);
    const QString queryStr(R"sql(
        INSERT INTO vms_layoutitem (
            uuid,
            resource_guid,
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

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    for (const ApiLayoutItemData& item : layout.items)
    {
        NX_ASSERT(!item.id.isNull(), "Invalid null id item inserting");
        QnSql::bind(item, &query);
        query.bindValue(":layoutId", internalId);
        if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return true;
}

} // namespace

bool fetchLayouts(const QSqlDatabase& database, const QnUuid& id, ApiLayoutDataList& layouts)
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
            l.cell_spacing_width as horizontalSpacing,
            l.cell_spacing_height as verticalSpacing,
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

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QSqlQuery queryItems(database);
    queryItems.setForwardOnly(true);
    QString queryItemsStr(R"sql(
        SELECT
            r.guid as layoutId,
            li.uuid as id,
            li.resource_guid as resourceId,
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

    if (!QnDbHelper::prepareSQLQuery(&queryItems, queryItemsStr, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&queryItems, Q_FUNC_INFO))
        return false;

    QnSql::fetch_many(query, &layouts);
    std::vector<ApiLayoutItemWithRefData> items;
    QnSql::fetch_many(queryItems, &items);
    QnDbHelper::mergeObjectListData(layouts, items, &ApiLayoutData::items, &ApiLayoutItemWithRefData::layoutId);

    return true;
}

bool saveLayout(
    ec2::database::api::Context* resourceContext,
    const ApiLayoutData& layout)
{
    qint32 internalId;
    if (!insertOrReplaceResource(resourceContext, layout, &internalId))
        return false;

    if (!insertOrReplaceLayout(resourceContext->database, layout, internalId))
        return false;

    return updateItems(resourceContext->database, layout, internalId);
}

bool removeLayout(
    ec2::database::api::Context* resourceContext,
    const QnUuid& id)
{
    int internalId = api::getResourceInternalId(resourceContext, id);
    if (internalId == 0)
        return true;

    if (!removeItems(resourceContext->database, internalId))
        return false;

    if (!cleanupVideoWalls(resourceContext->database, id))
        return false;

    if (!deleteLayoutInternal(resourceContext->database, internalId))
        return false;

    return deleteResourceInternal(resourceContext, internalId);
}

} // namespace api
} // namespace database
} // namespace ec2
