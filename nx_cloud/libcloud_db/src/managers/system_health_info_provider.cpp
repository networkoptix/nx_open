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

} // namespace cdb
} // namespace nx
