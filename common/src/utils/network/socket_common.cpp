/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_common.h"

#include "host_address_resolver.h"
#include "aio/aioservice.h"


const HostAddress HostAddress::localhost( QLatin1String("127.0.0.1") );
const HostAddress HostAddress::anyHost( INADDR_ANY );

HostAddress::HostAddress()
:
    m_addressResolved(true)
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
}

HostAddress::HostAddress( struct in_addr& sinAddr )
:
    m_sinAddr(sinAddr),
    m_addressResolved(true)
{
}

HostAddress::HostAddress( uint32_t _ipv4 )
:
    m_addressResolved(true)
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
    m_sinAddr.s_addr = htonl( _ipv4 );
}

HostAddress::HostAddress( const QString& addrStr )
:
    m_addrStr( addrStr ),
    m_addressResolved(false)
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
    //if addrStr is an ip address

    if( addrStr == lit("255.255.255.255") )
    {
        m_sinAddr.s_addr = 0xffffffffU;
        m_addressResolved = true;
        return;
    }

    m_sinAddr.s_addr = inet_addr( addrStr.toLatin1().constData() );
    if( m_sinAddr.s_addr != INADDR_NONE )
        m_addressResolved = true;   //addrStr contains valid ip address
}

uint32_t HostAddress::ipv4() const
{
    return ntohl(inAddr().s_addr);
}

QString HostAddress::toString() const
{
    if( !m_addrStr )
    {
        Q_ASSERT( m_addressResolved );
        m_addrStr = QLatin1String(inet_ntoa(m_sinAddr));
    }
    return m_addrStr.get();
}

bool HostAddress::operator==( const HostAddress& rhs ) const
{
    if( m_addressResolved != rhs.m_addressResolved )
        return false;

    return m_addressResolved
        ? memcmp( &m_sinAddr, &rhs.m_sinAddr, sizeof(m_sinAddr) ) == 0
        : m_addrStr == rhs.m_addrStr;
}

struct in_addr HostAddress::inAddr(bool* ok) const
{
    if( !m_addressResolved )
    {
        Q_ASSERT( m_addrStr );
        //resolving address
        //TODO #ak remove const_cast
        HostAddressResolver::instance()->resolveAddressSync( m_addrStr.get(), const_cast<HostAddress*>(this) );
    }
    if( ok )
        *ok = m_addressResolved;
    return m_sinAddr;
}


class SocketGlobalRuntimeInternal
{
public:
    HostAddressResolver hostAddressResolver;
    aio::AIOService aioService;
};

SocketGlobalRuntime::SocketGlobalRuntime()
:
    m_data( new SocketGlobalRuntimeInternal() )
{
}

SocketGlobalRuntime::~SocketGlobalRuntime()
{
    delete m_data;
    m_data = nullptr;
}

Q_GLOBAL_STATIC( SocketGlobalRuntime, socketGlobalRuntimeInstance )

SocketGlobalRuntime* SocketGlobalRuntime::instance()
{
    return socketGlobalRuntimeInstance();
}
