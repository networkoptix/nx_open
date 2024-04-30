// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>
#include <string>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/tunneling/client.h>
#include <nx/utils/move_only_func.h>

#include "relay_api_data_types.h"
#include "relay_api_result_code.h"

namespace nx::cloud::relay::api {

//-------------------------------------------------------------------------------------------------
// Client

using BeginListeningHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode,
        BeginListeningResponse,
        std::unique_ptr<network::AbstractStreamSocket>)>;

using StartClientConnectSessionHandler =
    nx::utils::MoveOnlyFunc<void(ResultCode, CreateClientSessionResponse)>;

using OpenRelayConnectionHandler =
    nx::utils::MoveOnlyFunc<void(
        ResultCode,
        std::unique_ptr<network::AbstractStreamSocket>)>;

using ClientFeedbackFunction = nx::utils::MoveOnlyFunc<void(ResultCode)>;

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API AbstractClient:
    public network::aio::BasicPollable
{
public:
    virtual void beginListening(
        const std::string& peerName,
        BeginListeningHandler handler) = 0;

    /**
     * @param desiredSessionId Can be empty.
     *   In this case server will generate unique session id itself.
     */
    virtual void startSession(
        const std::string& desiredSessionId,
        const std::string& targetPeerName,
        StartClientConnectSessionHandler handler) = 0;

    /**
     * After successful call socket provided represents connection to the requested peer.
     */
    virtual void openConnectionToTheTargetHost(
        const std::string& sessionId,
        OpenRelayConnectionHandler handler,
        const nx::network::http::tunneling::ConnectOptions& options = {}) = 0;

    virtual nx::utils::Url url() const = 0;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const = 0;
    virtual nx::network::http::StatusCode::Value prevRequestHttpStatusCode() const = 0;

    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * Api client to the traffic relay service.
 * Note: data connections to the relay are created using nx::network::http::tunneling::Client.
 */
class NX_NETWORK_API Client:
    public AbstractClient
{
    using base_type = AbstractClient;

public:
    /**
     * @param forcedHttpTunnelType passed to the nx::network::http::tunneling::Client.
     */
    Client(
        const nx::utils::Url& baseUrl,
        std::optional<int> forcedHttpTunnelType = std::nullopt);

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    virtual void beginListening(
        const std::string& peerName,
        BeginListeningHandler completionHandler) override;

    virtual void startSession(
        const std::string& desiredSessionId,
        const std::string& targetPeerName,
        StartClientConnectSessionHandler handler) override;

    virtual void openConnectionToTheTargetHost(
        const std::string& sessionId,
        OpenRelayConnectionHandler handler,
        const nx::network::http::tunneling::ConnectOptions& options = {}) override;

    virtual nx::utils::Url url() const override;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;
    virtual nx::network::http::StatusCode::Value prevRequestHttpStatusCode() const override;

    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractClient> m_actualClient;
};

} // namespace nx::cloud::relay::api
