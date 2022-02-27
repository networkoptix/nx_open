// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/utils/buffer.h>
#include <nx/utils/string.h>

namespace nx::hpm::api {

/**
 * Authorization credentials for MediatorServerTcpConnection.
 */
struct SystemCredentials
{
    std::string systemId;
    std::string serverId;
    std::string key;

    SystemCredentials() = default;

    SystemCredentials(
        std::string _systemId,
        std::string _serverId,
        std::string _key)
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

    std::string hostName() const { return nx::utils::buildString(serverId, '.', systemId); }
};


class AbstractCloudSystemCredentialsProvider
{
public:
    virtual ~AbstractCloudSystemCredentialsProvider() {}

    virtual std::optional<SystemCredentials> getSystemCredentials() const = 0;
};

} // namespace nx::hpm::api
