#include "socket_factory.h"

#include <iostream>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/cloud_stream_socket.h>
#include <nx/network/ssl/ssl_stream_server_socket.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/system_socket.h>
#include <nx/network/udt/udt_socket.h>
#include <nx/utils/basic_factory.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {

namespace {

SocketFactory::CreateStreamSocketFuncType createStreamSocketFunc;
SocketFactory::CreateStreamServerSocketFuncType createStreamServerSocketFunc;

} // namespace

class SocketFactoryImpl
{
public:
    using DatagramSocketFactory = nx::utils::BasicAbstractObjectFactory<
        AbstractDatagramSocket, UDPSocket, int /*e.g., AF_INET*/>;

    DatagramSocketFactory datagramSocketFactory;
};

//-------------------------------------------------------------------------------------------------
// Datagram factory.

std::unique_ptr<AbstractDatagramSocket> SocketFactory::createDatagramSocket()
{
    return instance().m_impl->datagramSocketFactory.create(s_udpIpVersion.load());
}

SocketFactory::DatagramSocketFactoryFunc SocketFactory::setCustomDatagramSocketFactoryFunc(
    DatagramSocketFactoryFunc func)
{
    return instance().m_impl->datagramSocketFactory.setCustomFunc(std::move(func));
}

//-------------------------------------------------------------------------------------------------

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
        return createSslAdapter(std::move(result));
#endif // ENABLE_SSL

    return result;
}

std::unique_ptr<nx::network::AbstractEncryptedStreamSocket> SocketFactory::createSslAdapter(
    std::unique_ptr<nx::network::AbstractStreamSocket> connection)
{
    return std::make_unique<ssl::ClientStreamSocket>(std::move(connection));
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
    if (s_isSslEnforced || sslRequired)
    {
        return createSslAdapter(
            std::move(result),
            s_isSslEnforced
                ? ssl::EncryptionUse::always
                : ssl::EncryptionUse::autoDetectByReceivedData);
    }
#endif // ENABLE_SSL

    return result;
}

std::unique_ptr<nx::network::AbstractStreamServerSocket> SocketFactory::createSslAdapter(
    std::unique_ptr<nx::network::AbstractStreamServerSocket> serverSocket,
    ssl::EncryptionUse encryptionUse)
{
    return std::make_unique<ssl::StreamServerSocket>(
        std::move(serverSocket),
        encryptionUse);
}

QString SocketFactory::toString(SocketType type)
{
    switch (type)
    {
        case SocketType::cloud:
            return "cloud";
        case SocketType::tcp:
            return "tcp";
        case SocketType::udt:
            return "udt";
    }

    NX_ASSERT(false, lm("Unrecognized socket type: ").arg(static_cast<int>(type)));
    return QString();
}

SocketFactory::SocketType SocketFactory::stringToSocketType(QString type)
{
    if (type.toLower() == "cloud")
        return SocketType::cloud;
    if (type.toLower() == "tcp")
        return SocketType::tcp;
    if (type.toLower() == "udt")
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

    NX_INFO(typeid(SocketFactory), lm("SocketFactory::setIpVersion(%1)").arg(ipVersion));

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

SocketFactory& SocketFactory::instance()
{
    static SocketFactory staticInstance;
    return staticInstance;
}

SocketFactory::SocketFactory():
    m_impl(std::make_unique<SocketFactoryImpl>())
{
}

SocketFactory::~SocketFactory()
{
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
                    return std::make_unique<cloud::CloudStreamSocket>(ipVersion);

                case NatTraversalSupport::disabled:
                    return std::make_unique<TCPSocket>(ipVersion);
            }
            break;

        case SocketFactory::SocketType::tcp:
            return std::make_unique<TCPSocket>(ipVersion);

        case SocketFactory::SocketType::udt:
            return std::make_unique<UdtStreamSocket>(ipVersion);

        default:
            break;
    };

    return nullptr;
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

} // namespace network
} // namespace nx
