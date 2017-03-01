#pragma once

#include <string>

#include <cdb/result_code.h>

#include "access_control/auth_types.h"
#include "data/system_data.h"

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

    void getSystemHealthHistory(
        const AuthorizationInfo& authzInfo,
        data::SystemId systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler);

private:
    const ec2::ConnectionManager& m_ec2ConnectionManager;
};

} // namespace cdb
} // namespace nx
