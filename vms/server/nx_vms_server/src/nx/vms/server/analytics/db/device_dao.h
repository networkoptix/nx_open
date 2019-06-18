#pragma once

#include <map>

#include <nx/sql/query_context.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace nx::analytics::db {

class DeviceDao
{
public:
    int deviceIdFromGuid(const QnUuid& deviceGuid) const;

    QnUuid deviceGuidFromId(int id) const;

    /**
     * @return Device id in the DB.
     */
    int insertOrFetch(
        nx::sql::QueryContext* queryContext,
        const QnUuid& deviceId);

    void loadDeviceDictionary(nx::sql::QueryContext* queryContext);

private:
    void addDeviceToDictionary(int id, const QnUuid& deviceGuid);

    mutable QnMutex m_mutex;
    std::map<QnUuid, int> m_deviceGuidToId;
    std::map<int, QnUuid> m_idToDeviceGuid;
};

} // namespace nx::analytics::db
