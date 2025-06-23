#pragma once

#ifdef _WIN32
#   include <winsock2.h>
#   include <Ws2ipdef.h>
#else
#   include <netinet/in.h>
#endif

#include <stdint.h>
#include <string>

#ifdef _WIN32
    using socklen_t = int;
#endif

namespace detail {

class UDT_API SocketAddress
{
public:
    SocketAddress();

    /**
     * Copies from addr.
     */
    SocketAddress(const sockaddr_storage* addr, socklen_t addrLen);
    SocketAddress(const sockaddr* addr, int addrLen);

    struct sockaddr* get();
    const struct sockaddr* get() const;

    socklen_t size() const;

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

    sockaddr_in* v4();
    const sockaddr_in* v4() const;

    sockaddr_in6* v6();
    const sockaddr_in6* v6() const;

    bool operator==(const SocketAddress& right) const;

    std::string toString() const;

private:
    sockaddr_storage m_address;
    socklen_t m_length = 0;
};

} // namespace detail
