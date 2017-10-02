#include "system_health_info_provider_stub.h"

namespace nx {
namespace cdb {
namespace test {

bool SystemHealthInfoProviderStub::isSystemOnline(const std::string& /*systemId*/) const
{
    return false;
}

void SystemHealthInfoProviderStub::getSystemHealthHistory(
    const AuthorizationInfo& /*authzInfo*/,
    data::SystemId /*systemId*/,
    std::function<void(api::ResultCode, api::SystemHealthHistory)> /*completionHandler*/)
{
}

} // namespace test
} // namespace cdb
} // namespace nx
