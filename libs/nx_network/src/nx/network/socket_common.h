// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nx/reflect/instrument.h>
#include <nx/reflect/tags.h>
#include <nx/utils/system_error.h>
#include <nx/utils/type_utils.h>

// TODO: #skolesnik Move all related conversion functions to separate header.
// very error prone
// For definitions use `#include <nx/utils/system_network_headers.h>`
struct in_addr;
struct in6_addr;
struct sockaddr_in;
struct sockaddr_in6;

class QHostAddress;
class QString;

// Needed for compatibility with nx_fusion.
// Luckily, custom nx_fusion serialization support does not require dependency on nx_fusion itself.
class QJsonValue;
class QnJsonContext;

namespace nx { class Url; }

namespace nx::network {

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

using IpV6WithScope = std::pair<std::optional<in6_addr>, std::optional<uint32_t>>;

//-------------------------------------------------------------------------------------------------

/**
 * Represents ipv4 address. Supports conversion to std::string and to uint32.
 * NOTE: Not using QHostAddress because QHostAddress can trigger dns name
 * lookup which depends on Qt sockets which we do not want to use.
 */
class NX_NETWORK_API HostAddress
{
public:
    HostAddress();
    HostAddress(const in_addr& v4Address);
    HostAddress(const in6_addr& v6Address, std::optional<uint32_t> scopeId = std::nullopt);
    HostAddress(const std::string_view& addrStr);

    // Construct from string literal
    template<size_t N>
    HostAddress(const char (&addrStr)[N]): HostAddress(std::string_view(addrStr))
    {
    }

    // TODO: #skolesnik Remove these
    HostAddress(const QString& str);
    HostAddress(const QHostAddress& host);

    /**
     * NOTE: This constructor works with any type that has "const char* data()" and "size()".
     * E.g., QByteArray.
     */
    template<typename String>
    HostAddress(
        const String& str,
        std::enable_if_t<nx::utils::IsConvertibleToStringViewV<String>>* = nullptr)
        :
        HostAddress(std::string_view(str.data(), (std::size_t) str.size()))
    {
    }

    HostAddress(const HostAddress& other);
    HostAddress(HostAddress&& other) noexcept;
    HostAddress& operator=(const HostAddress& other);
    HostAddress& operator=(HostAddress&& other) noexcept;

    ~HostAddress();

    bool operator==(const HostAddress& right) const;
    bool operator!=(const HostAddress& right) const;
    bool operator<(const HostAddress& right) const;

    /**
     * Domain name or IP v4 (if can be converted) or IP v6.
     */
    const std::string& toString() const;

    /**
     * IP v4 if address is v4 or v6 which can be converted to v4.
     */
    std::optional<in_addr> ipV4() const;

    /**
     * IP v6 if address is v6 or v4 converted to v6.
     */
    IpV6WithScope ipV6() const;
    std::optional<uint32_t> scopeId() const;

    /**
     * @return true if "localhost", 127.0.0.1/8 or ::1/128.
     */
    bool isLoopback() const;

    bool isLocalNetwork() const;
    bool isIpv4LinkLocalNetwork() const;

    /**
     * @return true if object was constructed using the ipv4 or ipv6 constructor. Returns false
     * if the object was constructed via a string.
     */
    bool isIpAddress() const;
    bool isPureIpV6() const;

    bool isMulticast() const;

    bool isEmpty() const;

    /**
     * @return Address similar to result of getForeignAddress function.
     * Removes string representation, making address explicit ipv4 / ipv6.
     */
    HostAddress toPureIpAddress(int desiredIpVersion) const;

    static const HostAddress localhost;
    static const HostAddress anyHost;

    static std::optional<std::string> ipToString(const in_addr& addr);
    static std::optional<std::string> ipToString(
        const in6_addr& addr,
        std::optional<uint32_t> scopeId);

    static in_addr ipV4from(const uint32_t& ip);
    static std::optional<in_addr> ipV4from(const std::string_view& ip);
    static IpV6WithScope ipV6from(const std::string_view& ip);

    static std::optional<in_addr> ipV4from(const in6_addr& addr);
    static IpV6WithScope ipV6from(const in_addr& addr);

    static HostAddress fromString(const std::string_view& host);

    void swap(HostAddress& other);

private:
    // Type-erasing adapter with the same size and alignment as `in_addr`
    using InAddr = std::uint32_t;

    #ifdef _MSC_VER
        #pragma warning(push)
        #pragma warning(disable:4324) //< 'structure was padded due to alignment specifier'
    #endif

    // `in6_addr` differs in alignment between Windows and POSIX
    #if defined(_WIN32)
        struct alignas(2) In6Addr { std::uint8_t value[16]; };
    #else
        struct alignas(4) In6Addr { std::uint8_t value[16]; };
    #endif

    mutable std::optional<std::string> m_string;
    std::optional<InAddr> m_ipV4;
    std::optional<In6Addr> m_ipV6;
    std::optional<uint32_t> m_scopeId;

    #ifdef _MSC_VER
        #pragma warning(pop)
    #endif

    HostAddress(
        std::optional<std::string_view> addressString,
        std::optional<in_addr> ipV4,
        std::optional<in6_addr> ipV6);
};

NX_REFLECTION_TAG_TYPE(HostAddress, useStringConversionForSerialization)

NX_NETWORK_API void swap(HostAddress& one, HostAddress& two);
NX_NETWORK_API void PrintTo(const HostAddress& val, ::std::ostream* os);

//-------------------------------------------------------------------------------------------------

static constexpr std::uint16_t kAnyPort = 0;

/**
 * Represents host and port (e.g. 127.0.0.1:1234).
 */
class NX_NETWORK_API SocketAddress
{
public:
    HostAddress address = HostAddress::anyHost;
    std::uint16_t port = kAnyPort;

    SocketAddress() = default;
    SocketAddress(const SocketAddress&) = default;

    SocketAddress(SocketAddress&&) = default;

    SocketAddress(const HostAddress& address, std::uint16_t port);
    SocketAddress(std::string_view endpoint);

    // From string literal
    template<size_t N>
    SocketAddress(const char (&endpoint)[N]):
        SocketAddress(std::string_view(endpoint))
    {
    }

    /**
     * NOTE: This constructor works with any type that has "const char* data()" and "size()".
     * E.g., QByteArray.
     */
    template<typename String>
    SocketAddress(
        const String& endpointStr,
        std::enable_if_t<nx::utils::IsConvertibleToStringViewV<String>>* = nullptr)
        :
        SocketAddress(std::string_view(endpointStr.data(), (std::size_t) endpointStr.size()))
    {
    }

    SocketAddress(const sockaddr_in& ipv4Endpoint);
    SocketAddress(const sockaddr_in6& ipv6Endpoint);
    ~SocketAddress();

    SocketAddress& operator=(const SocketAddress&) = default;
    SocketAddress& operator=(SocketAddress&&) = default;
    bool operator==(const SocketAddress& rhs) const;
    bool operator!=(const SocketAddress& rhs) const;
    bool operator<(const SocketAddress& rhs) const;

    std::string toString() const;
    bool isNull() const;

    static const SocketAddress anyAddress;
    static const SocketAddress anyPrivateAddress;
    static const SocketAddress anyPrivateAddressV4;
    static const SocketAddress anyPrivateAddressV6;

    static std::string_view trimIpV6(const std::string_view& str);

    static SocketAddress fromString(const std::string_view& str);
    static SocketAddress fromUrl(const nx::Url& url, bool useDefaultPortFromScheme = false);

    /**
     * Split host:port string to host and port. Port is optional.
     * host may be a domain name or a IPv4/IPv6 address.
     * Ipv6 address MUST be enclosed in square brackets if port is specified.
     * E.g., [ipv6]:port.
     * @return pair<host, port (if present)>.
     */
    static std::pair<std::string_view, std::optional<int>> split(const std::string_view& str);
    static std::pair<std::string_view, std::optional<int>> split(std::string_view&& str) = delete;

    static std::pair<std::string_view, std::optional<int>> split(const std::string& str);
    static std::pair<std::string_view, std::optional<int>> split(std::string&& str) = delete;
};

NX_NETWORK_API void PrintTo(const SocketAddress& val, ::std::ostream* os);

NX_REFLECTION_INSTRUMENT(SocketAddress, (address)(port))

NX_NETWORK_API void serialize(QnJsonContext*, const nx::network::SocketAddress& value, QJsonValue* target);
NX_NETWORK_API bool deserialize(QnJsonContext*, const QJsonValue& source, SocketAddress* value);

//-------------------------------------------------------------------------------------------------

struct NX_NETWORK_API KeepAliveOptions
{
    std::chrono::milliseconds inactivityPeriodBeforeFirstProbe = std::chrono::milliseconds::zero();
    std::chrono::milliseconds probeSendPeriod = std::chrono::milliseconds::zero();

    /**
     * The number of unacknowledged probes to send before considering the connection dead and
     * notifying the application layer.
     * The probe is considered unacknowledged if there was no response to it during the
     * probeSendPeriod period.
     */
    int probeCount = 0;

    KeepAliveOptions(
        std::chrono::milliseconds inactivityPeriodBeforeFirstProbe = std::chrono::milliseconds::zero(),
        std::chrono::milliseconds probeSendPeriod = std::chrono::milliseconds::zero(),
        int probeCount = 0);

    bool operator==(const KeepAliveOptions& rhs) const;
    bool operator!=(const KeepAliveOptions& rhs) const;

    /** Maximum time before lost connection can be acknowledged. */
    std::chrono::milliseconds maxDelay() const;
    std::string toString() const;

    void resetUnsupportedFieldsToSystemDefault();

    /**
     * @return std::nullopt in case of a parse error.
     */
    static std::optional<KeepAliveOptions> fromString(std::string_view str);
};

NX_NETWORK_API void PrintTo(const KeepAliveOptions& val, ::std::ostream* os);
NX_REFLECTION_TAG_TYPE(KeepAliveOptions, useStringConversionForSerialization)

} // namespace nx::network

namespace std {

template <> struct hash<nx::network::SocketAddress>
{
    size_t operator()(const nx::network::SocketAddress& socketAddress) const
    {
        const auto stdString = socketAddress.toString();
        return hash<std::string>{}(stdString);
    }
};

} // namespace std
