#include "system_health_info_provider.h"

#include "../ec2/connection_manager.h"

namespace nx {
namespace cdb {

// TODO: this class will do much more in 3.1

SystemHealthInfoProvider::SystemHealthInfoProvider(
    const ec2::ConnectionManager& ec2ConnectionManager)
    :
    m_ec2ConnectionManager(ec2ConnectionManager)
{
}

bool SystemHealthInfoProvider::isSystemOnline(
    const std::string& systemId) const
{
    return m_ec2ConnectionManager.isSystemConnected(systemId);
}

void SystemHealthInfoProvider::getSystemHealthHistory(
    const AuthorizationInfo& authzInfo,
    data::SystemId systemId,
    std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler)
{
    completionHandler(api::ResultCode::notImplemented, api::SystemHealthHistory());
}

} // namespace cdb
} // namespace nx
