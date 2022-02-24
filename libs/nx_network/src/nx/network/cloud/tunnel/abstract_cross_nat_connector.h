// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>

#include "abstract_outgoing_tunnel_connection.h"

namespace nx::network::cloud {

class NX_NETWORK_API AbstractCrossNatConnector:
    public aio::BasicPollable
{
public:
    using ConnectCompletionHandler = nx::utils::MoveOnlyFunc<void(
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)>;

    virtual ~AbstractCrossNatConnector() = default;

    virtual void connect(
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) = 0;

    virtual std::string getRemotePeerName() const = 0;
};

} // namespace nx::network::cloud
