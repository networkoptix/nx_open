
#pragma once

#include <chrono>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "abstract_outgoing_tunnel_connection.h"
#include "nx/network/aio/basic_pollable.h"


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API AbstractCrossNatConnector
    :
    public aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> ConnectCompletionHandler;

    virtual ~AbstractCrossNatConnector() {}

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) = 0;
};

}   //namespace cloud
}   //namespace network
}   //namespace nx
