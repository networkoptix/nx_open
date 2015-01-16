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
    if( !addrStr.isEmpty() )
        m_sinAddr.s_addr = inet_addr( addrStr.toLatin1().constData() );
    //otherwise considering 0.0.0.0
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

bool HostAddress::operator==( const HostAddress& right ) const
{
    return memcmp( &m_sinAddr, &right.m_sinAddr, sizeof(m_sinAddr) ) == 0;
}

struct in_addr HostAddress::inAddr() const
{
    return m_sinAddr;
}
