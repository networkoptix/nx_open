#pragma once

#include "../abstract_tunnel_authorizer.h"
#include "../../server/http_server_connection.h"

namespace nx::network::http::tunneling::detail {

template<typename ApplicationData>
class BasicCustomTunnelServer
{
public:
    using NewTunnelHandler = nx::utils::MoveOnlyFunc<void(
        ApplicationData /*requestData*/,
        std::unique_ptr<network::AbstractStreamSocket> /*connection*/)>;

    BasicCustomTunnelServer(NewTunnelHandler newTunnelHandler);
    virtual ~BasicCustomTunnelServer() = default;

    void setTunnelAuthorizer(
        TunnelAuthorizer<ApplicationData>* tunnelAuthorizer);

    void setRequestPath(const std::string& path);
    std::string requestPath() const;

protected:
    virtual StatusCode::Value validateOpenTunnelRequest(
        const RequestContext& requestContext);

    virtual network::http::RequestResult processOpenTunnelRequest(
        ApplicationData requestData,
        const network::http::Request& request,
        network::http::Response* const response) = 0;

    /**
     * This should be registered as the handler of the initial "open tunnel" request.
     */
    void processTunnelInitiationRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);

    void reportTunnel(
        ApplicationData applicationData,
        std::unique_ptr<AbstractStreamSocket> connection);

private:
    NewTunnelHandler m_newTunnelHandler;
    TunnelAuthorizer<ApplicationData>* m_tunnelAuthorizer = nullptr;
    std::string m_path;

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
BasicCustomTunnelServer<ApplicationData>::BasicCustomTunnelServer(
    NewTunnelHandler newTunnelHandler)
    :
    m_newTunnelHandler(std::move(newTunnelHandler))
{
}

template<typename ApplicationData>
void BasicCustomTunnelServer<ApplicationData>::setTunnelAuthorizer(
    TunnelAuthorizer<ApplicationData>* tunnelAuthorizer)
{
    m_tunnelAuthorizer = tunnelAuthorizer;
}

template<typename ApplicationData>
void BasicCustomTunnelServer<ApplicationData>::setRequestPath(
    const std::string& path)
{
    m_path = path;
}

template<typename ApplicationData>
std::string BasicCustomTunnelServer<ApplicationData>::requestPath() const
{
    return m_path;
}

template<typename ApplicationData>
StatusCode::Value BasicCustomTunnelServer<ApplicationData>::validateOpenTunnelRequest(
    const RequestContext& /*requestContext*/)
{
    return StatusCode::ok;
}

template<typename ApplicationData>
void BasicCustomTunnelServer<ApplicationData>::processTunnelInitiationRequest(
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
                    ApplicationData applicationData) mutable
            {
                onTunnelAutorizationCompleted(
                    result,
                    std::move(applicationData),
                    std::move(requestContext),
                    std::move(completionHandler));
            });
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
void BasicCustomTunnelServer<ApplicationData>::reportTunnel(
    ApplicationData applicationData,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    m_newTunnelHandler(
        std::move(applicationData),
        std::move(connection));
}

template<typename ApplicationData>
void BasicCustomTunnelServer<ApplicationData>::onTunnelAutorizationCompleted(
    StatusCode::Value authorizationResult,
    ApplicationData applicationData,
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (!StatusCode::isSuccessCode(authorizationResult))
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
void BasicCustomTunnelServer<ApplicationData>::openTunnel(
    ApplicationData applicationData,
    std::unique_ptr<RequestContext> requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    auto requestResult = this->processOpenTunnelRequest(
        std::move(applicationData),
        requestContext->request,
        requestContext->response);

    nx::utils::swapAndCall(completionHandler, std::move(requestResult));
}

} // namespace nx::network::http::tunneling::detail
