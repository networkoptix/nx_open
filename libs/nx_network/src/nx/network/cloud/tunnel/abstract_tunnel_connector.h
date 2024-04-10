// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/async_stoppable.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/expected.h>

#include "../data/connection_result_data.h"
#include "abstract_outgoing_tunnel_connection.h"
#include "tunnel_connect_statistics.h"
#include "tunnel.h"

namespace nx::network::cloud {

struct TunnelConnectResult
{
    nx::hpm::api::NatTraversalResultCode resultCode = nx::hpm::api::NatTraversalResultCode::ok;
    SystemError::ErrorCode sysErrorCode = SystemError::noError;

    std::unique_ptr<AbstractOutgoingTunnelConnection> connection;

    TunnelConnectStatistics stats;

    inline bool ok() const { return resultCode == nx::hpm::api::NatTraversalResultCode::ok; }
};

/**
 * Creates outgoing specialized AbstractTunnelConnection.
 * NOTE: Methods of this class are not thread-safe.
 */
class NX_NETWORK_API AbstractTunnelConnector:
    public aio::BasicPollable
{
public:
    using ConnectCompletionHandler = nx::utils::MoveOnlyFunc<void(TunnelConnectResult)>;

    /**
     * Helps to decide which method shall be used first.
     */
    virtual int getPriority() const = 0;
    /**
     * Establishes connection to the target host.
     * It is allowed to pipeline this method calls.
     * But these calls MUST be synchronized by caller.
     * @param timeout 0 means no timeout.
     * @param handler AbstractTunnelConnector can be safely freed within this handler.
     */
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) = 0;
    virtual const AddressEntry& targetPeerAddress() const = 0;
};

} // namespace nx::network::cloud
