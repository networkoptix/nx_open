#pragma once

#include <boost/optional.hpp>

#include "nx/network/buffer.h"

namespace nx {
namespace hpm {
namespace api {

/**
 * Authorization credentials for MediatorServerTcpConnection.
 */
struct SystemCredentials
{
    String systemId;
    String serverId;
    String key;

    SystemCredentials() {}
    SystemCredentials(
        nx::String _systemId,
        nx::String _serverId,
        nx::String _key)
    :
        systemId(std::move(_systemId)),
        serverId(std::move(_serverId)),
        key(std::move(_key))
    {
    }

    bool operator==(const SystemCredentials& rhs) const
    {
        return serverId == rhs.serverId && systemId == rhs.systemId && key == rhs.key;
    }

    String hostName() const { return serverId + String(".") + systemId; }
};


class AbstractCloudSystemCredentialsProvider
{
public:
    virtual ~AbstractCloudSystemCredentialsProvider() {}

    virtual boost::optional<SystemCredentials> getSystemCredentials() const = 0;
};

} // namespace api
} // namespace hpm
} // namespace nx
