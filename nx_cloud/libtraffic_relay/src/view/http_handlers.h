#pragma once

#include "basic_handler.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class BeginListeningHandler:
    public BasicHandlerWithoutRequestBody<
        api::BeginListeningRequest,
        controller::AbstractConnectSessionManager::BeginListeningHandler,
        api::BeginListeningResponse>
{
    using self_type = BeginListeningHandler;
    using base_type = 
        BasicHandlerWithoutRequestBody<
            api::BeginListeningRequest,
            controller::AbstractConnectSessionManager::BeginListeningHandler,
            api::BeginListeningResponse>;

public:
    static const char* kPath;

    BeginListeningHandler(
        controller::AbstractConnectSessionManager* connectSessionManager);

protected:
    virtual api::BeginListeningRequest prepareRequestData() override;
};

//-------------------------------------------------------------------------------------------------

class CreateClientSessionHandler:
    public BasicHandlerWithRequestBody<
        api::CreateClientSessionRequest,
        controller::AbstractConnectSessionManager::CreateClientSessionHandler,
        api::CreateClientSessionResponse>
{
    using self_type = CreateClientSessionHandler;
    using base_type = 
        BasicHandlerWithRequestBody<
            api::CreateClientSessionRequest,
            controller::AbstractConnectSessionManager::CreateClientSessionHandler,
            api::CreateClientSessionResponse>;

public:
    static const char* kPath;

    CreateClientSessionHandler(
        controller::AbstractConnectSessionManager* connectSessionManager);

protected:
    virtual void prepareRequestData(api::CreateClientSessionRequest* request) override;
};

//-------------------------------------------------------------------------------------------------

class ConnectToPeerHandler:
    public BasicHandlerWithoutRequestBody<
        api::ConnectToPeerRequest,
        controller::AbstractConnectSessionManager::ConnectToPeerHandler>
{
    using self_type = ConnectToPeerHandler;
    using base_type =
        BasicHandlerWithoutRequestBody<
            api::ConnectToPeerRequest,
            controller::AbstractConnectSessionManager::ConnectToPeerHandler>;

public:
    static const char* kPath;

    ConnectToPeerHandler(
        controller::AbstractConnectSessionManager* connectSessionManager);

protected:
    virtual api::ConnectToPeerRequest prepareRequestData() override;
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
