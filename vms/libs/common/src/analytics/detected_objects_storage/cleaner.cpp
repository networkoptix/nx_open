#include "cleaner.h"

#include "device_dao.h"

namespace nx::analytics::storage {

Cleaner::Cleaner(
    const DeviceDao& deviceDao,
    const QnUuid& /*deviceId*/,
    std::chrono::milliseconds /*oldestDataToKeepTimestamp*/)
    :
    m_deviceDao(deviceDao)
{
    // TODO
}

void Cleaner::clean(nx::sql::QueryContext* /*queryContext*/)
{
    // TODO
}

} // namespace nx::analytics::storage
