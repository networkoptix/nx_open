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

#include <QtCore/QtEndian>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/system_error.h>

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

enum class IpVersion
{
    v4 = AF_INET,
    v6 = AF_INET6,
};

static const size_t kUDPHeaderSize = 8;
static const size_t kIPHeaderSize = 20;
static const size_t kMaxUDPDatagramSize = 64*1024 - kUDPHeaderSize - kIPHeaderSize;
static const size_t kTypicalMtuSize = 1500;

enum InitializationFlags
{
    disableUdt = 0x01,
    disableCloudConnect = 0x02
};

NX_NETWORK_API bool socketCannotRecoverFromError(SystemError::ErrorCode sysErrorCode);

} // namespace network
} // namespace nx

/**
 * Represents ipv4 address. Supports conversion to QString and to uint32.
 * @note Not using QHostAddress because QHostAddress can trigger dns name 
 * lookup which depends on Qt sockets which we do not want to use.
 */
class NX_NETWORK_API HostAddress
{
public:
    HostAddress(const in_addr& addr);
    HostAddress(
        const in6_addr& addr = in6addr_any, 
        boost::optional<uint32_t> scopeId = boost::none);

    HostAddress(const QString& addrStr);
    HostAddress(const char* addrStr);

    ~HostAddress();

    bool operator==(const HostAddress& right) const;
    bool operator!=(const HostAddress& right) const;
    bool operator<(const HostAddress& right) const;

    /**
     * Domain name or IP v4 (if can be converted) or IP v6.
     */
    const QString& toString() const;

    /**
     * IP v4 if address is v4 or v6 which can be converted to v4.
     */
    boost::optional<in_addr> ipV4() const;

    using IpV6WithScope = std::pair<boost::optional<in6_addr>, boost::optional<uint32_t>>;
    /**
     * IP v6 if address is v6 or v4 converted to v6.
     */
    IpV6WithScope ipV6() const;
    boost::optional<uint32_t> scopeId() const;

    bool isLocal() const;
    bool isIpAddress() const;
    bool isPureIpV6() const;

    static const HostAddress localhost;
    static const HostAddress anyHost;

    static boost::optional<QString> ipToString(const in_addr& addr);
    static boost::optional<QString> ipToString(
        const in6_addr& addr, 
        boost::optional<uint32_t> scopeId);

    static boost::optional<in_addr> ipV4from(const QString& ip);
    static IpV6WithScope ipV6from(const QString& ip);

    static boost::optional<in_addr> ipV4from(const in6_addr& addr);
    static IpV6WithScope ipV6from(const in_addr& addr);

    void swap(HostAddress& other);

private:
    mutable boost::optional<QString> m_string;
    mutable boost::optional<in_addr> m_ipV4;
    mutable boost::optional<in6_addr> m_ipV6;
    mutable boost::optional<uint32_t> m_scopeId;
};

NX_NETWORK_API void swap(HostAddress& one, HostAddress& two);

Q_DECLARE_METATYPE(HostAddress)

/**
 * Represents host and port (e.g. 127.0.0.1:1234).
 */
class NX_NETWORK_API SocketAddress
{
public:
    HostAddress address;
    quint16 port;

    SocketAddress(const HostAddress& _address = HostAddress::anyHost, quint16 _port = 0);
    SocketAddress(const QString& str);
    SocketAddress(const QByteArray& utf8Str);
    SocketAddress(const char* utf8Str);
    ~SocketAddress();

    bool operator==(const SocketAddress& rhs) const;
    bool operator!=(const SocketAddress& rhs) const;
    bool operator<(const SocketAddress& rhs) const;

    QString toString() const;
    std::string toStdString() const;
    bool isNull() const;

    static const SocketAddress anyAddress;
    static const SocketAddress anyPrivateAddress;
    static QString trimIpV6(const QString& ip);
};

inline uint qHash(const SocketAddress &address)
{
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

} // namespace std

inline unsigned long long qn_htonll(unsigned long long value) { return qToBigEndian(value); }
inline unsigned long long qn_ntohll(unsigned long long value) { return qFromBigEndian(value); }

/* Note that we have to use #defines here so that these functions work even if
 * they are also defined in system network headers. */
#ifndef htonll
#define htonll qn_htonll
#endif
#ifndef ntohll
#define ntohll qn_ntohll
#endif

Q_DECLARE_METATYPE(SocketAddress)
