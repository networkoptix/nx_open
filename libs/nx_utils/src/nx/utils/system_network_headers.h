// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

// Windows network headers mut be included in a certain order to function properly.
#if defined(_WIN32)
    #if !defined(FD_SETSIZE)
        #define FD_SETSIZE 2048
    #endif

    // winsock2.h must be included before windows.h.
    #include <winsock2.h>

    // windows.h must be included before other network-related libraries.
    #include <windows.h>

    // ws2tcpip.h must be included before iphlpapi.h.
    #include <ws2tcpip.h>

    // Other libraries follow here.
    #include <in6addr.h>
    #include <iphlpapi.h>
    #include <ws2ipdef.h>

    // Windows does not support this flag, so we emulate it
    #define MSG_DONTWAIT 0x01000000
#else
    #include <sys/socket.h>

    #include <arpa/inet.h>
    #include <netinet/in.h>

#endif

// PCH for this header is compiled as C, which does not support operator overloading
#ifdef __cplusplus
    #include <cstdint>
    #include <cstring>

    #include <QtCore/QtEndian>

    inline bool operator==(const in_addr& left, const in_addr& right)
    {
        return std::memcmp(&left, &right, sizeof(left)) == 0;
    }

    inline bool operator==(const in6_addr& left, const in6_addr& right)
    {
        return std::memcmp(&left, &right, sizeof(left)) == 0;
    }

    namespace nx::network {

    template<typename T>
    constexpr T fromHostToNetwork(T value)
    {
        return qToBigEndian(value);
    }

    template<typename T>
    constexpr T fromNetworkToHost(T value)
    {
        return qFromBigEndian(value);
    }

    } // namespace nx::util::network

    #if !defined(_WIN32)
        constexpr in_addr in4addr_loopback = {
            nx::network::fromHostToNetwork(static_cast<std::uint32_t>(INADDR_LOOPBACK))
        };
    #endif

    #ifndef htonll
    #define htonll nx::network::fromHostToNetwork<unsigned long long>
    #endif
    #ifndef ntohll
    #define ntohll nx::network::fromNetworkToHost<unsigned long long>
    #endif

#endif
