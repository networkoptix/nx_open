#pragma once

#include <chrono>

#include <network/router.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/abstract_socket.h>
#include <common/common_module_aware.h>

namespace nx::vms::network {

class AbstractServerConnector:
    public /*mixin*/ QnCommonModuleAware
{
public:
    using QnCommonModuleAware::QnCommonModuleAware;
    virtual ~AbstractServerConnector() = default;

    using Connection = std::unique_ptr<nx::network::AbstractStreamSocket>;

    virtual cf::future<Connection> connect(
        const QnRoute& route,
        std::chrono::milliseconds timeout,
        bool sslRequired = true) = 0;

    cf::future<Connection> connect(
        const QnUuid& serverId,
        std::chrono::milliseconds timeout,
        bool sslRequired = true);
};

} // namespace nx::vms::network
