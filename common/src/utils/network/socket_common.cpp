/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_common.h"


const HostAddress HostAddress::localhost( QLatin1String("127.0.0.1") );
const HostAddress HostAddress::anyHost( INADDR_ANY );

HostAddress::HostAddress()
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
}

HostAddress::HostAddress( struct in_addr& sinAddr )
{
    memcpy( &m_sinAddr, &sinAddr, sizeof(sinAddr) );
}

HostAddress::HostAddress( uint32_t _ipv4 )
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
    m_sinAddr.s_addr = htonl( _ipv4 );
}

HostAddress::HostAddress( const QString& addrStr )
:
    m_addrStr( addrStr )
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
    m_sinAddr.s_addr = inet_addr( addrStr.toLatin1().constData() );
}

uint32_t HostAddress::ipv4() const
{
    return ntohl(m_sinAddr.s_addr);
}

QString HostAddress::toString() const
{
    if( !m_addrStr )
        m_addrStr = QLatin1String(inet_ntoa(m_sinAddr));
    return m_addrStr.get();
}


SocketAddress::SocketAddress( const HostAddress& _address, unsigned short _port )
:
    address( _address ),
    port( _port )
{
}

SocketAddress::SocketAddress( const QString& addrStr )
:
    port( 0 )
{
    const QStringList& tokens = addrStr.split( QLatin1Char(':') );
    if( tokens.size() > 1 )
        port = tokens[1].toUInt();
    //TODO/IMPL
}

QString SocketAddress::toString() const
{
    return lit("%1:%2").arg(address.toString()).arg(port);
}
