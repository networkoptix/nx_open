#include "socket_address.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include <cstring>
#include <algorithm>

namespace detail {

SocketAddress::SocketAddress()
{
    memset(&m_address, 0, sizeof(m_address));
    setFamily(AF_INET);
}

static constexpr socklen_t clamp_len(socklen_t v, socklen_t cap) {
#ifndef __GNUC__
    if constexpr (std::is_signed_v<socklen_t>)
        if (v < 0) return 0;
#endif
    return (v <= cap) ? v : cap;
}

SocketAddress::SocketAddress(const sockaddr_storage* addr, socklen_t addrLen)
{
    const socklen_t cap = static_cast<socklen_t>(sizeof(m_address));
    m_length = clamp_len(addrLen, cap);
    memcpy(&m_address, addr, m_length);
}

SocketAddress::SocketAddress(const sockaddr* addr, socklen_t addrLen)
{
    const socklen_t cap = static_cast<socklen_t>(sizeof(m_address));
    m_length = clamp_len(addrLen, cap);
    memcpy(&m_address, addr, m_length);
}

struct sockaddr* SocketAddress::get()
{
    return (struct sockaddr*) &m_address;
}

const struct sockaddr* SocketAddress::get() const
{
    return (const struct sockaddr*) &m_address;
}

socklen_t SocketAddress::size() const
{
    return m_length;
}

int SocketAddress::family() const
{
    return m_address.ss_family;
}

void SocketAddress::setFamily(int val)
{
    m_address.ss_family = val;
    m_length = val == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
}

unsigned short SocketAddress::port() const
{
    return m_address.ss_family == AF_INET
        ? ntohs(v4()->sin_port)
        : ntohs(v6()->sin6_port);
}

void SocketAddress::copy(sockaddr* name, int* namelen) const
{
    const auto bytesToCopy = std::min<int>(*namelen,
        family() == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6));
    memcpy(name, &m_address, bytesToCopy);
    *namelen = bytesToCopy;
}

void SocketAddress::copy(uint32_t ip[]) const
{
    if (family() == AF_INET)
    {
        ip[0] = v4()->sin_addr.s_addr;
    }
    else
    {
        const auto a = v6();
        ip[3] = (a->sin6_addr.s6_addr[15] << 24) + (a->sin6_addr.s6_addr[14] << 16) + (a->sin6_addr.s6_addr[13] << 8) + a->sin6_addr.s6_addr[12];
        ip[2] = (a->sin6_addr.s6_addr[11] << 24) + (a->sin6_addr.s6_addr[10] << 16) + (a->sin6_addr.s6_addr[9] << 8) + a->sin6_addr.s6_addr[8];
        ip[1] = (a->sin6_addr.s6_addr[7] << 24) + (a->sin6_addr.s6_addr[6] << 16) + (a->sin6_addr.s6_addr[5] << 8) + a->sin6_addr.s6_addr[4];
        ip[0] = (a->sin6_addr.s6_addr[3] << 24) + (a->sin6_addr.s6_addr[2] << 16) + (a->sin6_addr.s6_addr[1] << 8) + a->sin6_addr.s6_addr[0];
    }
}

sockaddr_in* SocketAddress::v4()
{
    return (sockaddr_in*) &m_address;
}

const sockaddr_in* SocketAddress::v4() const
{
    return (const sockaddr_in*) &m_address;
}

sockaddr_in6* SocketAddress::v6()
{
    return (sockaddr_in6*) &m_address;
}

const sockaddr_in6* SocketAddress::v6() const
{
    return (const sockaddr_in6*) &m_address;
}

bool SocketAddress::operator==(const SocketAddress& right) const
{
    if (family() != right.family())
        return false;

    if (family() == AF_INET)
    {
        return v4()->sin_addr.s_addr == right.v4()->sin_addr.s_addr
            && v4()->sin_port == right.v4()->sin_port;
    }
    else
    {
        return memcmp(&v6()->sin6_addr, &right.v6()->sin6_addr, sizeof(in6_addr)) == 0
            && v6()->sin6_port == right.v6()->sin6_port;
    }
}

std::string SocketAddress::toString() const
{
    char buffer[1024];

    switch (family())
    {
        case AF_INET:
            if (inet_ntop(AF_INET, (void*)&v4()->sin_addr, buffer, sizeof(buffer)))
                return std::string(buffer) + ":" + std::to_string(v4()->sin_port);
            return "invalid";

        case AF_INET6:
            if (inet_ntop(AF_INET6, (void*)&v6()->sin6_addr, buffer, sizeof(buffer)))
                return std::string(buffer) + ":" + std::to_string(v6()->sin6_port);
            return "invalid";

        default:
            return std::string("Invalid address family ") + std::to_string(family());
    }
}

} // namespace detail
