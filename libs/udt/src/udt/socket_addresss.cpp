#include "socket_addresss.h"

#include <cstring>
#include <algorithm>

namespace detail {

SocketAddress::SocketAddress(int family):
    m_length(family == AF_INET ? sizeof(m_address.v4) : sizeof(m_address.v6))
{
    memset(&m_address.v6, 0, sizeof(m_address.v6));
    m_address.v6.sin6_family = family;
}

SocketAddress::SocketAddress(const sockaddr* addr, int addrLen):
    m_length(addrLen)
{
    memset(&m_address.v6, 0, sizeof(m_address.v6));

    if (addr->sa_family == AF_INET)
    {
        m_address.v4 = *((sockaddr_in*)addr);
        m_length = sizeof(m_address.v4);
    }
    else if (addr->sa_family == AF_INET6)
    {
        m_address.v6 = *((sockaddr_in6*)addr);
        m_length = sizeof(m_address.v6);
    }
}

struct sockaddr* SocketAddress::get()
{
    return (struct sockaddr*) &m_address.v4;
}

const struct sockaddr* SocketAddress::get() const
{
    return (const struct sockaddr*) &m_address.v4;
}

socklen_t SocketAddress::size() const
{
    return m_address.v4.sin_family == AF_INET
        ? sizeof(m_address.v4)
        : sizeof(m_address.v6);
}

socklen_t& SocketAddress::length()
{
    return m_length;
}

int SocketAddress::family() const
{
    return m_address.v4.sin_family;
}

void SocketAddress::setFamily(int val)
{
    m_address.v4.sin_family = val;
}

unsigned short SocketAddress::port() const
{
    return m_address.v4.sin_family == AF_INET
        ? ntohs(m_address.v4.sin_port)
        : ntohs(m_address.v6.sin6_port);
}

void SocketAddress::copy(sockaddr* name, int* namelen) const
{
    if (family() == AF_INET)
    {
        const auto bytesToCopy = std::min<int>(*namelen, sizeof(m_address.v4));
        memcpy(name, &m_address.v4, bytesToCopy);
        *namelen = bytesToCopy;
    }
    else
    {
        const auto bytesToCopy = std::min<int>(*namelen, sizeof(m_address.v6));
        memcpy(name, &m_address.v6, bytesToCopy);
        *namelen = bytesToCopy;
    }
}

void SocketAddress::copy(uint32_t ip[]) const
{
    if (family() == AF_INET)
    {
        ip[0] = m_address.v4.sin_addr.s_addr;
    }
    else
    {
        const auto a = &m_address.v6;
        ip[3] = (a->sin6_addr.s6_addr[15] << 24) + (a->sin6_addr.s6_addr[14] << 16) + (a->sin6_addr.s6_addr[13] << 8) + a->sin6_addr.s6_addr[12];
        ip[2] = (a->sin6_addr.s6_addr[11] << 24) + (a->sin6_addr.s6_addr[10] << 16) + (a->sin6_addr.s6_addr[9] << 8) + a->sin6_addr.s6_addr[8];
        ip[1] = (a->sin6_addr.s6_addr[7] << 24) + (a->sin6_addr.s6_addr[6] << 16) + (a->sin6_addr.s6_addr[5] << 8) + a->sin6_addr.s6_addr[4];
        ip[0] = (a->sin6_addr.s6_addr[3] << 24) + (a->sin6_addr.s6_addr[2] << 16) + (a->sin6_addr.s6_addr[1] << 8) + a->sin6_addr.s6_addr[0];
    }
}

sockaddr_in& SocketAddress::v4()
{
    return m_address.v4;
}

const sockaddr_in& SocketAddress::v4() const
{
    return m_address.v4;
}

sockaddr_in6& SocketAddress::v6()
{
    return m_address.v6;
}

const sockaddr_in6& SocketAddress::v6() const
{
    return m_address.v6;
}

bool SocketAddress::operator==(const SocketAddress& right) const
{
    if (family() != right.family())
        return false;

    if (family() == AF_INET)
    {
        return v4().sin_addr.s_addr == right.v4().sin_addr.s_addr
            && v4().sin_port == right.v4().sin_port;
    }
    else
    {
        return memcmp(&v6().sin6_addr, &right.v6().sin6_addr, sizeof(v6().sin6_addr)) == 0
            && v6().sin6_port == right.v6().sin6_port;
    }
}

} // namespace detail
