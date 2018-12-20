#pragma once

#include "basic_custom_tunnel_server.h"
#include "experimental_tunnel_server.h"
#include "request_paths.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::tunneling::detail {

template<typename ...ApplicationData>
class MultiMessageTunnelServer:
    public SeparateUpDownConnectionsTunnelServer<ApplicationData...>
{
    using base_type = SeparateUpDownConnectionsTunnelServer<ApplicationData...>;

public:
    MultiMessageTunnelServer(
        typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~MultiMessageTunnelServer();

    virtual void registerRequestHandlers(
        const std::string& basePath,
        server::rest::MessageDispatcher* messageDispatcher) override;
};

template<typename ...ApplicationData>
MultiMessageTunnelServer<ApplicationData...>::MultiMessageTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler))
{
}

template<typename ...ApplicationData>
MultiMessageTunnelServer<ApplicationData...>::~MultiMessageTunnelServer()
{
    this->closeAllTunnels();
}

template<typename ...ApplicationData>
void MultiMessageTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    server::rest::MessageDispatcher* messageDispatcher)
{
    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        url::joinPath(basePath, kMultiMessageTunnelDownPath),
        [this](auto&&... args) { this->processTunnelInitiationRequest(std::move(args)...); });

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        url::joinPath(basePath, kMultiMessageTunnelUpPath),
        [this](auto&&... args) { this->processOpenTunnelUpChannelRequest(std::move(args)...); });
}

} // namespace nx::network::http::tunneling::detail
