#pragma once

#include <set>

#include <QtCore/QCache>

#include <nx/sql/query_context.h>

#include "attributes_dao.h"

namespace nx::analytics::db {

class ObjectTrackGroupDao
{
public:
    ObjectTrackGroupDao();

    /**
     * If the group trackIds is already known, then the existing id is returned.
     */
    int64_t insertOrFetchGroup(
        nx::sql::QueryContext* queryContext,
        const std::set<int64_t>& trackIds);

private:
    QCache<std::set<int64_t> /*trackIds*/, int64_t /*groupId*/> m_groupCache;

    int64_t saveToDb(
        nx::sql::QueryContext* queryContext,
        const std::set<int64_t>& attributesIds);
};

} // namespace nx::analytics::db
