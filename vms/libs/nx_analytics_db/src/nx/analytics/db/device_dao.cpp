#include "device_dao.h"

#include <nx/fusion/serialization/sql_functions.h>

namespace nx::analytics::db {

int DeviceDao::deviceIdFromGuid(const QnUuid& deviceGuid) const
{
    QnMutexLocker locker(&m_mutex);

    auto it = m_deviceGuidToId.find(deviceGuid);
    return it != m_deviceGuidToId.end() ? it->second : -1;
}

QnUuid DeviceDao::deviceGuidFromId(int id) const
{
    QnMutexLocker locker(&m_mutex);

    auto it = m_idToDeviceGuid.find(id);
    return it != m_idToDeviceGuid.end() ? it->second : QnUuid();
}

int DeviceDao::insertOrFetch(
    nx::sql::QueryContext* queryContext,
    const QnUuid& deviceGuid)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (auto it = m_deviceGuidToId.find(deviceGuid); it != m_deviceGuidToId.end())
            return it->second;
    }

    auto query = queryContext->connection()->createQuery();
    query->prepare("INSERT INTO device(guid) VALUES (:guid)");
    query->bindValue(":guid", QnSql::serialized_field(deviceGuid));
    query->exec();

    const auto deviceId = query->impl().lastInsertId().toLongLong();
    addDeviceToDictionary(deviceId, deviceGuid);

    return deviceId;
}

void DeviceDao::loadDeviceDictionary(nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare("SELECT id, guid FROM device");
    query->exec();
    while (query->next())
    {
        addDeviceToDictionary(
            query->value(0).toLongLong(),
            QnSql::deserialized_field<QnUuid>(query->value(1)));
    }
}

void DeviceDao::addDeviceToDictionary(
    int id, const QnUuid& deviceGuid)
{
    QnMutexLocker locker(&m_mutex);

    m_deviceGuidToId.emplace(deviceGuid, id);
    m_idToDeviceGuid.emplace(id, deviceGuid);
}

} // namespace nx::analytics::db
