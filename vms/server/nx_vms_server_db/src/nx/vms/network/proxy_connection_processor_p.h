#pragma once

#include <chrono>

#include <nx/network/socket.h>

#include "network/tcp_connection_priv.h"

namespace nx {
namespace vms {
namespace network {


class ProxyConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    ProxyConnectionProcessorPrivate():
        QnTCPConnectionProcessorPrivate(),
        connectTimeout(5000),
        lastIoTimePoint(std::chrono::steady_clock::now())
    {
    }
    virtual ~ProxyConnectionProcessorPrivate()
    {
    }

    std::unique_ptr<::nx::network::AbstractStreamSocket> dstSocket;
    ::nx::utils::Url lastConnectedEndpoint;
    std::chrono::milliseconds connectTimeout;
	std::chrono::steady_clock::time_point lastIoTimePoint;
    ::nx::vms::network::ReverseConnectionManager* reverseConnectionManager = nullptr;
};

} // namespace network
} // namespace vms
} // namespace nx
