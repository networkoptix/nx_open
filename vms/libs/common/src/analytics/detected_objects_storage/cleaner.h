#pragma once

#include <chrono>

#include <nx/sql/query_context.h>
#include <nx/utils/uuid.h>

namespace nx::analytics::storage {

class DeviceDao;

class Cleaner
{
public:
    Cleaner(
        const DeviceDao& deviceDao,
        const QnUuid& deviceId,
        std::chrono::milliseconds oldestDataToKeepTimestamp);

    void clean(nx::sql::QueryContext* queryContext);

private:
    const DeviceDao& m_deviceDao;
};

} // namespace nx::analytics::storage
