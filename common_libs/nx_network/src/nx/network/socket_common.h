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

#include <chrono>
#include <cstdint>
#include <string>

#include <QtCore/QtEndian>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>

#ifndef Q_MOC_RUN
#include <boost/optional.hpp>
#endif

#include <nx/utils/system_error.h>

NX_NETWORK_API bool operator==(const in_addr& left, const in_addr& right);
NX_NETWORK_API bool operator==(const in6_addr& left, const in6_addr& right);

#if !defined(_WIN32)
    NX_NETWORK_API extern const in_addr in4addr_loopback;
#endif

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
    none = 0,
    disableUdt = 0x01,
    disableCloudConnect = 0x02
};

NX_NETWORK_API bool socketCannotRecoverFromError(SystemError::ErrorCode sysErrorCode);

using IpV6WithScope = std::pair<boost::optional<in6_addr>, boost::optional<uint32_t>>;

/**
 * Represents ipv4 address. Supports conversion to QString and to uint32.
 * NOTE: Not using QHostAddress because QHostAddress can trigger dns name
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
    HostAddress(const std::string& addrStr);

    ~HostAddress();

    bool operator==(const HostAddress& right) const;
    bool operator!=(const HostAddress& right) const;
    bool operator<(const HostAddress& right) const;

    /**
     * Domain name or IP v4 (if can be converted) or IP v6.
     */
    const QString& toString() const;
    std::string toStdString() const;

    /**
     * IP v4 if address is v4 or v6 which can be converted to v4.
     */
    boost::optional<in_addr> ipV4() const;

    /**
     * IP v6 if address is v6 or v4 converted to v6.
     */
    IpV6WithScope ipV6() const;
    boost::optional<uint32_t> scopeId() const;

    bool isLocalHost() const;
    bool isLocalNetwork() const;
    bool isIpAddress() const;
    bool isPureIpV6() const;

    /**
     * @return Address similar to result of getForeignAddress function.
     * Removes string representation, making address explicit ipv4 / ipv6.
     */
    HostAddress toPureIpAddress(int desiredIpVersion) const;

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
    boost::optional<in_addr> m_ipV4;
    boost::optional<in6_addr> m_ipV6;
    boost::optional<uint32_t> m_scopeId;

    HostAddress(
        boost::optional<QString> addressString,
        boost::optional<in_addr> ipV4,
        boost::optional<in6_addr> ipV6);
};

NX_NETWORK_API void swap(HostAddress& one, HostAddress& two);

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
    SocketAddress(const std::string& str);
    SocketAddress(const char* utf8Str);
    SocketAddress(const sockaddr_in& ipv4Endpoint);
    SocketAddress(const sockaddr_in6& ipv6Endpoint);
    ~SocketAddress();

    bool operator==(const SocketAddress& rhs) const;
    bool operator!=(const SocketAddress& rhs) const;
    bool operator<(const SocketAddress& rhs) const;

    QString toString() const;
    std::string toStdString() const;
    bool isNull() const;

    static const SocketAddress anyAddress;
    static const SocketAddress anyPrivateAddress;
    static const SocketAddress anyPrivateAddressV4;
    static const SocketAddress anyPrivateAddressV6;
    static QString trimIpV6(const QString& ip);
};

inline uint qHash(const SocketAddress &address)
{
    return qHash(address.address.toString(), address.port);
}

struct NX_NETWORK_API KeepAliveOptions
{
    std::chrono::seconds inactivityPeriodBeforeFirstProbe;
    std::chrono::seconds probeSendPeriod;
    /**
     * The number of unacknowledged probes to send before considering the connection dead and
     * notifying the application layer.
     */
    size_t probeCount;

    KeepAliveOptions(
        std::chrono::seconds inactivityPeriodBeforeFirstProbe = std::chrono::seconds::zero(),
        std::chrono::seconds probeSendPeriod = std::chrono::seconds::zero(),
        size_t probeCount = 0);

    bool operator==(const KeepAliveOptions& rhs) const;

    /** Maximum time before lost connection can be acknowledged. */
    std::chrono::seconds maxDelay() const;
    QString toString() const;

    void resetUnsupportedFieldsToSystemDefault();

    static boost::optional<KeepAliveOptions> fromString(const QString& string);
};

} // namespace network
} // namespace nx

Q_DECLARE_METATYPE(nx::network::HostAddress)
Q_DECLARE_METATYPE(nx::network::SocketAddress)

namespace std {

template <> struct hash<nx::network::SocketAddress>
{
    size_t operator()(const nx::network::SocketAddress& socketAddress) const
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
