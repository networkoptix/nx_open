#pragma once

#include <atomic>
#include <boost/optional/optional.hpp>

#include "abstract_socket.h"
#include "socket_common.h"

namespace nx {
namespace network {
    
class UDPSocket;

/**
 * Contains factory methods for creating sockets.
 * All factory methods return objects created with operator new.
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

    typedef std::function<std::unique_ptr<AbstractStreamSocket>(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/,
        boost::optional<int> /*ipVersion*/)> CreateStreamSocketFuncType;
    typedef std::function<std::unique_ptr<AbstractStreamServerSocket>(
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/,
        boost::optional<int> /*ipVersion*/)> CreateStreamServerSocketFuncType;

    static std::unique_ptr< AbstractDatagramSocket > createDatagramSocket();
    static std::unique_ptr< nx::network::UDPSocket > createUdpSocket();

    /**
     * @param sslRequired If true than it is guaranteed that returned object
     * can be safely cast to AbstractEncryptedStreamSocket.
     */
    static std::unique_ptr< AbstractStreamSocket > createStreamSocket(
        bool sslRequired = false,
        nx::network::NatTraversalSupport natTraversalRequired = nx::network::NatTraversalSupport::enabled,
        boost::optional<int> ipVersion = boost::none);

    static std::unique_ptr< AbstractStreamServerSocket > createStreamServerSocket(
        bool sslRequired = false,
        nx::network::NatTraversalSupport natTraversalRequired = nx::network::NatTraversalSupport::enabled,
        boost::optional<int> ipVersion = boost::none);

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

private:
    SocketFactory();
    SocketFactory(const SocketFactory&);
    SocketFactory& operator=(const SocketFactory&);

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
