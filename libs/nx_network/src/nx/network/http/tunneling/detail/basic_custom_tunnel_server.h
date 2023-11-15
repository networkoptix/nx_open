// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

#include <nx/utils/counter.h>

#include "../../server/http_server_connection.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"
#include "../abstract_tunnel_authorizer.h"

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
        AbstractMessageDispatcher* messageDispatcher) = 0;
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
    ~BasicCustomTunnelServer();

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
    nx::utils::Counter m_asyncCallsTracker;

    void authorizeTunnel(
        const nx::network::http::HttpServerConnection& connection,
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void onTunnelAutorizationCompleted(
        StatusCode::Value authorizationResult,
        HttpHeaders responseHeaders,
        std::unique_ptr<RequestContext> requestContext,
        nx::network::http::RequestProcessedHandler completionHandler,
        ApplicationData... applicationData);

    void openTunnel(
        const RequestContext& requestContext,
        nx::network::http::RequestProcessedHandler completionHandler,
        HttpHeaders responseHeaders,
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
BasicCustomTunnelServer<ApplicationData...>::~BasicCustomTunnelServer()
{
    m_asyncCallsTracker.wait();
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
    RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    NX_VERBOSE(this, "Open tunnel request %1", requestContext.request.requestLine.url);

    const auto httpStatus = validateOpenTunnelRequest(requestContext);
    if (!StatusCode::isSuccessCode(httpStatus))
        return completionHandler(httpStatus);

    auto connection = requestContext.conn.lock();
    if (!connection) //< The connection was closed.
        return completionHandler(StatusCode::internalServerError);

    // NOTE: connection can be closed and all its posted calls queue cleaned.
    // But we need the following lambda to be executed in any case.
    connection->getAioThread()->dispatch(
        nullptr,
        [this, connection, requestContext = std::move(requestContext),
            completionHandler = std::move(completionHandler),
            locker = m_asyncCallsTracker.getScopedIncrement()]() mutable
        {
            if (m_tunnelAuthorizer)
            {
                authorizeTunnel(
                    *connection,
                    std::move(requestContext),
                    std::move(completionHandler));
            }
            else
            {
                openTunnel(
                    requestContext,
                    std::move(completionHandler),
                    {},
                    ApplicationData()...);
            }
        });
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
void BasicCustomTunnelServer<ApplicationData...>::authorizeTunnel(
    const nx::network::http::HttpServerConnection& connection,
    RequestContext requestContextOriginal,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    NX_VERBOSE(this, "Authorizing tunnel request %1", requestContextOriginal.request.requestLine.url);

    auto requestContext = std::make_unique<RequestContext>(
        std::move(requestContextOriginal));

    auto requestContextPtr = requestContext.get();

    m_tunnelAuthorizer->authorize(
        requestContextPtr,
        [this, aioThread = connection.getAioThread(), requestContext = std::move(requestContext),
            completionHandler = std::move(completionHandler)](
                StatusCode::Value result,
                HttpHeaders responseHeaders,
                ApplicationData... applicationData) mutable
        {
            NX_VERBOSE(this, "Tunnel %1 authorization completed with %2",
                requestContext->request.requestLine.url, StatusCode::toString(result));

            auto args = std::make_tuple(
                result,
                std::move(responseHeaders),
                std::move(requestContext),
                std::move(completionHandler),
                std::move(applicationData)...);

            // Moving execution to the connection thread.
            aioThread->dispatch(
                nullptr,
                [this, args = std::move(args), locker = m_asyncCallsTracker.getScopedIncrement()]() mutable
                {
                    std::apply([this](auto&&... args)
                    {
                        onTunnelAutorizationCompleted(std::forward<decltype(args)>(args)...);
                    },
                    std::move(args));
                });
        });
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::onTunnelAutorizationCompleted(
    StatusCode::Value authorizationResult,
    HttpHeaders responseHeaders,
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler,
    ApplicationData... applicationData)
{
    if (!StatusCode::isSuccessCode(authorizationResult))
    {
        RequestResult result(authorizationResult);
        result.headers = std::move(responseHeaders);
        return nx::utils::swapAndCall(completionHandler, std::move(result));
    }

    openTunnel(
        *requestContext,
        std::move(completionHandler),
        std::move(responseHeaders),
        std::move(applicationData)...);
}

template<typename ...ApplicationData>
void BasicCustomTunnelServer<ApplicationData...>::openTunnel(
    const RequestContext& requestContext,
    nx::network::http::RequestProcessedHandler completionHandler,
    HttpHeaders responseHeaders,
    ApplicationData... applicationData)
{
    auto requestResult = this->processOpenTunnelRequest(
        requestContext,
        std::move(applicationData)...);

    requestResult.headers.merge(std::move(responseHeaders));

    nx::utils::swapAndCall(completionHandler, std::move(requestResult));
}

} // namespace nx::network::http::tunneling::detail
