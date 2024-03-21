// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "port_forwarding.h"

#include <nx/network/aio/async_channel_bridge.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>

namespace nx::cloud::gateway {

namespace {

class Connection:
    public nx::network::aio::BasicPollable,
    public std::enable_shared_from_this<Connection>
{
public:
    using base_type = nx::network::aio::BasicPollable;

    Connection(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, Connection*)> closeConnection,
        const nx::network::SocketAddress& target,
        uint16_t targetPort,
        const nx::network::ssl::AdapterFunc& certificateCheck,
        const conf::RunTimeOptions& runTimeOptions)
        :
        m_closeConnection(std::move(closeConnection)),
        m_target(target),
        m_targetPort(targetPort),
        m_inSocket(std::move(socket)),
        m_httpConnect(std::make_unique<nx::network::http::AsyncClient>(certificateCheck))
    {
        base_type::bindToAioThread(m_inSocket->getAioThread());
        m_httpConnect->bindToAioThread(getAioThread());
        m_httpConnect->addRequestHeaders(runTimeOptions.enforcedHeaders(target));
        m_httpConnect->setOnResponseReceived([this]() { onConnect(); });
        m_httpConnect->setOnDone([this]() { onConnect(); });
    }

    void startReadingConnection(
        std::optional<std::chrono::milliseconds> /*inactivityTimeout*/)
    {
        NX_VERBOSE(this, "Connecting to port %1 on %2", m_targetPort, m_target);
        nx::network::url::Builder builder;
        builder.setScheme(nx::network::http::kSecureUrlSchemeName).setEndpoint(m_target);
        m_httpConnect->doConnect(builder,
            nx::network::SocketAddress(nx::network::HostAddress::localhost, m_targetPort)
                .toString());
    }

private:
    void onConnect()
    {
        const auto response = m_httpConnect->response();
        auto socket = m_httpConnect->takeSocket();
        m_httpConnect->pleaseStopSync();
        if (!response)
        {
            NX_VERBOSE(this,
                "Failed connection to port %1 on %2 without response", m_targetPort, m_target);
            m_closeConnection(m_httpConnect->lastSysErrorCode(), this);
        }
        else if (!nx::network::http::StatusCode::isSuccessCode(response->statusLine.statusCode))
        {
            NX_VERBOSE(this,
                "Failed connection to port %1 on %2: %3", m_targetPort, m_target, *response);
            m_closeConnection(m_httpConnect->lastSysErrorCode(), this);
        }
        else if (!socket)
        {
            NX_VERBOSE(this,
                "Failed connection to port %1 on %2 without socket", m_targetPort, m_target);
            m_closeConnection(m_httpConnect->lastSysErrorCode(), this);
        }
        else
        {
            m_bridge = nx::network::aio::makeAsyncChannelBridge(
                std::move(m_inSocket), std::move(socket));
            m_bridge->bindToAioThread(getAioThread());
            m_bridge->start([this](auto result) { m_closeConnection(result, this); });
            NX_VERBOSE(this, "Forwarding to port %1 on %2", m_targetPort, m_target);
            m_httpConnect.reset();
        }
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        if (m_inSocket)
            m_inSocket->bindToAioThread(aioThread);
        if (m_httpConnect)
            m_httpConnect->bindToAioThread(aioThread);
        if (m_bridge)
            m_bridge->bindToAioThread(aioThread);
    }

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_bridge.reset();
        m_httpConnect.reset();
        m_inSocket.reset();
        m_closeConnection(SystemError::noError, this);
        NX_VERBOSE(this, "Stooped forwarding to port %1 on %2", m_targetPort, m_target);
    }

private:
    const nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, Connection*)> m_closeConnection;
    const nx::network::SocketAddress m_target;
    const uint16_t m_targetPort;
    std::unique_ptr<nx::network::AbstractStreamSocket> m_inSocket;
    std::unique_ptr<nx::network::http::AsyncClient> m_httpConnect;
    std::unique_ptr<nx::network::aio::AsyncChannelBridge> m_bridge;
};

} // namespace

class PortForwarding::Server: public nx::network::server::StreamSocketServer<Server, Connection>
{
public:
    Server(
        const nx::network::SocketAddress& target,
        uint16_t targetPort,
        const nx::network::ssl::AdapterFunc& certificateCheck,
        const conf::RunTimeOptions& runTimeOptions)
        :
        m_target(target),
        m_targetPort(targetPort),
        m_certificateCheck(certificateCheck),
        m_runTimeOptions(runTimeOptions)
    {
    }

    static std::unique_ptr<Server> create(
        const nx::network::SocketAddress& target,
        uint16_t targetPort,
        const nx::network::ssl::AdapterFunc& certificateCheck,
        const conf::RunTimeOptions& runTimeOptions)
    {
        auto server =
            std::make_unique<Server>(target, targetPort, certificateCheck, runTimeOptions);

        if (!server->bind(nx::network::SocketAddress::anyPrivateAddress))
        {
            NX_WARNING(server.get(), "Failed to bind for %1", target);
            return {};
        }

        if (!server->listen())
        {
            NX_WARNING(server.get(), "Failed to listen for %1", target);
            return {};
        }

        return server;
    }

private:
    virtual std::shared_ptr<Connection> createConnection(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket) override
    {
        return std::make_unique<Connection>(
            std::move(socket),
            [this](auto code, auto connection)
            {
                connection->pleaseStop(
                    [this, code, connection]() { closeConnection(code, connection); });
            },
            m_target,
            m_targetPort,
            m_certificateCheck,
            m_runTimeOptions);
    }

private:
    const nx::network::SocketAddress m_target;
    const uint16_t m_targetPort;
    const nx::network::ssl::AdapterFunc m_certificateCheck;
    const conf::RunTimeOptions& m_runTimeOptions;
};

PortForwarding::PortForwarding(const conf::RunTimeOptions& runTimeOptions):
    m_runTimeOptions(runTimeOptions)
{
}

PortForwarding::~PortForwarding()
{
}

std::map<uint16_t, uint16_t> PortForwarding::forward(
    const nx::network::SocketAddress& target,
    const std::set<uint16_t>& targetPorts,
    const nx::network::ssl::AdapterFunc& certificateCheck)
{
    std::vector<std::unique_ptr<Server>> serversToRemove;
    std::map<uint16_t, uint16_t> result;

    NX_MUTEX_LOCKER lock(&m_mutex);
    auto serversIt = m_servers.find(target);
    if (targetPorts.empty())
    {
        if (serversIt == m_servers.end())
            return result;

        for (auto server = serversIt->second.begin(); server != serversIt->second.end();)
        {
            serversToRemove.emplace_back(std::move(server->second));
            server = serversIt->second.erase(server);
        }
        m_servers.erase(serversIt);
        return result;
    }

    auto& servers = serversIt == m_servers.end() ? m_servers[target] : serversIt->second;
    auto targetPort = targetPorts.begin();
    auto server = servers.begin();
    while (targetPort != targetPorts.end() && server != servers.end())
    {
        if (*targetPort < server->first)
        {
            if (auto s = Server::create(target, *targetPort, certificateCheck, m_runTimeOptions))
            {
                result.emplace_hint(result.end(), *targetPort, s->address().port);
                server = std::next(servers.emplace_hint(server, *targetPort, std::move(s)));
            }
            ++targetPort;
            continue;
        }

        if (*targetPort == server->first)
        {
            result.emplace_hint(result.end(), *targetPort, server->second->address().port);
            ++targetPort;
            ++server;
            continue;
        }

        serversToRemove.emplace_back(std::move(server->second));
        server = servers.erase(server);
    }
    while (server != servers.end())
    {
        serversToRemove.emplace_back(std::move(server->second));
        server = servers.erase(server);
    }
    for (; targetPort != targetPorts.end(); ++targetPort)
    {
        if (auto s = Server::create(target, *targetPort, certificateCheck, m_runTimeOptions))
        {
            result.emplace_hint(result.end(), *targetPort, s->address().port);
            server = std::next(servers.emplace_hint(server, *targetPort, std::move(s)));
        }
    }
    if (servers.empty())
        m_servers.erase(serversIt);
    return result;
}

} // namespace nx::cloud::gateway
