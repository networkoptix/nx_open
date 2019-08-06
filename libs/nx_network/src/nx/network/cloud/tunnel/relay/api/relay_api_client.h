#pragma once

#include <list>
#include <string>

#include <nx/network/aio/basic_pollable.h>
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
        BeginListeningHandler completionHandler) = 0;

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
        OpenRelayConnectionHandler handler) = 0;

    virtual nx::utils::Url url() const = 0;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const = 0;
};

//-------------------------------------------------------------------------------------------------

/**
 * This class is to hide different client types in the future.
 */
class NX_NETWORK_API Client:
    public AbstractClient
{
    using base_type = AbstractClient;

public:
    Client(const nx::utils::Url& baseUrl);

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
        OpenRelayConnectionHandler handler) override;

    virtual nx::utils::Url url() const override;

    virtual SystemError::ErrorCode prevRequestSysErrorCode() const override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unique_ptr<AbstractClient> m_actualClient;
};

} // namespace nx::cloud::relay::api
