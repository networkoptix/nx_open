#pragma once

#include <nx/network/http/server/abstract_fusion_request_handler.h>

#include "../controller/connect_session_manager.h"

namespace nx {
namespace cloud {
namespace relay {

class BeginListeningHandler:
    public nx_http::AbstractFusionRequestHandler<
        void, api::BeginListeningResponse>
{
    using self_type = BeginListeningHandler;

public:
    BeginListeningHandler(controller::ConnectSessionManager* connectSessionManager):
        m_connectSessionManager(connectSessionManager)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        const nx_http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        using namespace std::placeholders;

        NX_CRITICAL(requestPathParams().size() == 1); //< Failure here means bug somewhere down the stack.
        api::BeginListeningRequest inputData;
        inputData.peerName = requestPathParams()[0];

        m_connectSessionManager->beginListening(
            std::move(inputData),
            std::bind(&self_type::onRequestProcessed, this, _1, _2, _3));
    }

private:
    controller::ConnectSessionManager* m_connectSessionManager;

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

} // namespace relay
} // namespace cloud
} // namespace nx
