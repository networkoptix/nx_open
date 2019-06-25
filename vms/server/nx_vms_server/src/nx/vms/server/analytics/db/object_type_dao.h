#pragma once

#include <map>

#include <nx/sql/query_context.h>
#include <nx/utils/thread/mutex.h>

namespace nx::analytics::db {

class ObjectTypeDao
{
public:
    int64_t objectTypeIdFromName(const QString& name) const;

    QString objectTypeFromId(int64_t id) const;

    /**
     * @return Id of object type.
     */
    int64_t insertOrFetch(
        nx::sql::QueryContext* queryContext,
        const QString& objectTypeName);

    void loadObjectTypeDictionary(nx::sql::QueryContext* queryContext);

private:
    mutable QnMutex m_mutex;
    std::map<QString, int64_t> m_objectTypeToId;
    std::map<int64_t, QString> m_idToObjectType;

    void addObjectTypeToDictionary(int64_t id, const QString& name);
};

} // namespace nx::analytics::db
