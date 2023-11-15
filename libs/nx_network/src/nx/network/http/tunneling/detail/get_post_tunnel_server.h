// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>
#include <string>

#include <nx/network/aio/async_object_pool.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/log/log.h>

#include "../../http_types.h"
#include "../../server/rest/http_server_rest_message_dispatcher.h"
#include "../abstract_tunnel_authorizer.h"
#include "basic_custom_tunnel_server.h"
#include "get_post_tunnel_server_worker.h"
#include "request_paths.h"

namespace nx::network::http::tunneling::detail {

/**
 * Accepts HTTP tunnel based on subsequent GET/POST requests.
 * Example:
 * <pre><code>
 * C->S:
 * GET /get_post/1 HTTP/1.1
 *
 * S->C:
 * HTTP/1.1 200 OK
 * Content-Length: 10000000000
 * </code></pre>
 *
 * C->S:
 * POST /get_post/1 HTTP/1.1
 * Content-Length: 10000000000
 *
 * After receiving POST request, server uses TCP connection as a binary tunnel.
 * The client is expected to use it as a tunnel after receiving response to GET and sending POST.
 *
 * The tunneling logic itself is implemented in GetPostTunnelServerWorker class.
 * This class keeps a worker instance for each AIO thread and delegates work to a worker
 * depending on AIO thread the "establish tunnel" HTTP request was received in.
 */
template<typename ...ApplicationData>
class GetPostTunnelServer:
    public BasicCustomTunnelServer<ApplicationData...>
{
    using base_type = BasicCustomTunnelServer<ApplicationData...>;

    using Worker = GetPostTunnelServerWorker<ApplicationData...>;

public:
    GetPostTunnelServer(typename base_type::NewTunnelHandler newTunnelHandler);
    virtual ~GetPostTunnelServer();

    virtual void registerRequestHandlers(
        const std::string& basePath,
        AbstractMessageDispatcher* messageDispatcher) override;

private:
    /**
     * Processes the initial (GET) request. This request opens down stream.
     */
    virtual network::http::RequestResult processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... requestData) override;

    std::unique_ptr<Worker> createWorker();

private:
    nx::network::aio::AsyncObjectPool<Worker> m_pool;
};

//-------------------------------------------------------------------------------------------------

template<typename ...ApplicationData>
GetPostTunnelServer<ApplicationData...>::GetPostTunnelServer(
    typename base_type::NewTunnelHandler newTunnelHandler)
    :
    base_type(std::move(newTunnelHandler)),
    m_pool(
        &nx::network::SocketGlobals::instance().aioService(),
        [this]() { return this->createWorker(); })
{
}

template<typename ...ApplicationData>
GetPostTunnelServer<ApplicationData...>::~GetPostTunnelServer()
{
    m_pool.clear();
}

template<typename ...ApplicationData>
void GetPostTunnelServer<ApplicationData...>::registerRequestHandlers(
    const std::string& basePath,
    AbstractMessageDispatcher* messageDispatcher)
{
    const auto path = this->requestPath().empty()
        ? url::joinPath(basePath, kGetPostTunnelPath)
        : this->requestPath();

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::get,
        path,
        [this](auto&&... args)
        {
            this->processTunnelInitiationRequest(std::forward<decltype(args)>(args)...);
        });

    messageDispatcher->registerRequestProcessorFunc(
        nx::network::http::Method::post,
        path,
        [this](auto&&... args)
        {
            m_pool.get()->processOpenUpStreamRequest(std::forward<decltype(args)>(args)...);
        },
        MessageBodyDeliveryType::stream);
}

template<typename ...ApplicationData>
network::http::RequestResult
    GetPostTunnelServer<ApplicationData...>::processOpenTunnelRequest(
        const RequestContext& requestContext,
        ApplicationData... args)
{
    return m_pool.get()->processOpenTunnelRequest(
        requestContext, std::forward<decltype(args)>(args)...);
}

template<typename ...ApplicationData>
std::unique_ptr<typename GetPostTunnelServer<ApplicationData...>::Worker>
    GetPostTunnelServer<ApplicationData...>::createWorker()
{
    return std::make_unique<Worker>(
        [this](auto&&... args) { this->reportTunnel(std::forward<decltype(args)>(args)...); });
}

} // namespace nx::network::http::tunneling::detail
