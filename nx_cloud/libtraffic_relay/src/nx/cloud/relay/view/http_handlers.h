#pragma once

#include <nx/cloud/relaying/listening_peer_manager.h>
#include <nx/cloud/relaying/http_view/basic_http_handler.h>

#include "../controller/connect_session_manager.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class CreateClientSessionHandler:
    public relaying::BasicHandlerWithRequestBody<
        controller::AbstractConnectSessionManager,
        api::CreateClientSessionRequest,
        controller::AbstractConnectSessionManager::CreateClientSessionHandler,
        api::CreateClientSessionResponse>
{
    using self_type = CreateClientSessionHandler;
    using base_type = 
        relaying::BasicHandlerWithRequestBody<
            controller::AbstractConnectSessionManager,
            api::CreateClientSessionRequest,
            controller::AbstractConnectSessionManager::CreateClientSessionHandler,
            api::CreateClientSessionResponse>;

public:
    static const char* kPath;

    CreateClientSessionHandler(
        controller::AbstractConnectSessionManager* connectSessionManager);

protected:
    virtual void prepareRequestData(
        nx::network::http::HttpServerConnection* const connection,
        const nx::network::http::Request& httpRequest,
        api::CreateClientSessionRequest* request) override;
};

//-------------------------------------------------------------------------------------------------

class ConnectToPeerHandler:
    public relaying::BasicHandlerWithoutRequestBody<
        controller::AbstractConnectSessionManager,
        controller::ConnectToPeerRequestEx,
        controller::AbstractConnectSessionManager::ConnectToPeerHandler>
{
    using self_type = ConnectToPeerHandler;
    using base_type =
        relaying::BasicHandlerWithoutRequestBody<
            controller::AbstractConnectSessionManager,
            controller::ConnectToPeerRequestEx,
            controller::AbstractConnectSessionManager::ConnectToPeerHandler>;

public:
    static const char* kPath;

    ConnectToPeerHandler(
        controller::AbstractConnectSessionManager* connectSessionManager);

protected:
    virtual controller::ConnectToPeerRequestEx prepareRequestData(
        nx::network::http::HttpServerConnection* const connection,
        const nx::network::http::Request& httpRequest) override;
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
