#pragma once

#include <nx/network/url/url_parse_helper.h>

#include "get_post_tunnel_processor.h"
#include "../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling {

// TODO Should replace GetPostTunnelProcessor 
// after switching traffic_relay to http::tunneling::Server.
template<typename ApplicationData>
class GetPostTunnelProcessorImpl:
    public GetPostTunnelProcessor<ApplicationData>
{
public:
    using NewTunnelHandler = nx::utils::MoveOnlyFunc<void(
        ApplicationData /*requestData*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/)>;

    GetPostTunnelProcessorImpl(NewTunnelHandler newTunnelHandler);

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

    void setTunnelAuthorizer(
        typename TunnelAuthorizer<ApplicationData>* tunnelAuthorizer);

protected:
    virtual void onTunnelCreated(
        ApplicationData requestData,
        std::unique_ptr<network::AbstractStreamSocket> connection) override;

private:
    NewTunnelHandler m_newTunnelHandler;
    typename TunnelAuthorizer<ApplicationData>* m_tunnelAuthorizer = nullptr;

    void processTunnelInitiationRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler);

    void onTunnelAutorizationCompleted(
        StatusCode::Value authorizationResult,
        ApplicationData applicationData,
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void openTunnel(
        ApplicationData applicationData,
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);
};

//-------------------------------------------------------------------------------------------------

template<typename ApplicationData>
GetPostTunnelProcessorImpl<ApplicationData>::GetPostTunnelProcessorImpl(
    NewTunnelHandler newTunnelHandler)
    :
    m_newTunnelHandler(std::move(newTunnelHandler))
{
}

template<typename ApplicationData>
void GetPostTunnelProcessorImpl<ApplicationData>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    using namespace std::placeholders;

    static constexpr char path[] = "get_post/{sequence}";

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        url::joinPath(basePath, path),
        std::bind(&GetPostTunnelProcessorImpl::processTunnelInitiationRequest, this,
            _1, _2, _3, _4, _5));
}

template<typename ApplicationData>
void GetPostTunnelProcessorImpl<ApplicationData>::setTunnelAuthorizer(
    typename TunnelAuthorizer<ApplicationData>* tunnelAuthorizer)
{
    m_tunnelAuthorizer = tunnelAuthorizer;
}

template<typename ApplicationData>
void GetPostTunnelProcessorImpl<ApplicationData>::onTunnelCreated(
    ApplicationData requestData,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    m_newTunnelHandler(std::move(requestData), std::move(connection));
}

template<typename ApplicationData>
void GetPostTunnelProcessorImpl<ApplicationData>::processTunnelInitiationRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace std::placeholders;

    auto requestContext = std::make_unique<RequestContext>(
        connection,
        std::move(authInfo),
        std::move(request),
        response);

    if (m_tunnelAuthorizer)
    {
        auto requestContextPtr = requestContext.get();

        m_tunnelAuthorizer->authorize(
            requestContextPtr,
            std::bind(&GetPostTunnelProcessorImpl::onTunnelAutorizationCompleted, this,
                _1, _2, std::move(requestContext), std::move(completionHandler)));
    }
    else
    {
        openTunnel(
            ApplicationData(),
            std::move(requestContext),
            std::move(completionHandler));
    }
}

template<typename ApplicationData>
void GetPostTunnelProcessorImpl<ApplicationData>::onTunnelAutorizationCompleted(
    StatusCode::Value authorizationResult,
    ApplicationData applicationData,
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!StatusCode::isSuccess(authorizationResult))
    {
        nx::utils::swapAndCall(completionHandler, authorizationResult);
        return;
    }

    openTunnel(
        std::move(applicationData),
        std::move(requestContext),
        std::move(completionHandler));
}

template<typename ApplicationData>
void GetPostTunnelProcessorImpl<ApplicationData>::openTunnel(
    ApplicationData applicationData,
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto requestResult = processOpenTunnelRequest(
        std::move(applicationData),
        requestContext->request,
        requestContext->response);

    nx::utils::swapAndCall(completionHandler, std::move(requestResult));
}

} // namespace nx::network::http::tunneling
