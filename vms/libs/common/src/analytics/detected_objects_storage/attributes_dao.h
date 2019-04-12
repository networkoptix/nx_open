#pragma once

#include <deque>

#include <nx/sql/query_context.h>

#include "analytics_events_storage_types.h"

namespace nx::analytics::storage {

/**
 * NOTE: Not thread-safe.
 */
class AttributesDao
{
public:
    /**
     * @return attributesId
     * NOTE: Throws on failure.
     */
    long long insertOrFetchAttributes(
        nx::sql::QueryContext* queryContext,
        const std::vector<common::metadata::Attribute>& eventAttributes);

private:
    struct AttributesCacheEntry
    {
        QByteArray md5;
        long long id = -1;
    };

    std::deque<AttributesCacheEntry> m_attributesCache;

    void addToAttributesCache(long long id, const QByteArray& content);
    long long findAttributesIdInCache(const QByteArray& content);
};

} // namespace nx::analytics::storage
