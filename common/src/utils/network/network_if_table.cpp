#include "network_if_table.h"

#ifdef __linux

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <stdio.h>

std::list<NetworkIfInfo> NetworkIfInfo::getAll()
{
    struct ifaddrs* ifAddrList;
    if (getifaddrs(&ifAddrList) != 0)
        return std::list<NetworkIfInfo>();

    std::list<NetworkIfInfo> ifInfoList;
    for (struct ifaddrs* it = ifAddrList; it; it = it->ifa_next)
        if (it->ifa_addr->sa_family == AF_INET)
        {
            auto sa = static_cast<struct sockaddr_in *>(ifa->ifa_addr);
            NetworkIfInfo info = { QLatin1String(it->ifa_name), sa->sin_addr };
            ifInfoList.push_back(info);
        }

    freeifaddrs(ifap);
    return ifInfoList;
}

#else

std::list<NetworkIfInfo> NetworkIfInfo::getAll()
{
    return std::list<NetworkIfInfo>();
}

#endif
