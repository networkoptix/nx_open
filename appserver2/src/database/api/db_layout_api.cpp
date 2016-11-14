#include "db_layout_api.h"

#include <QtSql/QSqlQuery>

#include <database/api/db_resource_api.h>

#include <nx_ec/data/api_layout_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace database {
namespace api {

bool fetchLayouts(const QSqlDatabase& database, const QnUuid& id, ApiLayoutDataList& layouts)
{
    QSqlQuery query(database);
    QString filterStr;
    if (!id.isNull())
        filterStr = lit("WHERE r.guid = %1").arg(guidToSqlString(id));
    query.setForwardOnly(true);

    QString queryStr(lit(R"(
        SELECT
        r.guid as id, r.guid, r.xtype_guid as typeId, r.parent_guid as parentId, r.name, r.url,
        l.cell_spacing_height as verticalSpacing, l.locked,
        l.cell_aspect_ratio as cellAspectRatio, l.background_width as backgroundWidth,
        l.background_image_filename as backgroundImageFilename, l.background_height as backgroundHeight,
        l.cell_spacing_width as horizontalSpacing, l.background_opacity as backgroundOpacity, l.resource_ptr_id as id
        FROM vms_layout l
        JOIN vms_resource r on r.id = l.resource_ptr_id %1 ORDER BY r.guid
        )").arg(filterStr));

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QSqlQuery queryItems(database);
    queryItems.setForwardOnly(true);
    QString queryItemsStr(R"(
        SELECT
        r.guid as layoutId, li.zoom_bottom as zoomBottom, li.right, li.uuid as id, li.zoom_left as zoomLeft, li.resource_guid as resourceId,
        li.zoom_right as zoomRight, li.top, li.bottom, li.zoom_top as zoomTop,
        li.zoom_target_uuid as zoomTargetId, li.flags, li.contrast_params as contrastParams, li.rotation, li.id,
        li.dewarping_params as dewarpingParams, li.left, li.display_info as displayInfo
        FROM vms_layoutitem li
        JOIN vms_resource r on r.id = li.layout_id order by r.guid
    )");
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

bool insertOrReplaceLayout(
    const QSqlDatabase& database,
    const ApiLayoutData& layout,
    qint32 internalId)
{
    QSqlQuery query(database);
    const QString queryStr(R"(
        INSERT OR REPLACE
        INTO vms_layout
        (
        cell_spacing_height, locked,
        cell_aspect_ratio, background_width,
        background_image_filename, background_height,
        cell_spacing_width, background_opacity, resource_ptr_id
        ) VALUES (
        :verticalSpacing, :locked,
        :cellAspectRatio, :backgroundWidth,
        :backgroundImageFilename, :backgroundHeight,
        :horizontalSpacing, :backgroundOpacity, :internalId
        )
    )");
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    QnSql::bind(layout, &query);
    query.bindValue(":internalId", internalId);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool updateLayoutItems(
    const QSqlDatabase& database,
    const ApiLayoutData& layout,
    qint32 internalId)
{
    if (!removeLayoutItems(database, internalId))
        return false;

    QSqlQuery query(database);
    const QString queryStr(R"(
        INSERT INTO vms_layoutitem (
        zoom_bottom, right, uuid, zoom_left, resource_guid,
        zoom_right, top, layout_id, bottom, zoom_top,
        zoom_target_uuid, flags, contrast_params, rotation,
        dewarping_params, left, display_info
        ) VALUES (
        :zoomBottom, :right, :id, :zoomLeft, :resourceId,
        :zoomRight, :top, :layoutId, :bottom, :zoomTop,
        :zoomTargetId, :flags, :contrastParams, :rotation,
        :dewarpingParams, :left, :displayInfo
        )
    )");

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    for (const ApiLayoutItemData& item: layout.items)
    {
        QnSql::bind(item, &query);
        query.bindValue(":layoutId", internalId);
        if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
            return false;
    }

    return true;
}

bool saveLayout(const QSqlDatabase& database, const ApiLayoutData& layout)
{
    qint32 internalId;
    if (!insertOrReplaceResource(database, layout, &internalId))
        return false;

    if (!insertOrReplaceLayout(database, layout, internalId))
        return false;

    return updateLayoutItems(database, layout, internalId);
}

bool removeLayoutItems(const QSqlDatabase& database, qint32 internalId)
{
    const QString queryStr("DELETE FROM vms_layoutitem WHERE layout_id = ?");

    QSqlQuery query(database);
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(internalId);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

} // namespace api
} // namespace database
} // namespace ec2
