#include "reparent_videowall_layouts.h"

#include <QtSql/QSqlQuery>

#include <database/api/db_layout_api.h>

#include <nx/vms/api/data/layout_data.h>

#include <nx/utils/uuid.h>
#include <nx/utils/log/log.h>

#include <utils/db/db_helper.h>
#include <nx/utils/literal.h>

namespace ec2 {
namespace database {
namespace migrations {

namespace {

static const QString kLogPrefix("ec2::database::migrations::");

void log(const QString& message)
{
    NX_DEBUG(nx::utils::log::Tag(kLogPrefix), message);
}

struct LayoutOnVideoWall
{
    QnUuid layoutId;
    QnUuid videoWallId;
};

// Get any videowall id to reparent unused layouts to it
bool getAnyVideoWall(const QSqlDatabase& database, QnUuid& id)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    QString queryStr(R"sql(
        SELECT
            r.guid as guid
        FROM vms_videowall l
        JOIN vms_resource r on r.id = l.resource_ptr_id
        ORDER BY r.guid
    )sql");

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    if (query.next())
        id = QnUuid::fromRfc4122(query.value("guid").toByteArray());
    else
        id = QnUuid();

    return true;
}

bool getLayoutsOnVideoWalls(const QSqlDatabase& database, QList<LayoutOnVideoWall>& data)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    const QString queryStr = R"sql(
        SELECT
            item.layout_guid as layoutId,
            item.videowall_guid as videoWallId
        FROM vms_videowall_item item
    )sql";

    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    while (query.next())
    {
        LayoutOnVideoWall item;
        item.layoutId = QnUuid::fromRfc4122(query.value("layoutId").toByteArray());
        if (item.layoutId.isNull())
            continue;

        item.videoWallId = QnUuid::fromRfc4122(query.value("videoWallId").toByteArray());
        NX_ASSERT(!item.videoWallId.isNull());
        if (item.videoWallId.isNull())
            continue;

        data.push_back(item);
    }

    return true;
}

bool getAutoGeneratedLayouts(const QSqlDatabase& database, QList<QnUuid>& ids)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);

    const QString queryStr = R"sql(
        SELECT value, resource_guid
        FROM vms_kvpair
        WHERE name = "autoGenerated"
    )sql";
    if (!nx::sql::SqlQueryExecutionHelper::prepareSQLQuery(&query, queryStr, Q_FUNC_INFO))
        return false;

    if (!nx::sql::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    while (query.next())
    {
        const bool autogenerated = query.value("value").toBool();
        if (autogenerated)
            ids.push_back(QnUuid::fromRfc4122(query.value("resource_guid").toByteArray()));
    }

    return true;
}

bool reparentLayout(ec2::database::api::QueryContext* context, const QnUuid& layoutId, const QnUuid& videoWallId)
{
    nx::vms::api::LayoutDataList layouts;
    if (!api::fetchLayouts(context->database(), layoutId, layouts))
        return false;
    NX_ASSERT(layouts.size() < 2); //zero layouts is acceptable here as we may get invalid kvpairs

    for (auto& layout: layouts)
    {
        log(lit("Layout %1 (%2) parent id changed from %3 to %4")
            .arg(layoutId.toSimpleString())
            .arg(layout.name)
            .arg(layout.parentId.toSimpleString())
            .arg(videoWallId.toSimpleString())
        );
        layout.parentId = videoWallId;
        if (!api::saveLayout(context, layout))
            return false;
    }

    return true;
}

} // namespace

bool reparentVideoWallLayouts(ec2::database::api::QueryContext* context)
{
    log(lit("Applying DB migration: reparent videowall layouts"));

    // Get all layouts on the videowall
    QList<LayoutOnVideoWall> videoWallItems;
    if (!getLayoutsOnVideoWalls(context->database(), videoWallItems))
        return false;

    log(lit("Found %1 layouts on videowalls").arg(videoWallItems.size()));

    // Get all layouts in the database
    QList<QnUuid> autogenerated;
    if (!getAutoGeneratedLayouts(context->database(), autogenerated))
        return false;

    log(lit("Found %1 auto-generated layouts").arg(autogenerated.size()));

    QSet<QnUuid> processed;
    for (const auto& item: videoWallItems)
    {
        log(lit("Processing layout %1").arg(item.layoutId.toSimpleString()));
        if (processed.contains(item.layoutId))
        {
            log(lit("Layout %1 is already processed").arg(item.layoutId.toSimpleString()));
            continue;
        }

        if (autogenerated.contains(item.layoutId))
            log(lit("Layout %1 is auto-generated").arg(item.layoutId.toSimpleString()));

        // Layouts must simply be reparented once
        if (!reparentLayout(context, item.layoutId, item.videoWallId))
            return false;
        processed.insert(item.layoutId);
    }

    QSet<QnUuid> unused = autogenerated.toSet().subtract(processed);
    log(lit("Found %1 unused layouts").arg(unused.size()));

    QnUuid anyVideoWallId;
    if (!getAnyVideoWall(context->database(), anyVideoWallId))
        return false;

    if (!anyVideoWallId.isNull())
    {
        for (const QnUuid& id : unused)
        {
            log(lit("Layout %1 is unused and will be reparented to videowall %2")
                .arg(id.toSimpleString())
                .arg(anyVideoWallId.toSimpleString())
            );
            if (!reparentLayout(context, id, anyVideoWallId))
                return false;
        }
    }

    log(lit("End of DB migration: reparent videowall layouts"));
    return true;
}

} // namespace migrations
} // namespace database
} // namespace ec2
