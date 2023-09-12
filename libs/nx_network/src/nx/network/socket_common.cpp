// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "socket_common.h"

#include <cstring>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <nx/utils/timer_manager.h>
#include <nx/utils/url.h>

#include "socket_global.h"

bool operator==(const in_addr& left, const in_addr& right)
{
    return memcmp(&left, &right, sizeof(left)) == 0;
}

bool operator==(const in6_addr& left, const in6_addr& right)
{
    return memcmp(&left, &right, sizeof(left)) == 0;
}

#if !defined(_WIN32)
    #if defined(__arm__) && !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
        #undef htonl
    #endif

    // NOTE: calcLoopbackAddr() fixes a compile error on clang in the Release mode.
    auto calcLoopbackAddr() { return htonl(INADDR_LOOPBACK); }
    const in_addr in4addr_loopback{ calcLoopbackAddr() };
#endif

namespace nx::network {

bool socketCannotRecoverFromError(SystemError::ErrorCode sysErrorCode)
{
    return sysErrorCode != SystemError::noError
        && sysErrorCode != SystemError::wouldBlock
        && sysErrorCode != SystemError::again
        && sysErrorCode != SystemError::timedOut
        && sysErrorCode != SystemError::interrupted
        && sysErrorCode != SystemError::inProgress;
}

//-------------------------------------------------------------------------------------------------
// HostAddress

HostAddress::HostAddress(
    std::optional<std::string_view> addressString,
    std::optional<in_addr> ipV4,
    std::optional<in6_addr> ipV6)
    :
    m_string(std::move(addressString)),
    m_ipV4(std::move(ipV4)),
    m_ipV6(std::move(ipV6))
{
}

const HostAddress HostAddress::localhost(
    std::string_view("localhost"), in4addr_loopback, in6addr_loopback);

const HostAddress HostAddress::anyHost(*ipV4from("0.0.0.0"));

HostAddress::HostAddress(const in_addr& addr):
    m_ipV4(addr)
{
}

HostAddress::HostAddress(const in6_addr& addr, std::optional<uint32_t> scopeId):
    m_ipV6(addr),
    m_scopeId(scopeId)
{
}

HostAddress::HostAddress(const std::string_view& host): m_string(host)
{
    if (!host.empty())
    {
        NX_ASSERT_HEAVY_CONDITION(
            nx::utils::Url::isValidHost(host), "Invalid host address: [%1]", host);
    }
}

HostAddress::HostAddress(const char* addrStr):
    HostAddress(std::string_view(addrStr))
{
}

HostAddress::HostAddress(const QString& str):
    HostAddress(str.toStdString())
{
}

HostAddress::HostAddress(const QHostAddress& host):
    HostAddress(host.toString().toStdString())
{
}

HostAddress::~HostAddress() = default;

bool HostAddress::operator==(const HostAddress& rhs) const
{
    return isIpAddress() == rhs.isIpAddress()
        && toString() == rhs.toString()
        && m_scopeId == rhs.m_scopeId;
}

bool HostAddress::operator!=(const HostAddress& rhs) const
{
    return !(*this == rhs);
}

bool HostAddress::operator<(const HostAddress& rhs) const
{
    if (isIpAddress() != rhs.isIpAddress())
        return isIpAddress();

    if (toString() < rhs.toString())
        return true;
    else if (rhs.toString() < toString())
        return false;

    if (!(bool) m_scopeId && !(bool) rhs.m_scopeId)
        return false;

    if ((bool) m_scopeId && !(bool) rhs.m_scopeId)
        return false;

    if ((bool) rhs.m_scopeId && !(bool) m_scopeId)
        return true;

    return *m_scopeId < *rhs.m_scopeId;
}

static const auto kIpVersionMapPrefix =
    nx::utils::fromHex(std::string("00000000000000000000FFFF"));

const std::string& HostAddress::toString() const
{
    if (m_string)
        return *m_string;

    // FIXME: Remove this code when IPv6 is properly supported.
    //  Try to map it from IPv4 as v4 format is preferable
    auto ipV4 = m_ipV4;
    if (!ipV4)
        ipV4 = ipV4from(*m_ipV6);

    if (ipV4)
        m_string = ipToString(*ipV4);
    else
        m_string = ipToString(*m_ipV6, m_scopeId);

    return *m_string;
}

std::optional<in_addr> HostAddress::ipV4() const
{
    if (m_ipV4)
        return m_ipV4;

    // FIXME: Remove this code when IPv6 is properly supported.
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
        if (ipV6.first)
        {
            if (const auto ipV4 = ipV4from(*ipV6.first))
                return ipV4;
        }
    }

    return std::nullopt;
}

IpV6WithScope HostAddress::ipV6() const
{
    if (m_ipV6)
        return IpV6WithScope(m_ipV6, m_scopeId);

    if (m_ipV4)
        return ipV6from(*m_ipV4);

    const auto ipV6 = ipV6from(*m_string);
    if (ipV6.first)
        return ipV6;

    if (const auto ipV4 = ipV4from(*m_string))
        return ipV6from(*ipV4);

    return IpV6WithScope();
}

bool HostAddress::isLoopback() const
{
    if (m_string == localhost.m_string)
        return true;

    if (const auto& ip = ipV4())
    {
        const auto addr = ntohl(ip->s_addr);
        return addr >= 0x7F000000 && addr < 0x80000000;
    }

    if (const auto ip = ipV6().first) //< Note the absence of `&` to work around an MSVC bug.
    {
        // NOTE: IPv6 assigns only ::1/128 for loopback.
        return std::memcmp(&*ip, &in6addr_loopback, sizeof(*ip)) == 0;
    }

    return false; // not even IP address
}

bool HostAddress::isLocalNetwork() const
{
    if (const auto& ip = ipV4())
    {
        // TODO: Incorrect check for localhost! IPv4 should be 127.0.0.0/8, should call isLocalHost.
        const auto addr = ntohl(ip->s_addr);
        return (addr == 0x7F000001) // 127.0.0.1
            || (addr >= 0x0A000000 && addr <= 0x0AFFFFFF) // 10.*.*.*
            || (addr >= 0xAC100000 && addr <= 0xAC1FFFFF) // 172.16.*.* - 172.31.*.*
            || (addr >= 0xC0A80000 && addr <= 0xC0A8FFFF); // 192.168.0.0
    }

    if (const auto ip = ipV6().first) //< Note the absence of `&` to work around an MSVC bug.
    {
        return (std::memcmp(&*ip, &in6addr_loopback, sizeof(*ip)) == 0) // ::1
            || (ip->s6_addr[0] == 0xFD && ip->s6_addr[1] == 0x00) // FD00:*
            || (ip->s6_addr[0] == 0xFE && ip->s6_addr[1] == 0x80); // FE00:*
    }

    return false; // not even IP address
}

bool HostAddress::isIpv4LinkLocalNetwork() const
{
    if (const auto& ip = ipV4())
    {
        const auto addr = ntohl(ip->s_addr);
        return (addr & 0xFFFF0000) == 0xA9FE0000; // 169.254.0.0
    }
    return false;
}

bool HostAddress::isIpAddress() const
{
    if (m_ipV4 || m_ipV6)
        return true;

    return false;
}

bool HostAddress::isPureIpV6() const
{
    return (bool) ipV6().first && !(bool) ipV4();
}

bool HostAddress::isMulticast() const
{
    // IPv4: [224.0.0.0; 239.255.255.255]. I.e., [0xE0000000; 0xF0000000).
    // IPv6: ff00::/8.

    if (auto ipv4Addr = ipV4())
        return (ntohl(ipv4Addr->s_addr) & 0xF0000000) == 0xE0000000;

    if (auto ipv6Addr = ipV6(); ipv6Addr.first)
        return ipv6Addr.first->s6_addr[0] == 0xff;

    return false;
}

bool HostAddress::isEmpty() const
{
    return !m_ipV4 && !m_ipV6 && !(m_string && !m_string->empty());
}

HostAddress HostAddress::toPureIpAddress(int desiredIpVersion) const
{
    if (desiredIpVersion == AF_INET6)
    {
        const auto ipv6WithScope = ipV6();
        if (ipv6WithScope.first)
            return HostAddress(*ipv6WithScope.first, ipv6WithScope.second);
    }
    else
    {
        if (ipV4())
            return HostAddress(*ipV4());
    }

    return HostAddress();
}

std::optional<uint32_t> HostAddress::scopeId() const
{
    return m_scopeId;
}

std::optional<std::string> HostAddress::ipToString(const in_addr& addr)
{
    char buffer[1024];
    if (inet_ntop(AF_INET, (void*)&addr, buffer, sizeof(buffer)))
        return std::string(buffer);

    return std::nullopt;
}

std::optional<std::string> HostAddress::ipToString(
    const in6_addr& addr,
    std::optional<uint32_t> scopeId)
{
    char buffer[1024];
    std::string result;

    if (inet_ntop(AF_INET6, (void*)&addr, buffer, sizeof(buffer)))
        result = std::string(buffer);
    else
        return std::nullopt;

    if (scopeId && *scopeId != 0)
        nx::utils::buildString(&result, '%', *scopeId);

    return result;
}

in_addr HostAddress::ipV4from(const uint32_t& ip)
{
    in_addr v4;
    v4.s_addr = htonl(ip);
    return v4;
}

std::optional<in_addr> HostAddress::ipV4from(const std::string_view& str)
{
    static constexpr auto maxIpv4StrSize = sizeof("ddd.ddd.ddd.ddd") - 1;

    if (str.size() > maxIpv4StrSize)
        return std::nullopt;

    // NOTE: str.data() is not NULL-terminated.

    char buf[maxIpv4StrSize + 1];
    memcpy(buf, str.data(), str.size());
    buf[str.size()] = '\0';

    in_addr v4;
    if (inet_pton(AF_INET, buf, &v4))
        return v4;

    return std::nullopt;
}

IpV6WithScope HostAddress::ipV6from(const std::string_view& str)
{
    static constexpr auto maxIpv6AddressStrSize = 16 * 2 + 7;

    const auto [tokens, count] = nx::utils::split_n<2>(
        str, '%', nx::utils::GroupToken::none, nx::utils::SplitterFlag::noFlags);

    if (count == 0)
        return IpV6WithScope();

    const auto ipStr = tokens[0];
    std::optional<uint32_t> scopeId;

    const auto isScopeIdPresent = count > 1;
    if (isScopeIdPresent)
    {
        NX_ASSERT(!tokens[1].empty());
        if (tokens[1].empty())
            return IpV6WithScope();

        std::size_t ok = 0;
        scopeId = nx::utils::stoul(tokens[1], &ok);
        if (!ok)
            return IpV6WithScope();
    }

    if (ipStr.size() > maxIpv6AddressStrSize)
        return IpV6WithScope();

    char buf[maxIpv6AddressStrSize + 1];
    memcpy(buf, ipStr.data(), ipStr.size());
    buf[ipStr.size()] = '\0';

    struct in6_addr addr6;
    memset(&addr6, 0, sizeof(addr6));
    if (!inet_pton(AF_INET6, buf, &addr6))
        return IpV6WithScope();

    return IpV6WithScope{addr6, scopeId};
}

std::optional<in_addr> HostAddress::ipV4from(const in6_addr& v6)
{
    in_addr v4;

    // FIXME: Remove this code when IPv6 is properly supported.
    //  Try to map it from IPv4 as v4 format is preferable
    if (std::memcmp(&v6, &in6addr_any, sizeof(v6)) == 0)
    {
        v4.s_addr = htonl(INADDR_ANY);
        return v4;
    }

    if (std::memcmp(kIpVersionMapPrefix.data(), &v6.s6_addr[0], kIpVersionMapPrefix.size()) != 0)
        return std::nullopt;

    std::memcpy(&v4, &v6.s6_addr[kIpVersionMapPrefix.size()], sizeof(v4));
    return v4;
}

IpV6WithScope HostAddress::ipV6from(const in_addr& v4)
{
    // FIXME: Remove this code when IPv6 is properly supported.
    //  Try to map it from IPv4 as v4 format is preferable
    if (v4.s_addr == htonl(INADDR_ANY))
        return IpV6WithScope(in6addr_any, std::nullopt);

    in6_addr v6;
    std::memcpy(&v6.s6_addr[0], kIpVersionMapPrefix.data(), kIpVersionMapPrefix.size());
    std::memcpy(&v6.s6_addr[kIpVersionMapPrefix.size()], &v4, sizeof(v4));

    return IpV6WithScope(v6, std::nullopt);
}

void HostAddress::swap(HostAddress& other)
{
    m_string.swap(other.m_string);
    m_ipV4.swap(other.m_ipV4);
    m_ipV6.swap(other.m_ipV6);
}

HostAddress HostAddress::fromString(const std::string_view& host)
{
    std::optional<std::string_view> addressString;
    std::optional<in6_addr> ipV6;
    if (nx::utils::Url::isValidHost(host))
        addressString = host;
    else
        ipV6 = in6addr_any;
    std::optional<in_addr> ipV4;
    return HostAddress(std::move(addressString), std::move(ipV4), std::move(ipV6));
}

void swap(HostAddress& one, HostAddress& two)
{
    one.swap(two);
}

void PrintTo(const HostAddress& val, ::std::ostream* os)
{
    *os << val.toString();
}

//-------------------------------------------------------------------------------------------------
// SocketAddress

SocketAddress::SocketAddress(const HostAddress& address, quint16 port):
    address(address),
    port(port)
{
}

SocketAddress::SocketAddress(const std::string_view& str)
{
    const auto [hostStr, parsedPort] = split(str);
    address = HostAddress(hostStr);
    if (parsedPort)
        port = *parsedPort;

    NX_ASSERT_HEAVY_CONDITION(!toString().empty());
}

SocketAddress::SocketAddress(const char* endpointStr):
    SocketAddress(std::string_view(endpointStr))
{
}

SocketAddress::SocketAddress(const sockaddr_in& ipv4Endpoint):
    address(ipv4Endpoint.sin_addr),
    port(ntohs(ipv4Endpoint.sin_port))
{
}

SocketAddress::SocketAddress(const sockaddr_in6& ipv6Endpoint):
    address(ipv6Endpoint.sin6_addr, ipv6Endpoint.sin6_scope_id),
    port(ntohs(ipv6Endpoint.sin6_port))
{
}

SocketAddress::~SocketAddress() = default;

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

std::string SocketAddress::toString() const
{
    const auto& hostStr = address.toString();

    std::string result;
    //            '['    host          ']' ':'  port
    result.reserve(1 + hostStr.size() + 1 + 1 + 5);

    if (hostStr.find(':') != hostStr.npos) //< ipv6?
        nx::utils::buildString(&result, '[', hostStr, ']');
    else
        result += hostStr;

    if (port > 0)
        nx::utils::buildString(&result, ':', port);

    return result;
}

bool SocketAddress::isNull() const
{
    return address == HostAddress() && port == 0;
}

const SocketAddress SocketAddress::anyAddress(HostAddress::anyHost, 0);

const SocketAddress SocketAddress::anyPrivateAddress(
    HostAddress::localhost, 0);

const SocketAddress SocketAddress::anyPrivateAddressV4(
    HostAddress::localhost.toPureIpAddress(AF_INET), 0);

const SocketAddress SocketAddress::anyPrivateAddressV6(
    HostAddress::localhost.toPureIpAddress(AF_INET6), 0);

std::string_view SocketAddress::trimIpV6(const std::string_view& ip)
{
    if (nx::utils::startsWith(ip, '[') && nx::utils::endsWith(ip, ']'))
        return ip.substr(1, ip.length() - 2);

    return ip;
}

SocketAddress SocketAddress::fromString(const std::string_view& str)
{
    return SocketAddress(str);
}

SocketAddress SocketAddress::fromUrl(const nx::utils::Url& url)
{
    if (!url.isValid() || url.isLocalFile())
        return {};

    return {url.host().toStdString(), quint16(url.port(0))};
}

std::pair<std::string_view, std::optional<int>> SocketAddress::split(
    const std::string_view& str)
{
    const auto [tokens, count] = nx::utils::split_n<3>(
        str, ':',
        nx::utils::GroupToken::squareBrackets, nx::utils::SplitterFlag::noFlags);
    if (count > 2) // IPv6 address with no brackets?
        return {str, std::nullopt};

    std::pair<std::string_view, std::optional<int>> result;
    if (count >= 1)
        result.first = trimIpV6(tokens[0]);
    if (count >= 2)
        result.second = nx::utils::stoi(tokens[1]);

    return result;
}

std::pair<std::string_view, std::optional<int>> SocketAddress::split(const std::string& str)
{
    const std::string_view view(str.data(), str.size());
    return split(view);
}

void PrintTo(const SocketAddress& val, ::std::ostream* os)
{
    *os << val.toString();
}

void serialize(QnJsonContext*, const SocketAddress& value, QJsonValue* target)
{
    QJsonObject serialized;
    serialized.insert("address", QString::fromStdString(value.address.toString()));
    serialized.insert("port", value.port);
    *target = serialized;
}

bool deserialize(QnJsonContext*, const QJsonValue& source, SocketAddress* value)
{
    if (!source.isObject())
        return false;

    const auto obj = source.toObject();
    value->address = HostAddress(obj.value("address").toString().toStdString());
    value->port = obj.value("port").toInt();

    return true;
}

//-------------------------------------------------------------------------------------------------

KeepAliveOptions::KeepAliveOptions(
    std::chrono::milliseconds inactivityPeriodBeforeFirstProbe,
    std::chrono::milliseconds probeSendPeriod,
    int probeCount)
    :
    inactivityPeriodBeforeFirstProbe(inactivityPeriodBeforeFirstProbe),
    probeSendPeriod(probeSendPeriod),
    probeCount(probeCount)
{
}

bool KeepAliveOptions::operator==(const KeepAliveOptions& rhs) const
{
    return inactivityPeriodBeforeFirstProbe == rhs.inactivityPeriodBeforeFirstProbe
        && probeSendPeriod == rhs.probeSendPeriod
        && probeCount == rhs.probeCount;
}

bool KeepAliveOptions::operator!=(const KeepAliveOptions& rhs) const
{
    return !(*this == rhs);
}

std::chrono::milliseconds KeepAliveOptions::maxDelay() const
{
    return inactivityPeriodBeforeFirstProbe + probeSendPeriod * probeCount;
}

std::string KeepAliveOptions::toString() const
{
    using namespace std::chrono;

    static constexpr int kMillisPerSec = 1000;

    // Preserving existing behavior: if seconds are stored, then not adding a suffix.
    // TODO: #akolesnikov Rewrite this method using nx::utils::buildString

    QString result;
    result += "{";
    if (inactivityPeriodBeforeFirstProbe.count() % kMillisPerSec == 0)
        result += QString::number(duration_cast<seconds>(inactivityPeriodBeforeFirstProbe).count());
    else
        result += QString::number(inactivityPeriodBeforeFirstProbe.count()) + "ms";
    result += ",";
    if (probeSendPeriod.count() % kMillisPerSec == 0)
        result += QString::number(duration_cast<seconds>(probeSendPeriod).count());
    else
        result += QString::number(probeSendPeriod.count()) + "ms";
    result += ",";
    result += QString::number(probeCount);
    result += "}";

    return result.toStdString();
}

void KeepAliveOptions::resetUnsupportedFieldsToSystemDefault()
{
    #if defined(_WIN32)
        probeCount = 10;
    #elif defined(__APPLE__)
        probeSendPeriod = std::chrono::seconds::zero();
        probeCount = 0;
    #endif // _WIN32
}

std::optional<KeepAliveOptions> KeepAliveOptions::fromString(
    std::string_view str)
{
    str = nx::utils::trim(str, "{}");
    const auto [tokens, count] = nx::utils::split_n<3>(str, ',');
    if (count != 3)
        return std::nullopt;

    KeepAliveOptions options;
    options.inactivityPeriodBeforeFirstProbe =
        nx::utils::parseTimerDuration(nx::utils::trim(tokens[0]));
    options.probeSendPeriod =
        nx::utils::parseTimerDuration(nx::utils::trim(tokens[1]));
    options.probeCount = nx::utils::stoi(nx::utils::trim(tokens[2]));
    return options;
}

void PrintTo(const KeepAliveOptions& val, ::std::ostream* os)
{
    *os << val.toString();
}

} // namespace nx::network
