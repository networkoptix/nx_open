// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <functional>
#include <optional>

#include "abstract_socket.h"
#include "socket_common.h"
#include "ssl/ssl_stream_server_socket.h"

namespace nx::network {

namespace ssl {

using AdapterFunc = std::function<std::unique_ptr<nx::network::AbstractEncryptedStreamSocket>(
    std::unique_ptr<nx::network::AbstractStreamSocket> connection)>;

NX_NETWORK_API extern const AdapterFunc kDefaultCertificateCheck;
NX_NETWORK_API extern const AdapterFunc kAcceptAnyCertificate;

} // namespace ssl

class SocketFactoryImpl;

/**
 * Contains factory methods for creating sockets.
 * All factory methods return objects created with operator new.
 * TODO: #akolesnikov Refactor to a single static instance instead of every field being static as of now.
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
        ssl::AdapterFunc adapterFunc,
        bool /*sslRequired*/,
        nx::network::NatTraversalSupport /*natTraversalRequired*/,
        std::optional<int> /*ipVersion*/)>;

    using CreateStreamServerSocketFuncType = std::function<std::unique_ptr<AbstractStreamServerSocket>(
        bool /*sslRequired*/,
        std::optional<int> /*ipVersion*/)>;

    using DatagramSocketFactoryFunc = nx::utils::MoveOnlyFunc<
        std::unique_ptr<AbstractDatagramSocket>(int /*e.g., AF_INET*/)>;

    static std::unique_ptr<AbstractDatagramSocket> createDatagramSocket();

    /**
     * @return Old function.
     */
    static DatagramSocketFactoryFunc setCustomDatagramSocketFactoryFunc(
        DatagramSocketFactoryFunc func);

    /**
     * @param sslRequired If true then it is guaranteed that returned object can be safely cast to
     * AbstractEncryptedStreamSocket.
     */
    static std::unique_ptr<AbstractStreamSocket> createStreamSocket(
        ssl::AdapterFunc adapterFunc,
        bool sslRequired = false,
        NatTraversalSupport natTraversalRequired = NatTraversalSupport::enabled,
        std::optional<int> ipVersion = std::nullopt);

    static std::unique_ptr< AbstractStreamServerSocket > createStreamServerSocket(
        bool sslRequired = false,
        std::optional<int> ipVersion = std::nullopt);

    static std::unique_ptr<nx::network::AbstractStreamServerSocket> createSslAdapter(
        std::unique_ptr<nx::network::AbstractStreamServerSocket> serverSocket,
        ssl::Context* context,
        ssl::EncryptionUse encryptionUse);

    static std::string toString(SocketType type);
    static SocketType stringToSocketType(std::string type);

    /**
     * Enforces factory to produce certain sockets
     * NOTE: DEBUG use ONLY!
     */
    static void enforceStreamSocketType(SocketType type);
    static void enforceStreamSocketType(std::string type);
    static bool isStreamSocketTypeEnforced();

    /**
     * Set new factory.
     * @return old factory.
     */
    static CreateStreamSocketFuncType
        setCreateStreamSocketFunc(CreateStreamSocketFuncType newFactoryFunc);
    static CreateStreamServerSocketFuncType
        setCreateStreamServerSocketFunc(CreateStreamServerSocketFuncType newFactoryFunc);

    static void setIpVersion(const std::string& ipVersion);

    /**
     * @return Previous version.
     */
    static nx::network::IpVersion setUdpIpVersion(nx::network::IpVersion ipVersion);

    static int udpIpVersion();
    static int tcpClientIpVersion();
    static int tcpServerIpVersion();

    static SocketFactory& instance();

    static std::unique_ptr<AbstractStreamSocket> defaultStreamSocketFactoryFunc(
        ssl::AdapterFunc adapterFunc,
        bool sslRequired,
        nx::network::NatTraversalSupport nttType,
        std::optional<int> _ipVersion);

    static std::unique_ptr<AbstractStreamServerSocket> defaultStreamServerSocketFactoryFunc(
        bool sslRequired,
        std::optional<int> _ipVersion);

private:
    std::unique_ptr<SocketFactoryImpl> m_impl;

    SocketFactory();
    SocketFactory(const SocketFactory&) = delete;
    SocketFactory& operator=(const SocketFactory&) = delete;

    ~SocketFactory();

    static std::atomic<SocketType> s_enforcedStreamSocketType;

    static std::atomic<int> s_udpIpVersion;
    static std::atomic<int> s_tcpClientIpVersion;
    static std::atomic<int> s_tcpServerIpVersion;
};

} // namespace nx::network
