// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include "../abstract_tunnel_authorizer.h"
#include "../../server/http_server_connection.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling::detail {

template<typename... ApplicationData>
class AbstractTunnelServer
{
public:
    virtual ~AbstractTunnelServer() = default;

    virtual void setTunnelAuthorizer(
        TunnelAuthorizer<ApplicationData...>* tunnelAuthorizer) = 0;

    virtual void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher) = 0;
};

//-------------------------------------------------------------------------------------------------

template<typename... ApplicationData>
class BasicCustomTunnelServer:
    public AbstractTunnelServer<ApplicationData...>
{
public:
    using NewTunnelHandler = nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/,
        ApplicationData... /*requestData*/)>;

    BasicCustomTunnelServer(NewTunnelHandler newTunnelHandler);
    virtual ~BasicCustomTunnelServer() = default;

    virtual void setTunnelAuthorizer(
        TunnelAuthorizer<ApplicationData...>* tunnelAuthorizer) override;

    void setRequestPath(const std::string& path);
    std::string requestPath() const;

protected:
    virtual StatusCode::Value validateOpenTunnelRequest(
        const RequestContext& requestContext);

    /**
     * Note: it is guaranteed that this method is invoked within the
     * requestContext.connection's AIO thread.
     */
    virtual network::http::RequestResult processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData) = 0;

    /**
     * This should be registered as the handler of the initial "open tunnel" request.
     */
    void processTunnelInitiationRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void reportTunnel(
        std::unique_ptr<AbstractStreamSocket> connection,
        ApplicationData... applicationData);

private:
    NewTunnelHandler m_newTunnelHandler;
    TunnelAuthorizer<ApplicationData...>* m_tunnelAuthorizer = nullptr;
    std::string m_path;

    void onTunnelAutorizationCompleted(
        StatusCode::Value authorizationResult,
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::RequestProcessedHandler completionHandler,
        ApplicationData... applicationData);

    void openTunnel(
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::RequestProcessedHandler completionHandler,
        ApplicationData... applicationData);
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
BasicCustomTunnelServer<ApplicationData...>::BasicCustomTunnelServer(
    NewTunnelHandler newTunnelHandler)
    :
    m_newTunnelHandler(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::setTunnelAuthorizer(
    TunnelAuthorizer<ApplicationData...>* tunnelAuthorizer)
{
    m_tunnelAuthorizer = tunnelAuthorizer;
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::setRequestPath(
    const std::string& path)
{
    m_path = path;
}

template<typename ...ApplicationData>
std::string BasicCustomTunnelServer<ApplicationData...>::requestPath() const
{
    return m_path;
}

template<typename ...ApplicationData>
StatusCode::Value BasicCustomTunnelServer<ApplicationData...>::validateOpenTunnelRequest(
    const RequestContext& /*requestContext*/)
{
    return StatusCode::ok;
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::processTunnelInitiationRequest(
    RequestContext requestContextOriginal,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    NX_VERBOSE(this, "Open tunnel request %1", requestContextOriginal.request.requestLine.url);

    const auto httpStatus = validateOpenTunnelRequest(requestContextOriginal);
    if (!StatusCode::isSuccessCode(httpStatus))
        return completionHandler(httpStatus);

    auto requestContext = std::make_unique<RequestContext>(
        std::move(requestContextOriginal));

    if (m_tunnelAuthorizer)
    {
        auto requestContextPtr = requestContext.get();

        auto strongRef = requestContext->connection->shared_from_this();
        std::weak_ptr<HttpServerConnection> weakConnectionRef = strongRef;

        NX_VERBOSE(this, "Authorizing tunnel request %1",
            requestContextOriginal.request.requestLine.url);

        m_tunnelAuthorizer->authorize(
            requestContextPtr,
            [this, weakConnectionRef, requestContext = std::move(requestContext),
                completionHandler = std::move(completionHandler)](
                    StatusCode::Value result,
                    ApplicationData... applicationData) mutable
            {
                NX_VERBOSE(this, "Tunnel %1 authorization completed with %2",
                    requestContext->request.requestLine.url, StatusCode::toString(result));

                auto strongConnectionRef = weakConnectionRef.lock();
                if (!strongConnectionRef)
                    return nx::utils::swapAndCall(completionHandler, result);

                auto args = std::make_tuple(
                    result,
                    std::move(requestContext),
                    std::move(completionHandler),
                    std::move(applicationData)...);

                // NOTE: Connection should always be removed from its own thread.
                // So, if our strongConnectionRef actually owns connection,
                // it should not delete it here.
                auto* connectionPtr = strongConnectionRef.get();
                connectionPtr->getAioThread()->dispatch(
                    nullptr,
                    [this, args = std::move(args), strongConnectionRef = std::move(strongConnectionRef)]() mutable
                    {
                        std::apply(
                            [this](auto&&... args)
                            {
                                onTunnelAutorizationCompleted(std::forward<decltype(args)>(args)...);
                            },
                            std::move(args));
                    });
            });
    }
    else
    {
        openTunnel(
            std::move(requestContext),
            std::move(completionHandler),
            ApplicationData()...);
    }
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::reportTunnel(
    std::unique_ptr<AbstractStreamSocket> connection,
    ApplicationData... applicationData)
{
    m_newTunnelHandler(
        std::move(connection),
        std::move(applicationData)...);
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::onTunnelAutorizationCompleted(
    StatusCode::Value authorizationResult,
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler,
    ApplicationData... applicationData)
{
    if (!StatusCode::isSuccessCode(authorizationResult))
        return nx::utils::swapAndCall(completionHandler, authorizationResult);

    openTunnel(
        std::move(requestContext),
        std::move(completionHandler),
        std::move(applicationData)...);
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::openTunnel(
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler,
    ApplicationData... applicationData)
{
    auto requestResult = this->processOpenTunnelRequest(
        *requestContext,
        std::move(applicationData)...);

    nx::utils::swapAndCall(completionHandler, std::move(requestResult));
}

} // namespace nx::network::http::tunneling::detail
