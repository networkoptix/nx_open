// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/connection_server/stream_socket_server.h>
#include <nx/network/connection_server/base_server_connection.h>
#include <nx/network/connection_server/detail/connection_statistics.h>

#include <nx/network/aio/abstract_async_connector.h>
#include <nx/network/aio/stream_socket_connector.h>
#include <nx/network/aio/async_channel_bridge.h>

namespace nx::network::socks5 {

/**
 * Interface class instantiated by SOCKS5 server for each connection. Enables username/password
 * verification (if needed), establishes outgoing tunnel connection to server-provided host:port.
 */
class AbstractTunnelConnector: public aio::BasicPollable
{
public:
    using DoneCallback =
        nx::utils::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            std::unique_ptr<nx::network::AbstractStreamSocket>)>;

    /** The server calls this method to check if authentication is required for the connection. */
    virtual bool hasAuth() const = 0;

    /**
     * If authentication is required, the server call this method to verify credentials.
     * @return true if credentials are accepted, false otherwise.
     */
    virtual bool auth(const std::string& user, const std::string& password) = 0;

    /**
     * Server calls this method when connection to specified destionation is requested.
     *
     * @param address destination host and port
     * @param onDone function to call when connection is succeeded or failed, pass connected socket
     * as parameter in case of success, pass nullptr otherwise.
     */
    virtual void connectTo(const SocketAddress& address, DoneCallback onDone) = 0;

    virtual ~AbstractTunnelConnector() = default;
};

/** The purpose of this class is to hide implementation details of SOCKS5 server. */
class AbstractServerConnection:
    public nx::network::server::BaseServerConnection,
    public std::enable_shared_from_this<AbstractServerConnection>
{
    using base_type = nx::network::server::BaseServerConnection;

public:
    AbstractServerConnection(std::unique_ptr<nx::network::AbstractStreamSocket> streamSocket):
        base_type(std::move(streamSocket))
    {}

    nx::network::server::detail::ConnectionStatistics connectionStatistics;
};

/**
 * SOCKS5 server implementation, supports optional username/password authentication and establishes
 * the TCP tunnel connection to a host specified by domain, IPv4 or IPv6 address and port.
 * Uses AbstractTunnelConnector for authentication and outgoing connections.
 */
class NX_NETWORK_API Server:
    public nx::network::server::StreamSocketServer<Server, AbstractServerConnection>
{
    using base_type = nx::network::server::StreamSocketServer<Server, AbstractServerConnection>;

public:
    using ConnectorFactoryFunc =
        nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractTunnelConnector>()>;

    /**
     * Create server instance.
     *
     * @param connectorFactoryFunc a function which will be called by the server to instantiate
     * an AbstractTunnelConnector for each incoming connection.
     */
    Server(ConnectorFactoryFunc connectorFactoryFunc):
        m_connectorFactoryFunc(std::move(connectorFactoryFunc))
    {
    }

protected:
    virtual std::shared_ptr<AbstractServerConnection> createConnection(
        std::unique_ptr<nx::network::AbstractStreamSocket> streamSocket) override;

private:
    ConnectorFactoryFunc m_connectorFactoryFunc;
};

} // namespace nx::network::socks5
