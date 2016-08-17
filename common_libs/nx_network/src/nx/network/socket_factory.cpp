/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "system_socket.h"
#include "udt/udt_socket.h"
#include "ssl_socket.h"

#include <iostream>


using namespace nx::network;

static std::unique_ptr< AbstractStreamSocket > defaultStreamSocketFactoryFunc(
    SocketFactory::NatTraversalType nttType,
    SocketFactory::SocketType forcedSocketType)
{
    switch (forcedSocketType)
    {
        case SocketFactory::SocketType::cloud:
            switch (nttType)
            {
                case SocketFactory::NatTraversalType::nttAuto:
                case SocketFactory::NatTraversalType::nttEnabled:
                #if 0
                    // Using old simple non-Cloud sockets - can be useful for debug.
                    return std::make_unique<TCPSocket>(false, s_tcpClientIpVersion.load());
                #else
                    // Using Cloud-enabled sockets - recommended.
                    return std::make_unique< cloud::CloudStreamSocket >();
                #endif
    return new UDPSocket();
                case SocketFactory::NatTraversalType::nttDisabled:
                    return std::make_unique<TCPSocket>(false, s_tcpClientIpVersion.load());
            }

        case SocketFactory::SocketType::tcp:
            return std::make_unique<TCPSocket>(
                nttType != SocketFactory::NatTraversalType::nttDisabled,
                s_tcpClientIpVersion.load());

        case SocketFactory::SocketType::udt:
            return std::make_unique<UdtStreamSocket>(s_tcpClientIpVersion.load());

        default:
            return nullptr;
    };
}


static std::unique_ptr< AbstractStreamServerSocket > defaultStreamServerSocketFactoryFunc(
    SocketFactory::NatTraversalType nttType,
    SocketFactory::SocketType socketType)
{
    static_cast<void>(nttType);
    switch (socketType)
    {
        case SocketFactory::SocketType::cloud:
            // TODO #mux: uncomment when works properly
            // return std::make_unique< cloud::CloudServerSocket >();

        case SocketFactory::SocketType::tcp:
            return std::make_unique<TCPServerSocket>(s_tcpServerIpVersion.load());

        case SocketFactory::SocketType::udt:
            return std::make_unique<UdtStreamServerSocket>(s_tcpServerIpVersion.load());

        default:
            return nullptr;
    };
}

namespace {
SocketFactory::CreateStreamSocketFuncType createStreamSocketFunc;
SocketFactory::CreateStreamServerSocketFuncType createStreamServerSocketFunc;
}

std::unique_ptr<AbstractDatagramSocket> SocketFactory::createDatagramSocket()
{
    return std::unique_ptr<AbstractDatagramSocket>(new UDPSocket(false, s_tcpClientIpVersion.load()));
}

std::unique_ptr<AbstractStreamSocket> SocketFactory::createStreamSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    if (createStreamSocketFunc)
        return createStreamSocketFunc(sslRequired, natTraversalRequired);

    auto result = defaultStreamSocketFactoryFunc(
        natTraversalRequired,
        s_enforcedStreamSocketType);

    if (!result)
        return std::unique_ptr<AbstractStreamSocket>();

    #ifdef ENABLE_SSL
        if (sslRequired || s_isSslEnforced)
            result.reset(new SslSocket(result.release(), false));
    #endif // ENABLE_SSL

    return std::move(result);
}

std::unique_ptr< AbstractStreamServerSocket > SocketFactory::createStreamServerSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    if (createStreamServerSocketFunc)
        return createStreamServerSocketFunc(sslRequired, natTraversalRequired);

    auto result = defaultStreamServerSocketFactoryFunc(
        natTraversalRequired,
        s_enforcedStreamSocketType);

    if (!result)
        return std::unique_ptr<AbstractStreamServerSocket>();

    #ifdef ENABLE_SSL
        if (s_isSslEnforced)
            result.reset(new SslServerSocket(result.release(), false));
        else
        if (sslRequired)
            result.reset(new SslServerSocket(result.release(), true));
    #endif // ENABLE_SSL

    return std::move( result );
}

QString SocketFactory::toString( SocketType type )
{
    switch ( type )
    {
        case SocketType::cloud: return lit( "cloud" );
        case SocketType::tcp: return lit( "tcp" );
        case SocketType::udt: return lit( "udt" );
    }

    NX_ASSERT( false, lm("Unrecognized socket type: ").arg(static_cast<int>(type)) );
    return QString();
}

SocketFactory::SocketType SocketFactory::stringToSocketType( QString type )
{
    if( type.toLower() == lit("cloud") ) return SocketType::cloud;
    if( type.toLower() == lit("tcp") ) return SocketType::tcp;
    if( type.toLower() == lit("udt") ) return SocketType::udt;

    NX_ASSERT( false, lm("Unrecognized socket type: ").arg(type) );
    return SocketType::cloud;
}

void SocketFactory::enforceStreamSocketType( SocketType type )
{
    s_enforcedStreamSocketType = type;
    qWarning() << ">>> SocketFactory::enforceStreamSocketType("
               << toString( type ) << ") <<<";
}

void SocketFactory::enforceStreamSocketType( QString type )
{
    enforceStreamSocketType( stringToSocketType( type ) );
}

bool SocketFactory::isStreamSocketTypeEnforced()
{
    return s_enforcedStreamSocketType != SocketType::cloud;
}

void SocketFactory::enforceSsl( bool isEnforced )
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

void SocketFactory::setIpVersion( const QString& ipVersion )
{
    if( ipVersion.isEmpty() )
        return;

    NX_LOG( lit("SocketFactory::setIpVersion( %1 )").arg(ipVersion), cl_logALWAYS );

    if( ipVersion == QLatin1String("4") )
    {
        s_udpIpVersion = AF_INET;
        s_tcpClientIpVersion = AF_INET;
        s_tcpServerIpVersion = AF_INET;
        return;
    }

    if( ipVersion == QLatin1String("6") )
    {
        s_udpIpVersion = AF_INET6;
        s_tcpClientIpVersion = AF_INET6;
        s_tcpServerIpVersion = AF_INET6;
        return;
    }

    if( ipVersion == QLatin1String("6tcp") ) /** TCP only */
    {
        s_udpIpVersion = AF_INET;
        s_tcpClientIpVersion = AF_INET6;
        s_tcpServerIpVersion = AF_INET6;
        return;
    }

    if( ipVersion == QLatin1String("6server") ) /** Server only */
    {
        s_udpIpVersion = AF_INET;
        s_tcpClientIpVersion = AF_INET;
        s_tcpServerIpVersion = AF_INET6;
        return;
    }

    std::cerr << "Unsupported IP version: " << ipVersion.toStdString() << std::endl;
    ::abort();
}

std::atomic< SocketFactory::SocketType >
    SocketFactory::s_enforcedStreamSocketType(
        SocketFactory::SocketType::cloud );

std::atomic< bool > SocketFactory::s_isSslEnforced( false );

#if defined(__APPLE__) && defined(TARGET_OS_IPHONE)
    std::atomic<int> SocketFactory::s_tcpServerIpVersion(AF_INET6);
    std::atomic<int> SocketFactory::s_tcpClientIpVersion(AF_INET6);
    std::atomic<int> SocketFactory::s_udpIpVersion(AF_INET6);
#else
    std::atomic<int> SocketFactory::s_tcpServerIpVersion(AF_INET);