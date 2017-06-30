#include "socket_common.h"

#include <cstring>

#include "socket_global.h"

namespace nx {
namespace network {

bool socketCannotRecoverFromError(SystemError::ErrorCode sysErrorCode)
{
    return sysErrorCode != SystemError::noError
        && sysErrorCode != SystemError::wouldBlock
        && sysErrorCode != SystemError::again
        && sysErrorCode != SystemError::timedOut
        && sysErrorCode != SystemError::interrupted
        && sysErrorCode != SystemError::inProgress;
}

} // namespace network
} // namespace nx

//-------------------------------------------------------------------------------------------------
// HostAddress

const HostAddress HostAddress::localhost(*ipV4from(lit("127.0.0.1")));
const HostAddress HostAddress::anyHost(*ipV4from(lit("0.0.0.0")));

HostAddress::HostAddress( const in_addr& addr ):
    m_ipV4( addr )
{
}

HostAddress::HostAddress( const in6_addr& addr ):
    m_ipV6( addr )
{
}

HostAddress::HostAddress( const QString& addrStr ):
    m_string( addrStr )
{
}

HostAddress::HostAddress( const char* addrStr ):
    HostAddress( QString::fromLatin1(addrStr) )
{
}

HostAddress::~HostAddress()
{
}

bool HostAddress::operator==( const HostAddress& rhs ) const
{
    return isIpAddress() == rhs.isIpAddress() && toString() == rhs.toString();
}

bool HostAddress::operator!=( const HostAddress& rhs ) const
{
    return !(*this == rhs);
}

bool HostAddress::operator<( const HostAddress& rhs ) const
{
    if (isIpAddress() != rhs.isIpAddress())
        return isIpAddress();

    return toString() < rhs.toString();
}

static const QString kIpVersionConvertPart = QLatin1String("::ffff:");
static const QByteArray kIpVersionMapPrefix = QByteArray::fromHex("00000000000000000000FFFF");

const QString& HostAddress::toString() const
{
    if (m_string)
        return *m_string;

    // TODO: Remove this hack when IPv6 is properly supported!
    //  Try to map it from IPv4 as v4 format is preferable
    auto ipV4 = m_ipV4;
    if (!ipV4)
        ipV4 = ipV4from(*m_ipV6);

    if (ipV4)
        m_string = ipToString(*ipV4);
    else
        m_string = ipToString(*m_ipV6);

    return *m_string;
}

boost::optional<in_addr> HostAddress::ipV4() const
{
    if (m_ipV4)
        return m_ipV4;

    // TODO: Remove this hack when IPv6 is properly supported!
    //  Try to map it from IPv4 as v4 format is preferable
    if (m_ipV6)
    {
        if (const auto ipV4 = ipV4from(*m_ipV6))
            return ipV4;
    }

    if (m_string)
    {
        if (const auto ipV4 = ipV4from(*m_string))
            return ipV4;

        const auto ipV6 = ipV6from(*m_string);
        if (ipV6)
        {
            if (const auto ipV4 = ipV4from(*ipV6))
                return ipV4;
        }
    }

    return boost::none;
}

boost::optional<in6_addr> HostAddress::ipV6() const
{
    if (m_ipV6)
        return m_ipV6;

    if (m_ipV4)
        return ipV6from(*m_ipV4);

    if (const auto ipV6 = ipV6from(*m_string))
        return ipV6;

    if (const auto ipV4 = ipV4from(*m_string))
        return ipV6from(*ipV4);

    return boost::none;
}

bool HostAddress::isLocal() const
{
    if (const auto& ip = ipV4())
    {
        const auto addr = ntohl(ip->s_addr);
        return (addr == 0x7F000001) // 127.0.0.1
            || (addr >= 0x0A000000 && addr <= 0x0AFFFFFF) // 10.*.*.*
            || (addr >= 0xAC100000 && addr <= 0xAC1FFFFF) // 172.16.*.* - 172.31.*.*
            || (addr >= 0xC0A80000 && addr <= 0xC0A8FFFF); // 192.168.0.0
    }

    if (const auto& ip = ipV6())
    {
        return (std::memcmp(&*ip, &in6addr_loopback, sizeof(*ip)) == 0) // ::1
            || (ip->s6_addr[0] == 0xFD && ip->s6_addr[1] == 0x00) // FD00:*
            || (ip->s6_addr[0] == 0xFE && ip->s6_addr[1] == 0x80); // FE00:*
    }

    return false; // not even IP address
}

bool HostAddress::isIpAddress() const
{
    return m_ipV4 || m_ipV6;
}

boost::optional<QString> HostAddress::ipToString(const in_addr& addr)
{
    char buffer[1024];
    if (inet_ntop(AF_INET, (void*)&addr, buffer, sizeof(buffer)))
        return QString(QLatin1String(buffer));

    return boost::none;

    return QString(QLatin1String(inet_ntoa(addr)));
}

boost::optional<QString> HostAddress::ipToString(const in6_addr& addr)
{
    char buffer[1024];
    if (inet_ntop(AF_INET6, (void*)&addr, buffer, sizeof(buffer)))
        return QString(QLatin1String(buffer));

    return boost::none;
}

boost::optional<in_addr> HostAddress::ipV4from(const QString& ip)
{
    in_addr v4;
    if (inet_pton(AF_INET, ip.toLatin1().data(), &v4))
        return v4;

    return boost::none;
}

boost::optional<in6_addr> HostAddress::ipV6from(const QString& ip)
{
    in6_addr v6;
    if (inet_pton(AF_INET6, ip.toLatin1().data(), &v6))
        return v6;

    return boost::none;
}

boost::optional<in_addr> HostAddress::ipV4from(const in6_addr& v6)
{
    in_addr v4;

    // TODO: Remove this hack when IPv6 is properly supported!
    //  Try to map it from IPv4 as v4 format is preferable
    if (std::memcmp(&v6, &in6addr_any, sizeof(v6)) == 0)
    {
        v4.s_addr = htonl(INADDR_ANY);
        return v4;
    }
    if (std::memcmp(&v6, &in6addr_loopback, sizeof(v6)) == 0)
    {
        v4.s_addr = htonl(INADDR_LOOPBACK);
        return v4;
    }

    if (std::memcmp(kIpVersionMapPrefix.data(), &v6.s6_addr[0], kIpVersionMapPrefix.size()) != 0)
        return boost::none;

    std::memcpy(&v4, &v6.s6_addr[kIpVersionMapPrefix.size()], sizeof(v4));
    return v4;
}

in6_addr HostAddress::ipV6from(const in_addr& v4)
{
    // TODO: Remove this hack when IPv6 is properly supported!
    //  Try to map it from IPv4 as v4 format is preferable
    if (v4.s_addr == htonl(INADDR_ANY))
        return in6addr_any;
    if (v4.s_addr == htonl(INADDR_LOOPBACK))
        return in6addr_loopback;

    in6_addr v6;
    std::memcpy(&v6.s6_addr[0], kIpVersionMapPrefix.data(), kIpVersionMapPrefix.size());
    std::memcpy(&v6.s6_addr[kIpVersionMapPrefix.size()], &v4, sizeof(v4));
    return v6;
}

void HostAddress::swap(HostAddress& other)
{
    m_string.swap(other.m_string);
    m_ipV4.swap(other.m_ipV4);
    m_ipV6.swap(other.m_ipV6);
}

void swap(HostAddress& one, HostAddress& two)
{
    one.swap(two);
}

//-------------------------------------------------------------------------------------------------
// SocketAddress

SocketAddress::SocketAddress(const HostAddress& _address, quint16 _port):
    address(_address),
    port(_port)
{
}

SocketAddress::SocketAddress(const QString& str):
    port(0)
{
    // NOTE: support all formats
    //  IPv4  <host> or <host>:<port> e.g. 127.0.0.1, 127.0.0.1:80
    //  IPv6  [<host>] or [<host>]:<port> e.g. [::1] [::1]:80
    int sepPos = str.lastIndexOf(QLatin1Char(':'));
    if (sepPos == -1 || str.indexOf(QLatin1Char(']'), sepPos) != -1)
    {
        address = HostAddress(trimIpV6(str));
    }
    else
    {
        address = HostAddress(trimIpV6(str.mid(0, sepPos)));
        port = str.mid(sepPos + 1).toInt();
    }
}

SocketAddress::SocketAddress(const QByteArray& utf8Str):
    SocketAddress(QString::fromUtf8(utf8Str))
{
}

SocketAddress::SocketAddress(const char* utf8Str):
    SocketAddress(QByteArray(utf8Str))
{
}

SocketAddress::~SocketAddress()
{
}

bool SocketAddress::operator==(const SocketAddress& rhs) const
{
    return address == rhs.address && port == rhs.port;
}

bool SocketAddress::operator!=(const SocketAddress& rhs) const
{
    return !(*this == rhs);
}

bool SocketAddress::operator<(const SocketAddress& rhs) const
{
    if (address < rhs.address)
        return true;

    if (rhs.address < address)
        return false;

    return port < rhs.port;
}

QString SocketAddress::toString() const
{
    auto host = address.toString();
    if (host.contains(QLatin1Char(':')))
        host = QString(QLatin1String("[%1]")).arg(host);

    return host + (port > 0 ? QString::fromLatin1(":%1").arg(port) : QString());
}

std::string SocketAddress::toStdString() const
{
    return toString().toStdString();
}

bool SocketAddress::isNull() const
{
    return address == HostAddress() && port == 0;
}

const SocketAddress SocketAddress::anyAddress(HostAddress::anyHost, 0);
const SocketAddress SocketAddress::anyPrivateAddress(HostAddress::localhost, 0);

QString SocketAddress::trimIpV6(const QString& ip)
{
    if (ip.startsWith(QLatin1Char('[')) && ip.endsWith(QLatin1Char(']')))
        return ip.mid(1, ip.length() - 2);

    return ip;
}
