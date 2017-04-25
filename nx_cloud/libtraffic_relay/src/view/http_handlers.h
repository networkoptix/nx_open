#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>

#include "../controller/connect_session_manager.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

class BeginListeningHandler:
    public nx_http::AbstractFusionRequestHandler<
        void, api::BeginListeningResponse>
{
    using self_type = BeginListeningHandler;

public:
    static const char* kPath;

    BeginListeningHandler(controller::AbstractConnectSessionManager* connectSessionManager):
        m_connectSessionManager(connectSessionManager)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        const nx_http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        using namespace std::placeholders;

        api::BeginListeningRequest inputData;
        inputData.peerName = requestPathParams()[0].toStdString();

        m_connectSessionManager->beginListening(
            std::move(inputData),
            std::bind(&self_type::onRequestProcessed, this, _1, _2, _3));
    }

private:
    controller::AbstractConnectSessionManager* m_connectSessionManager;

    void onRequestProcessed(
        api::ResultCode resultCode,
        api::BeginListeningResponse response,
        nx_http::ConnectionEvents connectionEvents)
    {
        setConnectionEvents(std::move(connectionEvents));

        this->requestCompleted(
            api::resultCodeToFusionRequestResult(resultCode),
            std::move(response));
    }
};

//-------------------------------------------------------------------------------------------------

class CreateClientSessionHandler:
    public nx_http::AbstractFusionRequestHandler<
        api::CreateClientSessionRequest, api::CreateClientSessionResponse>
{
    using self_type = CreateClientSessionHandler;

public:
    static const char* kPath;

    CreateClientSessionHandler(controller::AbstractConnectSessionManager* connectSessionManager):
        m_connectSessionManager(connectSessionManager)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        const nx_http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        api::CreateClientSessionRequest inputData) override
    {
        using namespace std::placeholders;

        inputData.targetPeerName = requestPathParams()[0].toStdString();

        m_connectSessionManager->createClientSession(
            std::move(inputData),
            std::bind(&self_type::onRequestProcessed, this, _1, _2));
    }

private:
    controller::AbstractConnectSessionManager* m_connectSessionManager;

    void onRequestProcessed(
        api::ResultCode resultCode,
        api::CreateClientSessionResponse response)
    {
        this->requestCompleted(
            api::resultCodeToFusionRequestResult(resultCode),
            std::move(response));
    }
};

//-------------------------------------------------------------------------------------------------

class ConnectToPeerHandler:
    public nx_http::AbstractFusionRequestHandler<void, void>
{
    using self_type = ConnectToPeerHandler;

public:
    static const char* kPath;

    ConnectToPeerHandler(controller::AbstractConnectSessionManager* connectSessionManager):
        m_connectSessionManager(connectSessionManager)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        const nx_http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        using namespace std::placeholders;

        api::ConnectToPeerRequest inputData;
        inputData.sessionId = requestPathParams()[0].toStdString();

        m_connectSessionManager->connectToPeer(
            std::move(inputData),
            std::bind(&self_type::onRequestProcessed, this, _1, _2));
    }

private:
    controller::AbstractConnectSessionManager* m_connectSessionManager;

    void onRequestProcessed(
        api::ResultCode resultCode,
        nx_http::ConnectionEvents connectionEvents)
    {
        setConnectionEvents(std::move(connectionEvents));
        this->requestCompleted(api::resultCodeToFusionRequestResult(resultCode));
    }
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
