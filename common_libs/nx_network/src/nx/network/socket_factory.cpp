#include "socket_factory.h"

#include <iostream>

#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/ssl_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/std/cpp14.h>

using namespace nx::network;

namespace {

SocketFactory::CreateStreamSocketFuncType createStreamSocketFunc;
SocketFactory::CreateStreamServerSocketFuncType createStreamServerSocketFunc;

} // namespace

std::unique_ptr<AbstractDatagramSocket> SocketFactory::createDatagramSocket()
{
    return createUdpSocket();
}

std::unique_ptr<UDPSocket> SocketFactory::createUdpSocket()
{
    return std::make_unique<UDPSocket>(s_udpIpVersion.load());
}

std::unique_ptr<AbstractStreamSocket> SocketFactory::createStreamSocket(
    bool sslRequired,
    NatTraversalSupport natTraversalSupport,
    boost::optional<int> ipVersion)
{
    if (createStreamSocketFunc)
        return createStreamSocketFunc(sslRequired, natTraversalSupport, ipVersion);

    auto result = defaultStreamSocketFactoryFunc(
        natTraversalSupport,
        s_enforcedStreamSocketType,
        ipVersion);

    if (!result)
        return std::unique_ptr<AbstractStreamSocket>();

#ifdef ENABLE_SSL
    if (sslRequired || s_isSslEnforced)
        result.reset(new deprecated::SslSocket(std::move(result), false));
#endif // ENABLE_SSL

    return std::move(result);
}

std::unique_ptr< AbstractStreamServerSocket > SocketFactory::createStreamServerSocket(
    bool sslRequired,
    NatTraversalSupport natTraversalSupport,
    boost::optional<int> ipVersion)
{
    if (createStreamServerSocketFunc)
        return createStreamServerSocketFunc(sslRequired, natTraversalSupport, ipVersion);

    auto result = defaultStreamServerSocketFactoryFunc(
        natTraversalSupport,
        s_enforcedStreamSocketType,
        ipVersion);

    if (!result)
        return std::unique_ptr<AbstractStreamServerSocket>();

#ifdef ENABLE_SSL
    if (s_isSslEnforced)
        result.reset(new deprecated::SslServerSocket(std::move(result), false));
    else
        if (sslRequired)
            result.reset(new deprecated::SslServerSocket(std::move(result), true));
#endif // ENABLE_SSL

    return std::move(result);
}

QString SocketFactory::toString(SocketType type)
{
    switch (type)
    {
        case SocketType::cloud:
            return lit("cloud");
        case SocketType::tcp:
            return lit("tcp");
        case SocketType::udt:
            return lit("udt");
    }

    NX_ASSERT(false, lm("Unrecognized socket type: ").arg(static_cast<int>(type)));
    return QString();
}

SocketFactory::SocketType SocketFactory::stringToSocketType(QString type)
{
    if (type.toLower() == lit("cloud"))
        return SocketType::cloud;
    if (type.toLower() == lit("tcp"))
        return SocketType::tcp;
    if (type.toLower() == lit("udt"))
        return SocketType::udt;

    NX_ASSERT(false, lm("Unrecognized socket type: ").arg(type));
    return SocketType::cloud;
}

void SocketFactory::enforceStreamSocketType(SocketType type)
{
    s_enforcedStreamSocketType = type;
    qWarning() << ">>> SocketFactory::enforceStreamSocketType("
        << toString(type) << ") <<<";
}

void SocketFactory::enforceStreamSocketType(QString type)
{
    enforceStreamSocketType(stringToSocketType(type));
}

bool SocketFactory::isStreamSocketTypeEnforced()
{
    return s_enforcedStreamSocketType != SocketType::cloud;
}

void SocketFactory::enforceSsl(bool isEnforced)
{
    s_isSslEnforced = isEnforced;
    qWarning() << ">>> SocketFactory::enforceSsl(" << isEnforced << ") <<<";
}

bool SocketFactory::isSslEnforced()
{
    return s_isSslEnforced;
}

SocketFactory::CreateStreamSocketFuncType
SocketFactory::setCreateStreamSocketFunc(
    CreateStreamSocketFuncType newFactoryFunc)
{
    auto bak = std::move(createStreamSocketFunc);
    createStreamSocketFunc = std::move(newFactoryFunc);
    return bak;
}

SocketFactory::CreateStreamServerSocketFuncType
SocketFactory::setCreateStreamServerSocketFunc(
    CreateStreamServerSocketFuncType newFactoryFunc)
{
    auto bak = std::move(createStreamServerSocketFunc);
    createStreamServerSocketFunc = std::move(newFactoryFunc);
    return bak;
}

void SocketFactory::setIpVersion(const QString& ipVersion)
{
    if (ipVersion.isEmpty())
        return;

    NX_LOG(lit("SocketFactory::setIpVersion( %1 )").arg(ipVersion), cl_logALWAYS);

    if (ipVersion == QLatin1String("4"))
    {
        s_udpIpVersion = AF_INET;
        s_tcpClientIpVersion = AF_INET;
        s_tcpServerIpVersion = AF_INET;
        return;
    }

    if (ipVersion == QLatin1String("6"))
    {
        s_udpIpVersion = AF_INET6;
        s_tcpClientIpVersion = AF_INET6;
        s_tcpServerIpVersion = AF_INET6;
        return;
    }

    if (ipVersion == QLatin1String("6tcp")) /** TCP only */
    {
        s_udpIpVersion = AF_INET;
        s_tcpClientIpVersion = AF_INET6;
        s_tcpServerIpVersion = AF_INET6;
        return;
    }

    if (ipVersion == QLatin1String("6server")) /** Server only */
    {
        s_udpIpVersion = AF_INET;
        s_tcpClientIpVersion = AF_INET;
        s_tcpServerIpVersion = AF_INET6;
        return;
    }

    std::cerr << "Unsupported IP version: " << ipVersion.toStdString() << std::endl;
    ::abort();
}

nx::network::IpVersion SocketFactory::setUdpIpVersion(nx::network::IpVersion ipVersion)
{
    const int prevVersion = s_udpIpVersion.load();
    s_udpIpVersion = static_cast<int>(ipVersion);
    return static_cast<nx::network::IpVersion>(prevVersion);
}

int SocketFactory::udpIpVersion()
{
    return s_udpIpVersion;
}

int SocketFactory::tcpClientIpVersion()
{
    return s_tcpClientIpVersion;
}

int SocketFactory::tcpServerIpVersion()
{
    return s_tcpServerIpVersion;
}

std::atomic< SocketFactory::SocketType >
SocketFactory::s_enforcedStreamSocketType(
    SocketFactory::SocketType::cloud);

std::atomic< bool > SocketFactory::s_isSslEnforced(false);

#if TARGET_OS_IPHONE
    std::atomic<int> SocketFactory::s_tcpServerIpVersion(AF_INET6);
    std::atomic<int> SocketFactory::s_tcpClientIpVersion(AF_INET6);
    std::atomic<int> SocketFactory::s_udpIpVersion(AF_INET6);
#else
    std::atomic<int> SocketFactory::s_tcpServerIpVersion(AF_INET);
    std::atomic<int> SocketFactory::s_tcpClientIpVersion(AF_INET);
    std::atomic<int> SocketFactory::s_udpIpVersion(AF_INET);
#endif

std::unique_ptr<AbstractStreamSocket> SocketFactory::defaultStreamSocketFactoryFunc(
    NatTraversalSupport nttType,
    SocketType forcedSocketType,
    boost::optional<int> _ipVersion)
{
    auto ipVersion = (bool) _ipVersion ? *_ipVersion : s_tcpClientIpVersion.load();
    switch (forcedSocketType)
    {
        case SocketFactory::SocketType::cloud:
            switch (nttType)
            {
                case NatTraversalSupport::enabled:
                    if (SocketGlobals::ini().disableCloudSockets)
                        return std::make_unique<TCPSocket>(ipVersion);
                    return std::make_unique<cloud::CloudStreamSocket>(ipVersion);

                case NatTraversalSupport::disabled:
                    return std::make_unique<TCPSocket>(ipVersion);
            }

        case SocketFactory::SocketType::tcp:
            return std::make_unique<TCPSocket>(ipVersion);

        case SocketFactory::SocketType::udt:
            return std::make_unique<UdtStreamSocket>(ipVersion);

        default:
            return nullptr;
    };
}

std::unique_ptr<AbstractStreamServerSocket> SocketFactory::defaultStreamServerSocketFactoryFunc(
    NatTraversalSupport nttType, SocketType socketType, boost::optional<int> _ipVersion)
{
    auto ipVersion = (bool) _ipVersion ? *_ipVersion : s_tcpServerIpVersion.load();
    static_cast<void>(nttType);
    switch (socketType)
    {
        case SocketFactory::SocketType::cloud:
            // TODO #mux: uncomment when works properly
            // return std::make_unique<cloud::CloudServerSocket>(ipVersion);

        case SocketFactory::SocketType::tcp:
            return std::make_unique<TCPServerSocket>(ipVersion);

        case SocketFactory::SocketType::udt:
            return std::make_unique<UdtStreamServerSocket>(ipVersion);

        default:
            return nullptr;
    };
}
