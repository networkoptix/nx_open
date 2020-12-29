#include "proxy_handler.h"

#include <typeinfo>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "../run_time_options.h"

namespace nx {
namespace cloud {
namespace gateway {

constexpr const int kSocketTimeoutMs = 29*1000;
using SslMode = nx::network::http::server::proxy::SslMode;

ProxyHandler::ProxyHandler(
    const conf::Settings& settings,
    const conf::RunTimeOptions& runTimeOptions)
:
    m_settings(settings),
    m_runTimeOptions(runTimeOptions)
{
    setTargetHostConnectionInactivityTimeout(
        m_settings.proxy().targetConnectionInactivityTimeout);
}

std::unique_ptr<nx::network::aio::AbstractAsyncConnector>
    ProxyHandler::createTargetConnector()
{
    auto targetPeerConnector = std::make_unique<TargetPeerConnector>();

    targetPeerConnector->setTargetConnectionRecvTimeout(m_settings.tcp().recvTimeout);
    targetPeerConnector->setTargetConnectionSendTimeout(m_settings.tcp().sendTimeout);
    targetPeerConnector->setTimeout(m_settings.tcp().sendTimeout);

    return targetPeerConnector;
}

void ProxyHandler::detectProxyTarget(
    const nx::network::http::HttpServerConnection& connection,
    nx::network::http::Request* const request,
    ProxyTargetDetectedHandler handler)
{
    auto resultCode = nx::network::http::StatusCode::internalServerError;

    TargetHost targetHost;
    if (!request->requestLine.url.host().isEmpty())
        resultCode = cutTargetFromUrl(request, &targetHost);
    else
        resultCode = cutTargetFromPath(request, &targetHost);

    if (resultCode != nx::network::http::StatusCode::ok)
    {
        NX_DEBUG(this, lm("Failed to find address string in request path %1 received from %2")
            .arg(request->requestLine.url).arg(connection.socket()->getForeignAddress()));

        handler(resultCode, TargetHost());
        return;
    }

    if (!network::SocketGlobals::addressResolver()
            .isCloudHostName(targetHost.target.address.toString()))
    {
        // No cloud address means direct IP.
        if (!m_settings.cloudConnect().allowIpTarget)
        {
            NX_DEBUG(this, lm("It is forbidden by settings to proxy to non-cloud address (%1)")
                .arg(targetHost.target));
            handler(nx::network::http::StatusCode::forbidden, TargetHost());
            return;
        }

        if (targetHost.target.port == 0)
            targetHost.target.port = m_settings.http().proxyTargetPort;
    }
    else
    {
        // NOTE: Cloud address should not have port.
        targetHost.target.port = 0;
    }

    if (targetHost.sslMode == SslMode::followIncomingConnection)
        targetHost.sslMode = m_settings.cloudConnect().preferedSslMode;

    if (targetHost.sslMode == SslMode::followIncomingConnection && connection.isSsl())
        targetHost.sslMode = SslMode::enabled;

    if (targetHost.sslMode == SslMode::enabled && !m_settings.http().sslSupport)
    {
        NX_DEBUG(this, lm("SSL requested but forbidden by settings. Originator endpoint: %1")
            .arg(connection.socket()->getForeignAddress()));

        handler(nx::network::http::StatusCode::forbidden, TargetHost());
        return;
    }

    if (targetHost.sslMode == SslMode::followIncomingConnection
        && m_settings.http().sslSupport
        && m_runTimeOptions.isSslEnforced(targetHost.target))
    {
        targetHost.sslMode = SslMode::enabled;
    }

    handler(resultCode, targetHost);
}

network::http::StatusCode::Value ProxyHandler::cutTargetFromUrl(
    nx::network::http::Request* const request,
    TargetHost* const targetHost)
{
    if (!m_settings.http().allowTargetEndpointInUrl)
    {
        NX_DEBUG(this, lm("It is forbidden by settings to specify target in url (%1)")
            .arg(request->requestLine.url));
        return nx::network::http::StatusCode::forbidden;
    }

    // Using original url path.
    auto targetEndpoint = network::SocketAddress(
        request->requestLine.url.host(),
        request->requestLine.url.port(nx::network::http::DEFAULT_HTTP_PORT));

    request->requestLine.url.setScheme(QString());
    request->requestLine.url.setHost(QString());
    request->requestLine.url.setPort(-1);

    nx::network::http::insertOrReplaceHeader(
        &request->headers,
        nx::network::http::HttpHeader("Host", targetEndpoint.toString().toUtf8()));

    targetHost->target = std::move(targetEndpoint);
    return nx::network::http::StatusCode::ok;
}

network::http::StatusCode::Value ProxyHandler::cutTargetFromPath(
    nx::network::http::Request* const request,
    TargetHost* const targetHost)
{
    // Parse path, expected format: /target[/some/longer/url].
    const auto path = request->requestLine.url.path();
    auto pathItems = path.splitRef('/', QString::SkipEmptyParts);
    if (pathItems.isEmpty())
        return nx::network::http::StatusCode::badRequest;

    // Parse first path item, expected format: [protocol:]address[:port].
    auto targetParts = pathItems[0].split(':', QString::SkipEmptyParts);

    // Is port specified?
    if (targetParts.size() > 1)
    {
        bool isPortSpecified;
        targetHost->target.port = targetParts.back().toInt(&isPortSpecified);
        if (isPortSpecified)
            targetParts.pop_back();
        else
            targetHost->target.port = nx::network::http::DEFAULT_HTTP_PORT;
    }

    // Is protocol specified?
    if (targetParts.size() > 1)
    {
        const auto protocol = targetParts.front().toString();
        targetParts.pop_front();

        if (protocol == lit("ssl") || protocol == lit("https"))
            targetHost->sslMode = SslMode::enabled;
        else if (protocol == lit("http"))
            targetHost->sslMode = SslMode::disabled;
    }

    if (targetParts.size() > 1)
        return nx::network::http::StatusCode::badRequest;

    // Get address.
    targetHost->target.address = network::HostAddress(targetParts.front().toString());
    pathItems.pop_front();

    // Restore path without 1st item: /[some/longer/url].
    auto query = request->requestLine.url.query();
    if (pathItems.isEmpty())
    {
        request->requestLine.url = "/";
    }
    else
    {
        NX_ASSERT(pathItems[0].position() > 0);
        request->requestLine.url = path.mid(pathItems[0].position() - 1);  //-1 to include '/'
    }

    request->requestLine.url.setQuery(std::move(query));
    return nx::network::http::StatusCode::ok;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
