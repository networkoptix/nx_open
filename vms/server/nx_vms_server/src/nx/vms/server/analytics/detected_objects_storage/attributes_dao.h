#pragma once

#include <QtCore/QCache>

#include <deque>

#include <nx/sql/query_context.h>

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>

namespace nx::analytics::storage {

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
    long long insertOrFetchAttributes(
        nx::sql::QueryContext* queryContext,
        const std::vector<common::metadata::Attribute>& eventAttributes);

    void clear();

    static std::vector<common::metadata::Attribute> deserialize(
        const QString& attributesStr);

private:
    QCache<QByteArray /*md5*/, long long /*id*/> m_attributesCache;

    void addToAttributesCache(long long id, const QByteArray& content);
    long long findAttributesIdInCache(const QByteArray& content);
};

} // namespace nx::analytics::storage
