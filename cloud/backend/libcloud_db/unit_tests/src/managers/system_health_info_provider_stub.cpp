#include "system_health_info_provider_stub.h"

namespace nx::cloud::db {
namespace test {

bool SystemHealthInfoProviderStub::isSystemOnline(const std::string& systemId) const
{
    return m_onlineSystems.find(systemId) != m_onlineSystems.end();
}

void SystemHealthInfoProviderStub::getSystemHealthHistory(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId /*systemId*/,
    std::function<void(api::Result, api::SystemHealthHistory)> /*completionHandler*/)
{
}

void SystemHealthInfoProviderStub::setSystemStatus(const std::string& id, bool online)
{
    if (online)
        m_onlineSystems.insert(id);
    else
        m_onlineSystems.erase(id);
}

} // namespace test
} // namespace nx::cloud::db
