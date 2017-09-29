#pragma once

#include "basic_handler.h"
#include "../controller/connect_session_manager.h"
#include "../controller/listening_peer_manager.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class BeginListeningHandler:
    public BasicHandlerWithoutRequestBody<
        controller::AbstractListeningPeerManager,
        api::BeginListeningRequest,
        controller::AbstractListeningPeerManager::BeginListeningHandler,
        api::BeginListeningResponse>
{
    using self_type = BeginListeningHandler;
    using base_type = 
        BasicHandlerWithoutRequestBody<
            controller::AbstractListeningPeerManager,
            api::BeginListeningRequest,
            controller::AbstractListeningPeerManager::BeginListeningHandler,
            api::BeginListeningResponse>;

public:
    static const char* kPath;

    BeginListeningHandler(
        controller::AbstractListeningPeerManager* listeningPeerManager);

protected:
    virtual api::BeginListeningRequest prepareRequestData(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& httpRequest) override;
};

//-------------------------------------------------------------------------------------------------

class CreateClientSessionHandler:
    public BasicHandlerWithRequestBody<
        controller::AbstractConnectSessionManager,
        api::CreateClientSessionRequest,
        controller::AbstractConnectSessionManager::CreateClientSessionHandler,
        api::CreateClientSessionResponse>
{
    using self_type = CreateClientSessionHandler;
    using base_type = 
        BasicHandlerWithRequestBody<
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
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& httpRequest,
        api::CreateClientSessionRequest* request) override;
};

//-------------------------------------------------------------------------------------------------

class ConnectToPeerHandler:
    public BasicHandlerWithoutRequestBody<
        controller::AbstractConnectSessionManager,
        controller::ConnectToPeerRequestEx,
        controller::AbstractConnectSessionManager::ConnectToPeerHandler>
{
    using self_type = ConnectToPeerHandler;
    using base_type =
        BasicHandlerWithoutRequestBody<
            controller::AbstractConnectSessionManager,
            controller::ConnectToPeerRequestEx,
            controller::AbstractConnectSessionManager::ConnectToPeerHandler>;

public:
    static const char* kPath;

    ConnectToPeerHandler(
        controller::AbstractConnectSessionManager* connectSessionManager);

protected:
    virtual controller::ConnectToPeerRequestEx prepareRequestData(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& httpRequest) override;
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
