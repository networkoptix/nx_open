// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reverse_proxy_handler.h"

#include <nx/network/url/url_parse_helper.h>

#include "proxy_handler.h"

namespace nx::network::http::server::proxy {

namespace detail {

class ProxyHandler:
    public AbstractProxyHandler
{
public:
    ProxyHandler(TargetHost target):
        m_target(std::move(target))
    {
    }

protected:
    virtual void detectProxyTarget(
        const nx::network::http::ConnectionAttrs& /*connAttrs*/,
        const nx::network::SocketAddress& /*requestSource*/,
        nx::network::http::Request* const /*request*/,
        ProxyTargetDetectedHandler handler) override
    {
        handler(StatusCode::ok, m_target);
    }

private:
    TargetHost m_target;
};

} // namespace

//-------------------------------------------------------------------------------------------------

bool ReverseProxyHandler::add(
    const Method& method,
    const std::string_view& pathRegex,
    const nx::utils::Url& targetUrl)
{
    AbstractProxyHandler::TargetHost proxyTarget;
    proxyTarget.target = url::getEndpoint(targetUrl);
    proxyTarget.sslMode = nx::utils::stricmp(targetUrl.scheme().toStdString(), "https") == 0
        ? SslMode::enabled : SslMode::disabled;

    return m_dispatcher.registerRequestProcessor(
        pathRegex,
        [proxyTarget]() { return std::make_unique<detail::ProxyHandler>(proxyTarget); },
        method);
}

void ReverseProxyHandler::serve(
    network::http::RequestContext requestContext,
    nx::utils::MoveOnlyFunc<void(network::http::RequestResult)> completionHandler)
{
    m_dispatcher.serve(std::move(requestContext), std::move(completionHandler));
}

} // namespace nx::network::http::server::proxy
