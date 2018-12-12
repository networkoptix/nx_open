#pragma once

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
    using namespace std::placeholders;

    const auto httpStatus = validateOpenTunnelRequest(
        requestContextOriginal);
    if (!StatusCode::isSuccessCode(httpStatus))
    {
        completionHandler(httpStatus);
        return;
    }

    auto requestContext = std::make_unique<RequestContext>(
        std::move(requestContextOriginal));

    if (m_tunnelAuthorizer)
    {
        auto requestContextPtr = requestContext.get();

        m_tunnelAuthorizer->authorize(
            requestContextPtr,
            [this, requestContext = std::move(requestContext),
                completionHandler = std::move(completionHandler)](
                    StatusCode::Value result,
                    ApplicationData... applicationData) mutable
            {
                onTunnelAutorizationCompleted(
                    result,
                    std::move(requestContext),
                    std::move(completionHandler),
                    std::move(applicationData)...);
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
    {
        nx::utils::swapAndCall(completionHandler, authorizationResult);
        return;
    }

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
