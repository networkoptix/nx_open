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

class NX_NETWORK_API Client:
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

} // namespace nx::cloud::relay::api
