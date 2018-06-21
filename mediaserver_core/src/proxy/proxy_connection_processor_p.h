#pragma once

#include <chrono>

#include <nx/network/socket.h>

#include "network/tcp_connection_priv.h"
#include "network/universal_tcp_listener.h"

class QnProxyConnectionProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnProxyConnectionProcessorPrivate():
        QnTCPConnectionProcessorPrivate(),
        connectTimeout(5000),
        lastIoTimePoint(std::chrono::steady_clock::now())
    {
    }
    virtual ~QnProxyConnectionProcessorPrivate()
    {
    }

    std::unique_ptr<nx::network::AbstractStreamSocket> dstSocket;
    nx::utils::Url lastConnectedUrl;
    std::chrono::milliseconds connectTimeout;
	std::chrono::steady_clock::time_point lastIoTimePoint;
    nx::vms::network::ReverseConnectionManager* reverseConnectionManager = nullptr;
};
