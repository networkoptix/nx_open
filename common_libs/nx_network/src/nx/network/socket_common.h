#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <in6addr.h>
    #include <ws2ipdef.h>

    // Windows does not support this flag, so we emulate it
    #define MSG_DONTWAIT 0x01000000
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#include <stdint.h>

#include <QString>
#include <QUrl>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif
#include "utils/common/hash.h"


namespace nx {
namespace network {

class DnsResolver;

enum class TransportProtocol
{
    udp,
    tcp,
    udt
};

enum class NatTraversalSupport
{
    disabled,
    enabled,
};

static const size_t kUDPHeaderSize = 8;
static const size_t kIPHeaderSize = 20;
static const size_t kMaxUDPDatagramSize = 64*1024 - kUDPHeaderSize - kIPHeaderSize;
static const size_t kTypicalMtuSize = 1500;

} // network
} // nx

//!Represents ipv4 address. Supports conversion to QString and to uint32
/*!
    \note Not using QHostAddress because QHostAddress can trigger dns name lookup which depends on Qt sockets which we do not want to use
*/
class NX_NETWORK_API HostAddress
{
public:
    HostAddress(const in_addr& addr);
    HostAddress(const in6_addr& addr = in6addr_any);

    HostAddress(const QString& addrStr);
    HostAddress(const char* addrStr);

    bool operator==(const HostAddress& right) const;
    bool operator!=(const HostAddress& right) const;
    bool operator<(const HostAddress& right) const;

    /** Domain name or IP v4 (if can be converted) or IP v6 */
    const QString& toString() const;

    /** IP v4 if address is v4 or v6 which can be converted to v4 */
    boost::optional<in_addr> ipV4() const;

    /** IP v6 if address is v6 or v4 converted to v6 */
    boost::optional<in6_addr> ipV6() const;

    bool isLocal() const;
    bool isIpAddress() const;

    static const HostAddress localhost;
    static const HostAddress anyHost;

    static boost::optional<QString> ipToString(const in_addr& addr);
    static boost::optional<QString> ipToString(const in6_addr& addr);

    static boost::optional<in_addr> ipV4from(const QString& ip);
    static boost::optional<in6_addr> ipV6from(const QString& ip);

    static boost::optional<in_addr> ipV4from(const in6_addr& addr);
    static in6_addr ipV6from(const in_addr& addr);

private:
    mutable boost::optional<QString> m_string;
    boost::optional<in_addr> m_ipV4;
    boost::optional<in6_addr> m_ipV6;
};

//!Represents host and port (e.g. 127.0.0.1:1234)
class NX_NETWORK_API SocketAddress
{
public:
    HostAddress address;
    quint16 port;

    SocketAddress(const HostAddress& _address = HostAddress::anyHost, quint16 _port = 0);
    SocketAddress(const QString& str);
    SocketAddress(const QByteArray& utf8Str);
    SocketAddress(const char* utf8Str);
    SocketAddress(const QUrl& url);

    bool operator==(const SocketAddress& rhs) const;
    bool operator!=(const SocketAddress& rhs) const;
    bool operator<(const SocketAddress& rhs) const;

    QString toString() const;
    QUrl toUrl(const QString& scheme = QString()) const;
    bool isNull() const;

    static const SocketAddress anyAddress;
    static const SocketAddress anyPrivateAddress;
    static QString trimIpV6(const QString& ip);
};

inline uint qHash(const SocketAddress &address) {
    return qHash(address.address.toString(), address.port);
}

namespace std {

template <> struct hash<SocketAddress>
{
    size_t operator()(const SocketAddress& socketAddress) const
    {
        const auto stdString = socketAddress.toString().toStdString();
        return hash<std::string>{}(stdString);
    }
};

}

Q_DECLARE_METATYPE(SocketAddress)
