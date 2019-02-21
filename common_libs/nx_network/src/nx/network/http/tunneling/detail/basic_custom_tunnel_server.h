#pragma once

#include "../abstract_tunnel_authorizer.h"
#include "../../server/http_server_connection.h"

namespace nx_http::tunneling::detail {

template<typename ...ApplicationData>
class BasicCustomTunnelServer
{
public:
    using NewTunnelHandler = nx::utils::MoveOnlyFunc<void(
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/,
        ApplicationData... /*requestData*/)>;

    BasicCustomTunnelServer(NewTunnelHandler newTunnelHandler);
    virtual ~BasicCustomTunnelServer() = default;

    void setTunnelAuthorizer(
        TunnelAuthorizer<ApplicationData...>* tunnelAuthorizer);

    void setRequestPath(const std::string& path);
    std::string requestPath() const;

protected:
    virtual StatusCode::Value validateOpenTunnelRequest(
        const RequestContext& requestContext);

    virtual nx_http::RequestResult processOpenTunnelRequest(
        const nx_http::Request& request,
        nx_http::Response* const response,
        ApplicationData... requestData) = 0;

    /**
     * This should be registered as the handler of the initial "open tunnel" request.
     */
    void processTunnelInitiationRequest(
        RequestContext requestContext,
        nx_http::RequestProcessedHandler completionHandler);

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
        nx_http::RequestProcessedHandler completionHandler,
        ApplicationData... applicationData);

    void openTunnel(
        std::unique_ptr<RequestContext> requestContext,
        nx_http::RequestProcessedHandler completionHandler,
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
    nx_http::RequestProcessedHandler completionHandler)
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
    nx_http::RequestProcessedHandler completionHandler,
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
    nx_http::RequestProcessedHandler completionHandler,
    ApplicationData... applicationData)
{
    auto requestResult = this->processOpenTunnelRequest(
        requestContext->request,
        requestContext->response,
        std::move(applicationData)...);

    nx::utils::swapAndCall(completionHandler, std::move(requestResult));
}

} // namespace nx_http::tunneling::detail
