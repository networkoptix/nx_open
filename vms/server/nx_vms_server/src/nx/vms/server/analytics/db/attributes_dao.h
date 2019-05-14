#pragma once

#include <QtCore/QCache>

#include <deque>

#include <nx/sql/query_context.h>

#include <analytics/db/analytics_db_types.h>

namespace nx::analytics::db {

/**
 * NOTE: Not thread-safe.
 */
class AttributesDao
{
public:
    AttributesDao();

    /**
     * @return attributesId
     * NOTE: Throws on failure.
     */
    int64_t insertOrFetchAttributes(
        nx::sql::QueryContext* queryContext,
        const std::vector<common::metadata::Attribute>& eventAttributes);

    void clear();

    static std::vector<common::metadata::Attribute> deserialize(
        const QString& attributesStr);

private:
    QCache<QByteArray /*md5*/, int64_t /*id*/> m_attributesCache;

    void addToAttributesCache(int64_t id, const QByteArray& content);
    int64_t findAttributesIdInCache(const QByteArray& content);
};

} // namespace nx::analytics::db
