#pragma once

#include <map>

#include <nx/sql/query_context.h>
#include <nx/utils/thread/mutex.h>

namespace nx::analytics::storage {

class ObjectTypeDao
{
public:
    void addObjectTypeToDictionary(long long id, const QString& name);

    long long objectTypeIdFromName(const QString& name) const;

    QString objectTypeFromId(long long id) const;

    long long insertOrFetch(
        nx::sql::QueryContext* queryContext,
        const QString& objectTypeName);

    void loadObjectTypeDictionary(nx::sql::QueryContext* queryContext);

private:
    mutable QnMutex m_mutex;
    std::map<QString, long long> m_objectTypeToId;
    std::map<long long, QString> m_idToObjectType;
};

} // namespace nx::analytics::storage
