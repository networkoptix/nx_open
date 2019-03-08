#pragma once

#include <atomic>

#include <boost/optional/optional.hpp>

#include "abstract_socket.h"
#include "socket_common.h"
#include "ssl/ssl_stream_server_socket.h"

namespace nx {
namespace network {

class SocketFactoryImpl;

/**
 * Contains factory methods for creating sockets.
 * All factory methods return objects created with operator new.
 * TODO: #ak Refactor to a single static instance instead of every field being static as of now.
 */
class NX_NETWORK_API SocketFactory
{
public:
    enum class SocketType
    {
        cloud,  //< Production mode.
        tcp,    //< \class TcpSocket and \class TcpServerSocket.
        udt,    //< \class UdtSocket and \class UdtServerSocket.
    };

    using CreateStreamSocketFuncType = std::function<std::unique_ptr<AbstractStreamSocket>(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/,
        boost::optional<int> /*ipVersion*/)>;

    using CreateStreamServerSocketFuncType = std::function<std::unique_ptr<AbstractStreamServerSocket>(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/,
        boost::optional<int> /*ipVersion*/)>;

    using DatagramSocketFactoryFunc = nx::utils::MoveOnlyFunc<
        std::unique_ptr<AbstractDatagramSocket>(int /*e.g., AF_INET*/)>;

    static std::unique_ptr<AbstractDatagramSocket> createDatagramSocket();

    /**
     * @return Old function.
     */
    static DatagramSocketFactoryFunc setCustomDatagramSocketFactoryFunc(
        DatagramSocketFactoryFunc func);

    /**
     * @param sslRequired If true than it is guaranteed that returned object
     * can be safely cast to AbstractEncryptedStreamSocket.
     */
    static std::unique_ptr< AbstractStreamSocket > createStreamSocket(
        bool sslRequired = false,
        nx::network::NatTraversalSupport natTraversalRequired = nx::network::NatTraversalSupport::enabled,
        boost::optional<int> ipVersion = boost::none);

    static std::unique_ptr<nx::network::AbstractEncryptedStreamSocket> createSslAdapter(
        std::unique_ptr<nx::network::AbstractStreamSocket> connection);

    static std::unique_ptr< AbstractStreamServerSocket > createStreamServerSocket(
        bool sslRequired = false,
        nx::network::NatTraversalSupport natTraversalRequired = nx::network::NatTraversalSupport::enabled,
        boost::optional<int> ipVersion = boost::none);

    static std::unique_ptr<nx::network::AbstractStreamServerSocket> createSslAdapter(
        std::unique_ptr<nx::network::AbstractStreamServerSocket> serverSocket,
        ssl::EncryptionUse encryptionUse);

    static QString toString(SocketType type);
    static SocketType stringToSocketType(QString type);

    /**
     * Enforces factory to produce certain sockets
     * NOTE: DEBUG use ONLY!
     */
    static void enforceStreamSocketType(SocketType type);
    static void enforceStreamSocketType(QString type);
    static bool isStreamSocketTypeEnforced();
    static void enforceSsl(bool isEnforced = true);
    static bool isSslEnforced();

    /**
     * Set new factory.
     * @return old factory.
     */
    static CreateStreamSocketFuncType
        setCreateStreamSocketFunc(CreateStreamSocketFuncType newFactoryFunc);
    static CreateStreamServerSocketFuncType
        setCreateStreamServerSocketFunc(CreateStreamServerSocketFuncType newFactoryFunc);

    static void setIpVersion(const QString& ipVersion);

    /**
     * @return Previous version.
     */
    static nx::network::IpVersion setUdpIpVersion(nx::network::IpVersion ipVersion);

    static int udpIpVersion();
    static int tcpClientIpVersion();
    static int tcpServerIpVersion();

    static SocketFactory& instance();

private:
    std::unique_ptr<SocketFactoryImpl> m_impl;

    SocketFactory();
    SocketFactory(const SocketFactory&) = delete;
    SocketFactory& operator=(const SocketFactory&) = delete;

    ~SocketFactory();

    static std::atomic<SocketType> s_enforcedStreamSocketType;
    static std::atomic<bool> s_isSslEnforced;

    static std::atomic<int> s_udpIpVersion;
    static std::atomic<int> s_tcpClientIpVersion;
    static std::atomic<int> s_tcpServerIpVersion;

    static std::unique_ptr<AbstractStreamSocket> defaultStreamSocketFactoryFunc(
        nx::network::NatTraversalSupport nttType,
        SocketType forcedSocketType,
        boost::optional<int> _ipVersion);

    static std::unique_ptr<AbstractStreamServerSocket> defaultStreamServerSocketFactoryFunc(
        nx::network::NatTraversalSupport nttType,
        SocketType socketType,
        boost::optional<int> _ipVersion);
};

} // namespace network
} // namespace nx
