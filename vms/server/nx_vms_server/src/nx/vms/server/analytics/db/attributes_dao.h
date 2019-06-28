#pragma once

#include <QtCore/QCache>

#include <nx/sql/query_context.h>

#include <analytics/db/analytics_db_types.h>

namespace std { uint qHash(const std::set<int64_t>& value); }

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

    int64_t combineAttributes(
        nx::sql::QueryContext* queryContext,
        const std::set<int64_t>& attributesIds);

    std::set<int64_t> lookupCombinedAttributes(
        nx::sql::QueryContext* queryContext,
        const QString& text);

    void clear();

    static std::vector<common::metadata::Attribute> deserialize(
        const QString& attributesStr);

private:
    QCache<QByteArray /*md5*/, int64_t /*id*/> m_attributesCache;
    QCache<std::set<int64_t> /*attributesIds*/, int64_t /*id*/> m_combinedAttrsCache;

    /**
     * @return Attributes set id.
     */
    int64_t insertAttributes(
        nx::sql::QueryContext* queryContext,
        const std::vector<common::metadata::Attribute>& attributes,
        const QByteArray& serializedAttributes);

    void addToAttributesCache(int64_t id, const QByteArray& content);
    int64_t findAttributesIdInCache(const QByteArray& content);
    void removeFromAttributesCache(const QByteArray& content);

    int64_t saveToDb(
        nx::sql::QueryContext* queryContext,
        const std::set<int64_t>& attributesIds);
};

} // namespace nx::analytics::db
