// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stun_over_http_server.h"

namespace nx::network::stun {

namespace detail {

class TunnelAuthorizer:
    public nx::network::http::tunneling::TunnelAuthorizer<nx::utils::stree::StringAttrDict>
{
public:
    virtual void authorize(
        const nx::network::http::RequestContext* requestContext,
        CompletionHandler completionHandler) override
    {
        completionHandler(nx::network::http::StatusCode::ok, {}, requestContext->attrs);
    }
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

StunOverHttpServer::StunOverHttpServer(
    nx::network::stun::AbstractMessageHandler* messageHandler)
    :
    m_messageHandler(messageHandler),
    m_tunnelAuthorizer(std::make_unique<detail::TunnelAuthorizer>()),
    m_httpTunnelingServer(
        [this](auto&&... args) { createStunConnection(std::forward<decltype(args)>(args)...); },
        m_tunnelAuthorizer.get())
{
}

StunOverHttpServer::~StunOverHttpServer() = default;

StunOverHttpServer::StunConnectionPool& StunOverHttpServer::stunConnectionPool()
{
    return m_stunConnectionPool;
}

const StunOverHttpServer::StunConnectionPool& StunOverHttpServer::stunConnectionPool() const
{
    return m_stunConnectionPool;
}

void StunOverHttpServer::setInactivityTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_inactivityTimeout = timeout;
}

void StunOverHttpServer::createStunConnection(
    std::unique_ptr<AbstractStreamSocket> connection,
    nx::utils::stree::StringAttrDict attrs)
{
    auto stunConnection = std::make_shared<nx::network::stun::ServerConnection>(
        std::move(connection),
        m_messageHandler,
        std::move(attrs));
    m_stunConnectionPool.saveConnection(stunConnection);
    stunConnection->startReadingConnection(m_inactivityTimeout);
}

void StunOverHttpServer::createStunConnection(
    std::unique_ptr<AbstractStreamSocket> connection,
    nx::network::http::RequestContext ctx)
{
    createStunConnection(std::move(connection), std::move(ctx.attrs));
}

} // namespace nx::network::stun
