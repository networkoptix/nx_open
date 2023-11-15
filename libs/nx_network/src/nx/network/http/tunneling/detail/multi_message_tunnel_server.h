// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../../server/rest/http_server_rest_message_dispatcher.h"
#include "basic_custom_tunnel_server.h"
#include "experimental_tunnel_server.h"
#include "request_paths.h"

namespace nx::network::http::tunneling::detail {

template<typename ...ApplicationData>
class MultiMessageTunnelServer:
    public SeparateUpDownConnectionsTunnelServer<ApplicationData...>
{
    using base_type = SeparateUpDownConnectionsTunnelServer<ApplicationData...>;

public:
    MultiMessageTunnelServer(
        typename base_type::NewTunnelHandler newTunnelHandler)
        :
        base_type(std::move(newTunnelHandler))
    {
    }

    virtual ~MultiMessageTunnelServer();

    virtual void registerRequestHandlers(
        const std::string& basePath,
        AbstractMessageDispatcher* messageDispatcher) override;
};

template<typename ...ApplicationData>
MultiMessageTunnelServer<ApplicationData...>::~MultiMessageTunnelServer()
{
    this->closeAllTunnels();
}

template<typename ...ApplicationData>
void MultiMessageTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    AbstractMessageDispatcher* messageDispatcher)
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
