#pragma once

#ifdef _WIN32
#   include <winsock2.h>
#   include <Ws2ipdef.h>
#else
#   include <netinet/in.h>
#endif

#include <cstdint>

#ifdef _WIN32
    using socklen_t = int;
#endif

namespace detail {

class UDT_API SocketAddress
{
public:
    SocketAddress(int family = AF_INET6);

    /**
     * Copies from addr.
     */
    SocketAddress(const sockaddr* addr, int addrLen);

    struct sockaddr* get();
    const struct sockaddr* get() const;

    socklen_t size() const;
    socklen_t& length();

    int family() const;
    void setFamily(int val);

    unsigned short port() const;

    /**
     * Copies address to name.
     * @param namelen Number of bytes copied is stored here.
     */
    void copy(sockaddr* name, int* namelen) const;

    /**
     * @param ip MUST have enough space for Ipv6 address (16 bytes).
     */
    void copy(uint32_t ip[]) const;

    sockaddr_in& v4();
    const sockaddr_in& v4() const;

    sockaddr_in6& v6();
    const sockaddr_in6& v6() const;

    bool operator==(const SocketAddress& right) const;

private:
    union {
        sockaddr_in v4;
        sockaddr_in6 v6;
    } m_address;

    socklen_t m_length;
};

} // namespace detail
