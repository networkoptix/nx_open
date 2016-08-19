
#include "socket_common.h"

#include "socket_global.h"

const HostAddress HostAddress::localhost( "127.0.0.1" );
const HostAddress HostAddress::anyHost( "0.0.0.0" );

HostAddress::HostAddress( const in_addr& addr )
:
    m_ipV4( addr )
{
}

HostAddress::HostAddress( const in6_addr& addr )
:
    m_ipV6( addr )
{
}

HostAddress::HostAddress( const QString& addrStr )
:
    m_string( addrStr )
{
}

HostAddress::HostAddress( const char* addrStr )
:
    m_string( QLatin1String(addrStr) )
{
}

bool HostAddress::isResolved() const
{
    return ipV4() || ipV6();
}

bool HostAddress::isLocal() const
{
    const auto& string = toString();

    // TODO: add some more IPv4
    return string == QLatin1String("127.0.0.1") // localhost
        || string.startsWith(QLatin1String("10.")) // IPv4 private
        || string.startsWith(QLatin1String("192.168")) // IPv4 private
        || string.startsWith(QLatin1String("fd00::")) // IPv6 private
        || string.startsWith(QLatin1String("fe80::")); // IPv6 link-local
}

bool HostAddress::operator==( const HostAddress& rhs ) const
{
    if (isResolved() != rhs.isResolved())
        return false;

    if (!isResolved())
        return toString() == rhs.toString();

    if (ipV4() && rhs.ipV4())
        return memcmp(m_ipV4.get_ptr(), rhs.m_ipV4.get_ptr(), sizeof(*m_ipV4)) == 0;

    if (ipV6() && rhs.ipV6())
        return memcmp(m_ipV6.get_ptr(), rhs.m_ipV6.get_ptr(), sizeof(*m_ipV6)) == 0;

    return false;
}

bool HostAddress::operator!=( const HostAddress& rhs ) const
{
    return toString() != rhs.toString();
}

bool HostAddress::operator<( const HostAddress& rhs ) const
{
    return toString() < rhs.toString();
}

static const QString kIpVersionConvertPart = QLatin1String("::ffff:");

const QString& HostAddress::toString() const
{
    if (m_string)
        return *m_string;

    if (m_ipV4)
    {
        m_string = ipToString(*m_ipV4);
        Q_ASSERT(m_string);
        return *m_string;
    }

    Q_ASSERT(m_ipV6);
    auto string = ipToString(*m_ipV6);
    Q_ASSERT(string);

    // TODO: Remove this hack when IPv6 is properly supported!
    //  Try to map it on IPv4 as v4 format is preferable
    if (*string == QLatin1String("::"))
    {
        *string = QLatin1String("0.0.0.0");
    }
    else if (*string == QLatin1String("::1"))
    {
        *string = QLatin1String("127.0.0.1");
    }
    else if (string->startsWith(kIpVersionConvertPart))
    {
        const auto part = string->mid(kIpVersionConvertPart.length());
        m_ipV4 = ipV4from(part);
        if (m_ipV4)
        {
            m_string = part;
            return *m_string;
        }
    }

    m_string = string;
    return *m_string;
}

const boost::optional<in_addr>& HostAddress::ipV4() const
{
    if (m_ipV4)
        return m_ipV4;

    if (m_string)
    {
        m_ipV4 = ipV4from(*m_string);
        if (m_ipV4)
            return m_ipV4;
    }
    else
    {
        Q_ASSERT(m_ipV6);
        toString(); // Converts from IPv6
    }

    Q_ASSERT(m_string);
    m_ipV4 = ipV4from(*m_string);
    if (!m_ipV4)
    {
        // Try to map it on IPv4 if HostAddress was created by IPv4 string
        if (*m_string == QLatin1String("::"))
            m_ipV4 = ipV4from(QLatin1String("0.0.0.0"));

        else if (*m_string == QLatin1String("::1"))
            m_ipV4 = ipV4from(QLatin1String("127.0.0.1"));

        else if (m_string->startsWith(kIpVersionConvertPart))
            m_ipV4 = ipV4from(m_string->mid(kIpVersionConvertPart.length()));
    }

    return m_ipV4;
}

const boost::optional<in6_addr>& HostAddress::ipV6() const
{
    if (m_ipV6)
        return m_ipV6;

    if (m_string)
    {
        m_ipV6 = ipV6from(*m_string);
        if (m_ipV6)
            return m_ipV6;
    }
    else
    {
        Q_ASSERT(m_ipV4);
        m_string = ipToString(*m_ipV4);
    }

    Q_ASSERT(m_string);

    // TODO: Remove this hack when IPv6 is properly supported!
    //  Try to map it from IPv4 as v4 format is preferable
    if (*m_string == QLatin1String("0.0.0.0"))
        m_ipV6 = ipV6from(QLatin1String("::"));
    else if (*m_string == QLatin1String("127.0.0.1"))
        m_ipV6 = ipV6from(QLatin1String("::1"));
    else
        m_ipV6 = ipV6from(kIpVersionConvertPart + *m_string);

    return m_ipV6;
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

SocketAddress::SocketAddress(const QUrl& url):
    address(url.host()),
    port((quint16)url.port(0))
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

QUrl SocketAddress::toUrl(const QString& scheme) const
{
    QUrl url;
    url.setScheme(scheme.isEmpty() ? lit("http") : scheme);
    url.setHost(address.toString());
    if (port > 0)
        url.setPort(port);
    return url;
}

bool SocketAddress::isNull() const
{
    return address == HostAddress() && port == 0;
}

const SocketAddress SocketAddress::anyAddress(HostAddress::anyHost, 0);

QString SocketAddress::trimIpV6(const QString& ip)
{
    if (ip.startsWith(QLatin1Char('[')) && ip.endsWith(QLatin1Char(']')))
        return ip.mid(1, ip.length() - 2);

    return ip;
}
