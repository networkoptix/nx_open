#include "reparent_videowall_layouts.h"

#include <QtSql/QSqlQuery>

#include <database/api/db_layout_api.h>

#include <nx_ec/data/api_layout_data.h>

#include <nx/utils/uuid.h>

#include <utils/db/db_helper.h>

namespace ec2 {
namespace database {
namespace migrations {

struct LayoutOnVideoWall
{
    QnUuid itemId;
    QnUuid layoutId;
    QnUuid videoWallId;
};

bool getLayoutsOnVideoWalls(const QSqlDatabase& database, QList<LayoutOnVideoWall>& data)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    const QString queryStr = R"sql(
        SELECT
            item.guid as itemId,
            item.layout_guid as layoutId,
            item.videowall_guid as videoWallId
        FROM vms_videowall_item item
    )sql";

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    while (query.next())
    {
        LayoutOnVideoWall item;
        item.layoutId = QnUuid::fromRfc4122(query.value("layoutId").toByteArray());
        if (item.layoutId.isNull())
            continue;

        item.itemId = QnUuid::fromRfc4122(query.value("itemId").toByteArray());
        item.videoWallId = QnUuid::fromRfc4122(query.value("videoWallId").toByteArray());
        NX_ASSERT(!item.videoWallId.isNull());
        if (item.videoWallId.isNull())
            continue;

        data.push_back(item);
    }

    return true;
}

bool updateVideowallItem(const QSqlDatabase& database, const LayoutOnVideoWall& item)
{
    QSqlQuery query(database);

    const QString queryStr(R"sql(
        UPDATE vms_videowall_item
        SET layout_guid = :layoutId
        WHERE guid = :itemId;
    )sql");

    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.bindValue(":layoutId", item.layoutId.toRfc4122());
    query.bindValue(":itemId", item.itemId.toRfc4122());
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool getLayoutIsAutoGenerated(const QSqlDatabase& database, const QnUuid& layoutId, bool& result)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    const QString queryStr = R"sql(
        SELECT value
        FROM vms_kvpair
        WHERE resource_guid = ? AND name = "autoGenerated"
    )sql";
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(layoutId.toRfc4122());

    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    if (query.next())
        result = query.value("value").toBool();

    return true;
}

bool markLayoutAsAutoGenerated(const QSqlDatabase& database, const QnUuid& layoutId)
{
    QSqlQuery query(database);

    const QString queryStr = R"sql(
        INSERT OR REPLACE INTO vms_kvpair
        (resource_guid, name, value)
        VALUES
        (?, "autoGenerated", "true")
    )sql";
    if (!QnDbHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    query.addBindValue(layoutId.toRfc4122());
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool reparentLayout(const QSqlDatabase& database, const LayoutOnVideoWall& item)
{
    ApiLayoutDataList layouts;
    if (!api::fetchLayouts(database, item.layoutId, layouts))
        return false;
    NX_ASSERT(layouts.size() == 1);

    for (auto& layout: layouts)
    {
        layout.parentId = item.videoWallId;
        if (!api::saveLayout(database, layout))
            return false;
    }

    return true;
}

bool cloneLayout(const QSqlDatabase& database, const LayoutOnVideoWall& item)
{
    ApiLayoutDataList layouts;
    if (!api::fetchLayouts(database, item.layoutId, layouts))
        return false;
    NX_ASSERT(layouts.size() == 1);

    for (auto& layout: layouts)
    {
        // Create new layout as copy of existing.
        layout.id = QnUuid::createUuid();
        layout.parentId = item.videoWallId;
        layout.name = "Copy of " + layout.name; //< TODO: #low #GDM #3.0 translate somehow

        if (!api::saveLayout(database, layout))
            return false;

        LayoutOnVideoWall updatedItem(item);
        updatedItem.layoutId = layout.id;
        if (!updateVideowallItem(database, updatedItem))
            return false;

        if (!markLayoutAsAutoGenerated(database, layout.id))
            return false;
    }

    return true;
}

bool reparentVideoWallLayouts(const QSqlDatabase& database)
{
    QList<LayoutOnVideoWall> videoWallItems;
    if (!getLayoutsOnVideoWalls(database, videoWallItems))
        return false;

    for (const auto& item: videoWallItems)
    {
        bool autoGenerated = false;
        if (!getLayoutIsAutoGenerated(database, item.layoutId, autoGenerated))
            return false;

        bool success = autoGenerated
            ? reparentLayout(database, item)
            : cloneLayout(database, item);

        if (!success)
            return false;
    }

    return true;
}

} // namespace migrations
} // namespace database
} // namespace ec2
