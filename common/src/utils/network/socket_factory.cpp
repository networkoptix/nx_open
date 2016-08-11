/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "system_socket.h"
#include "ssl_socket.h"

#include <common/common_globals.h>
#include <utils/common/log.h>


AbstractDatagramSocket* SocketFactory::createDatagramSocket()
{
    return new UDPSocket( s_udpIpVersion.load() );
}

AbstractStreamSocket* SocketFactory::createStreamSocket( bool sslRequired, SocketFactory::NatTraversalType /*natTraversalRequired*/ )
{
    AbstractStreamSocket* result = new TCPSocket( s_tcpClientIpVersion.load() );
#ifdef ENABLE_SSL
    if (sslRequired)
        result = new QnSSLSocket(result, false);
#endif
    
    return result;
}

AbstractStreamServerSocket* SocketFactory::createStreamServerSocket( bool sslRequired, SocketFactory::NatTraversalType /*natTraversalRequired*/ )
{
#ifdef ENABLE_SSL
    if (sslRequired)
        return new TCPSslServerSocket( s_tcpServerIpVersion.load() );
    else
        return new TCPServerSocket( s_tcpServerIpVersion.load() );
#else
    return new TCPServerSocket();
#endif // ENABLE_SSL
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

#if defined(__APPLE__) && defined(TARGET_OS_IPHONE)
    std::atomic<int> SocketFactory::m_tcpServerIpVersion(AF_INET6);
    std::atomic<int> SocketFactory::s_tcpClientIpVersion(AF_INET6);
    std::atomic<int> SocketFactory::s_udpIpVersion(AF_INET6);
#else
    std::atomic<int> SocketFactory::s_tcpServerIpVersion(AF_INET);
    std::atomic<int> SocketFactory::s_tcpClientIpVersion(AF_INET);
    std::atomic<int> SocketFactory::s_udpIpVersion(AF_INET);
#endif
