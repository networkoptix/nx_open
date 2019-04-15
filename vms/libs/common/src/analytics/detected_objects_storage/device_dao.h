#pragma once

#include <map>

#include <nx/sql/query_context.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx::analytics::storage {

class DeviceDao
{
public:
    void addDeviceToDictionary(long long id, const QnUuid& deviceGuid);

    long long deviceIdFromGuid(const QnUuid& deviceGuid) const;

    QnUuid deviceGuidFromId(long long id) const;

    long long insertOrFetch(
        nx::sql::QueryContext* queryContext,
        const QnUuid& deviceId);

    void loadDeviceDictionary(nx::sql::QueryContext* queryContext);

private:
    mutable QnMutex m_mutex;
    std::map<QnUuid, long long> m_deviceGuidToId;
    std::map<long long, QnUuid> m_idToDeviceGuid;
};

} // namespace nx::analytics::storage
