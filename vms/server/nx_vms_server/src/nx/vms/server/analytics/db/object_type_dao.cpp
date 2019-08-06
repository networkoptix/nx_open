#include "object_type_dao.h"

namespace nx::analytics::db {

int64_t ObjectTypeDao::objectTypeIdFromName(const QString& name) const
{
    QnMutexLocker locker(&m_mutex);

    auto it = m_objectTypeToId.find(name);
    return it != m_objectTypeToId.end() ? it->second : -1;
}

QString ObjectTypeDao::objectTypeFromId(int64_t id) const
{
    QnMutexLocker locker(&m_mutex);

    auto it = m_idToObjectType.find(id);
    return it != m_idToObjectType.end() ? it->second : QString();
}

int64_t ObjectTypeDao::insertOrFetch(
    nx::sql::QueryContext* queryContext,
    const QString& objectTypeName)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (auto it = m_objectTypeToId.find(objectTypeName); it != m_objectTypeToId.end())
            return it->second;
    }

    auto query = queryContext->connection()->createQuery();
    query->prepare("INSERT INTO object_type(name) VALUES (:name)");
    query->bindValue(":name", objectTypeName);
    query->exec();

    const auto id = query->impl().lastInsertId().toLongLong();
    addObjectTypeToDictionary(id, objectTypeName);

    return id;
}

void ObjectTypeDao::loadObjectTypeDictionary(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare("SELECT id, name FROM object_type");
    query->exec();
    while (query->next())
        addObjectTypeToDictionary(query->value(0).toLongLong(), query->value(1).toString());
}

void ObjectTypeDao::addObjectTypeToDictionary(
    int64_t id, const QString& name)
{
    QnMutexLocker locker(&m_mutex);

    m_objectTypeToId.emplace(name, id);
    m_idToObjectType.emplace(id, name);
}

} // namespace nx::analytics::db
