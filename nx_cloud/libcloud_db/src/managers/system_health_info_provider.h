#pragma once

#include <string>

namespace nx {
namespace cdb {

namespace ec2 {
class ConnectionManager;
} // namespace ec2

/**
 * Aggregates system health information from different sources.
 */
class SystemHealthInfoProvider
{
public:
    SystemHealthInfoProvider(const ec2::ConnectionManager& ec2ConnectionManager);

    bool isSystemOnline(const std::string& systemId) const;

private:
    const ec2::ConnectionManager& m_ec2ConnectionManager;
};

} // namespace cdb
} // namespace nx
