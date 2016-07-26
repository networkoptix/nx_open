
#include "socket_common.h"

#include "socket_global.h"


const HostAddress HostAddress::localhost( QLatin1String("127.0.0.1") );
const HostAddress HostAddress::anyHost( (uint32_t)INADDR_ANY );

static const QByteArray IP_V6_MAP_PREFIX_HEX = QByteArray(10*2, '0') + QByteArray(2*2, 'F');
static const QByteArray IP_V6_MAP_PREFIX = QByteArray::fromHex(IP_V6_MAP_PREFIX_HEX);


///////////////////////////////////////////////////
//   class HostAddress
///////////////////////////////////////////////////

HostAddress::HostAddress()
:
    m_addressResolved(true)
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );
}

HostAddress::~HostAddress()
{
}

HostAddress::HostAddress( const HostAddress& rhs )
:
    m_addrStr( rhs.m_addrStr ),
    m_sinAddr( rhs.m_sinAddr ),
    m_addressResolved( rhs.m_addressResolved )
{
}

HostAddress::HostAddress( HostAddress&& rhs )
:
    m_addrStr( std::move(rhs.m_addrStr) ),
    m_sinAddr( rhs.m_sinAddr ),
    m_addressResolved( rhs.m_addressResolved )
{
    memset( &rhs.m_sinAddr, 0, sizeof(rhs.m_sinAddr) );
    rhs.m_addressResolved = false;
}

HostAddress::HostAddress( const struct in_addr& sinAddr )
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

HostAddress::HostAddress( const QByteArray& _ipv6 )
:
    m_addressResolved(true)
{
    memset( &m_sinAddr, 0, sizeof(m_sinAddr) );

    if( _ipv6.size() != IP_V6_MAP_PREFIX.size() +
        static_cast<int>(sizeof(m_sinAddr.s_addr)) ) return;
    if( !_ipv6.startsWith(IP_V6_MAP_PREFIX) ) return;

    uint32_t _ipv4;
    QDataStream stream(_ipv6.right(sizeof(m_sinAddr.s_addr)));
    stream >> _ipv4;

    m_sinAddr.s_addr = htonl( _ipv4 );
}

HostAddress::HostAddress( const QString& addrStr )
:
    m_addrStr( addrStr ),
    m_addressResolved( false )
{
    initializeFromString(addrStr.toLatin1().constData());
}

HostAddress::HostAddress( const char* addrStr )
:
    m_addrStr( QLatin1String(addrStr) ),
    m_addressResolved(false)
{
    initializeFromString(addrStr);
}

uint32_t HostAddress::ipv4() const
{
    return ntohl(inAddr().s_addr);
}

QByteArray HostAddress::ipv6() const
{
    QByteArray _ipv6;
    QDataStream stream(&_ipv6, QIODevice::WriteOnly);
    stream.writeRawData(IP_V6_MAP_PREFIX.data(), IP_V6_MAP_PREFIX.size());
    stream << ipv4();
    return _ipv6;
}

QString HostAddress::toString() const
{
    if( !m_addrStr )
    {
        NX_ASSERT( m_addressResolved );
        m_addrStr = QLatin1String(inet_ntoa(m_sinAddr));
    }
    return m_addrStr.get();
}

bool HostAddress::isResolved() const
{
    return m_addressResolved;
}

bool HostAddress::isLocalIp() const
{
    // TODO: #mux virify with standart
    static const std::vector<QString> kLocalPrefixes
    {
        QLatin1String("192.168"),
        QLatin1String("127.0.0"),
    };

    const auto string = toString();
    for (const auto& prefix: kLocalPrefixes)
        if (string.startsWith(prefix))
            return true;

    return false;
}

HostAddress& HostAddress::operator=( const HostAddress& rhs )
{
    m_addrStr = rhs.m_addrStr;
    m_sinAddr = rhs.m_sinAddr;
    m_addressResolved = rhs.m_addressResolved;

    return *this;
}

HostAddress& HostAddress::operator=( HostAddress&& rhs )
{
    m_addrStr = std::move(rhs.m_addrStr);
    m_sinAddr = rhs.m_sinAddr;
    m_addressResolved = rhs.m_addressResolved;

    memset( &rhs.m_sinAddr, 0, sizeof(rhs.m_sinAddr) );
    rhs.m_addressResolved = false;

    return *this;
}

bool HostAddress::operator==( const HostAddress& rhs ) const
{
    if( m_addressResolved != rhs.m_addressResolved )
        return false;

    return m_addressResolved
        ? memcmp( &m_sinAddr, &rhs.m_sinAddr, sizeof(m_sinAddr) ) == 0
        : m_addrStr == rhs.m_addrStr;
}

bool HostAddress::operator!=(const HostAddress& right) const
{
    return !(*this == right);
}

bool HostAddress::operator<( const HostAddress& right ) const
{
    if( m_addressResolved < right.m_addressResolved )
        return true;
    if( m_addressResolved > right.m_addressResolved )
        return false;

    return m_addressResolved
        ? m_sinAddr.s_addr < right.m_sinAddr.s_addr
        : m_addrStr < right.m_addrStr;
}

struct in_addr HostAddress::inAddr(bool* ok) const
{
    if( !m_addressResolved )
    {
        NX_ASSERT( m_addrStr );
        const auto addrs = nx::network::SocketGlobals::addressResolver().resolveSync(
                    m_addrStr.get(), false );

        if ( !addrs.empty() )
        {
            // TODO: use IpAddress instead
            m_sinAddr = addrs.front().host.m_sinAddr;
            m_addressResolved = true;
        }
    }
    if( ok )
        *ok = m_addressResolved;
    return m_sinAddr;
}

void HostAddress::initializeFromString(const char* addrStr)
{
    memset(&m_sinAddr, 0, sizeof(m_sinAddr));
    //if addrStr is an ip address

    if (strcmp(addrStr, "") == 0 || strcmp(addrStr, "0.0.0.0") == 0)
    {
        m_addressResolved = true;
        return;
    }

    if (strcmp(addrStr, "255.255.255.255") == 0)
    {
        m_sinAddr.s_addr = 0xffffffffU;
        m_addressResolved = true;
        return;
    }

    m_sinAddr.s_addr = inet_addr(addrStr);
    if (m_sinAddr.s_addr != INADDR_NONE)
        m_addressResolved = true;   //addrStr contains valid ip address
}


///////////////////////////////////////////////////
//   class SocketAddress
///////////////////////////////////////////////////

SocketAddress::SocketAddress()
:
    port(0)
{
}

SocketAddress::~SocketAddress()
{
}

SocketAddress::SocketAddress( HostAddress _address, quint16 _port )
:
    address( std::move(_address) ),
    port( _port )
{
}

SocketAddress::SocketAddress( const QString& str )
:
    port( 0 )
{
    initializeFromString(str);
}

SocketAddress::SocketAddress(const QByteArray& utf8Str)
:
    SocketAddress(QString::fromUtf8(utf8Str))
{
}

SocketAddress::SocketAddress( const char* str )
:
    port( 0 )
{
    initializeFromString(QString::fromUtf8(str));
}

SocketAddress::SocketAddress(const QUrl& url)
    :
    address(url.host()),
    port(url.port(0))
{
}

QString SocketAddress::toString() const
{
    return
        address.toString() +
            (port > 0 ? QString::fromLatin1(":%1").arg(port) : QString());
}

QUrl SocketAddress::toUrl(const QString& scheme)
{
    QUrl url;
    url.setScheme(scheme.isEmpty() ? lit("http") : scheme);
    url.setHost(address.toString());
    if (port > 0)
        url.setPort(port);
    return url;
}

bool SocketAddress::operator==( const SocketAddress& rhs ) const
{
    return address == rhs.address && port == rhs.port;
}

bool SocketAddress::operator!=( const SocketAddress& rhs ) const
{
    return !(*this == rhs);
}

bool SocketAddress::operator<( const SocketAddress& rhs ) const
{
    if( address < rhs.address )
        return true;
    if( rhs.address < address )
        return false;
    return port < rhs.port;
}

bool SocketAddress::isNull() const
{
    return address == HostAddress() && port == 0;
}

const SocketAddress SocketAddress::anyAddress(HostAddress::anyHost, 0);

void SocketAddress::initializeFromString( const QString& str )
{
    int sepPos = str.indexOf(L':');
    if( sepPos == -1 )
    {
        address = HostAddress(str);
    }
    else
    {
        address = HostAddress(str.mid( 0, sepPos ));
        port = str.mid( sepPos+1 ).toInt();
    }
}
