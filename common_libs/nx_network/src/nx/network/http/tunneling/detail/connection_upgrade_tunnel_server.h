#pragma once

#include <memory>
#include <string>

#include <nx/network/url/url_parse_helper.h>

#include "basic_custom_tunnel_server.h"
#include "../abstract_tunnel_authorizer.h"
#include "../../server/http_server_connection.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling::detail {

template<typename ...ApplicationData>
class ConnectionUpgradeTunnelServer:
    public BasicCustomTunnelServer<ApplicationData...>
{
    using base_type = BasicCustomTunnelServer<ApplicationData...>;

public:
    ConnectionUpgradeTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~ConnectionUpgradeTunnelServer();

    void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher);

    void setProtocolName(const std::string& protocolName);
    std::string protocolName() const;

    void setUpgradeRequestMethod(http::Method::ValueType method);
    http::Method::ValueType upgradeRequestMethod() const;

private:
    std::string m_protocolToUpgradeTo = "NxProprietary";
    http::Method::ValueType m_upgradeRequestMethod = http::Method::get;

    virtual StatusCode::Value validateOpenTunnelRequest(
        const RequestContext& requestContext) override;

    virtual network::http::RequestResult processOpenTunnelRequest(
        const network::http::Request& request,
        network::http::Response* response,
        ApplicationData... requestData) override;
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
ConnectionUpgradeTunnelServer<ApplicationData...>::ConnectionUpgradeTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
ConnectionUpgradeTunnelServer<ApplicationData...>::~ConnectionUpgradeTunnelServer()
{
}

template<typename ...ApplicationData>
void ConnectionUpgradeTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    using namespace std::placeholders;

    static constexpr char path[] = "upgrade_connection";

    messageDispatcher->registerRequestProcessorFunc(
        m_upgradeRequestMethod,
        !this->requestPath().empty() ? this->requestPath() : url::joinPath(basePath, path),
        std::bind(&ConnectionUpgradeTunnelServer::processTunnelInitiationRequest, this, _1, _2));
}

template<typename ...ApplicationData>
void ConnectionUpgradeTunnelServer<ApplicationData...>::setProtocolName(
    const std::string& protocolName)
{
    m_protocolToUpgradeTo = protocolName;
}

template<typename ...ApplicationData>
std::string ConnectionUpgradeTunnelServer<ApplicationData...>::protocolName() const
{
    return m_protocolToUpgradeTo;
}

template<typename ...ApplicationData>
void ConnectionUpgradeTunnelServer<ApplicationData...>::setUpgradeRequestMethod(
    http::Method::ValueType method)
{
    m_upgradeRequestMethod = method;
}

template<typename ...ApplicationData>
http::Method::ValueType
    ConnectionUpgradeTunnelServer<ApplicationData...>::upgradeRequestMethod() const
{
    return m_upgradeRequestMethod;
}

template<typename ...ApplicationData>
StatusCode::Value ConnectionUpgradeTunnelServer<ApplicationData...>::validateOpenTunnelRequest(
    const RequestContext& requestContext)
{
    auto upgradeIter = requestContext.request.headers.find("Upgrade");
    if (upgradeIter == requestContext.request.headers.end() ||
        upgradeIter->second.toStdString() != m_protocolToUpgradeTo)
    {
        return nx::network::http::StatusCode::badRequest;
    }

    return nx::network::http::StatusCode::ok;
}

template<typename ...ApplicationData>
network::http::RequestResult
    ConnectionUpgradeTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const network::http::Request& /*request*/,
        network::http::Response* /*response*/,
        ApplicationData... requestData)
{
    RequestResult requestResult(StatusCode::switchingProtocols);
    requestResult.connectionEvents.onResponseHasBeenSent =
        [this, requestData = std::make_tuple(std::move(requestData)...)](
            HttpServerConnection* httpConnection) mutable
        {
            auto allArgs = std::tuple_cat(
                std::make_tuple(this, httpConnection->takeSocket()),
                std::move(requestData));

            std::apply(
                &ConnectionUpgradeTunnelServer::reportTunnel,
                std::move(allArgs));
        };
    
    return requestResult;
}

} // namespace nx::network::http::tunneling::detail
