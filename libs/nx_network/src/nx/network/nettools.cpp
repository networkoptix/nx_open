// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "nettools.h"

#include <iostream>
#include <memory>
#include <sstream>

#include "system_network_headers.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtNetwork/QHostInfo>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include "ping.h"

#if defined(Q_OS_LINUX)
    #include <netdb.h>

    #ifndef Q_OS_ANDROID
        #include <ifaddrs.h>
    #endif
    #include <unistd.h>

    #include <net/if.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <sys/types.h>

    #include <net/if_arp.h>

    #include <netinet/in.h>
    #include <arpa/inet.h>
#elif defined(Q_OS_MAC)
    #include <err.h>
    #include <ifaddrs.h>
    #include <stdio.h>
    #include <stdlib.h>

    #include <net/if.h>
    #include <net/if_dl.h>
    #include <netinet/in.h>
    #include <sys/file.h>
    #include <sys/socket.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #ifndef Q_OS_IOS
        #include <net/route.h>
        #include <netinet/if_ether.h>
    #endif
#endif

namespace {

// NOTE: It is unknown if this legacy is used by somebody. Better to remove it some day if possible.
static QList<QHostAddress> allowedAddresses;

struct nettoolsFunctionsTag {};
static const nx::utils::log::Tag kLogTag(typeid(nettoolsFunctionsTag));
static const std::chrono::milliseconds kCacheTimeout(5000);

} // namespace

namespace nx {
namespace network {

void setInterfaceListFilter(const QList<QHostAddress>& ifList)
{
    allowedAddresses = ifList;
}

QHostAddress QnInterfaceAndAddr::broadcastAddress() const
{
    quint32 broadcastIpv4 = address.toIPv4Address() | ~netMask.toIPv4Address();
    return QHostAddress(broadcastIpv4);
}

QHostAddress QnInterfaceAndAddr::subNetworkAddress() const
{
    quint32 subnetworkIpV4 = address.toIPv4Address() & netMask.toIPv4Address();
    return QHostAddress(subnetworkIpV4);
}

bool QnInterfaceAndAddr::isHostBelongToIpv4Network(const QHostAddress& address) const
{
    auto between = [](const quint32 min, const quint32 value, const quint32 max)
    { return min <= value && value < max; };

    return between(
        subNetworkAddress().toIPv4Address(),
        address.toIPv4Address(),
        broadcastAddress().toIPv4Address());
}

QString QnInterfaceAndAddr::toString() const
{
    return NX_FMT("%1: %2 %3", name, address, netMask);
}

QnInterfaceAndAddrList getAllIPv4Interfaces(
    bool ignoreUsb0NetworkInterfaceIfOthersExist,
    InterfaceListPolicy interfaceListPolicy,
    bool ignoreLoopback)
{
    struct LocalCache
    {
        QnInterfaceAndAddrList value;
        QElapsedTimer timer;
        nx::Mutex guard;
    };

    static LocalCache caches[(int)InterfaceListPolicy::count];

    LocalCache &cache = caches[(int) interfaceListPolicy];
    {
        // speed optimization
        NX_MUTEX_LOCKER lock(&cache.guard);
        if (!cache.value.isEmpty() && (cache.timer.elapsed() < kCacheTimeout.count()))
            return cache.value;
    }

    QList<QnInterfaceAndAddr> result;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces)
    {
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        if (ignoreLoopback && iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;

        if (ignoreUsb0NetworkInterfaceIfOthersExist)
        {
            if (iface.name() == "usb0" && interfaces.size() > 1)
                continue;
        }

        bool addInterfaceAnyway =
            interfaceListPolicy == InterfaceListPolicy::allowInterfacesWithoutAddress;
        QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        for (const QNetworkAddressEntry& address : addresses)
        {
            if (ignoreLoopback && address.ip() == QHostAddress::LocalHost)
                continue;
            if (address.ip().protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            if (allowedAddresses.isEmpty() || allowedAddresses.contains(address.ip()))
            {
                result.append(QnInterfaceAndAddr(
                    iface.name(), address.ip(), address.netmask(), iface));
                addInterfaceAnyway = false;
                if (interfaceListPolicy != InterfaceListPolicy::keepAllAddressesPerInterface)
                    break;
            }
        }

        if (addInterfaceAnyway)
            result.append(QnInterfaceAndAddr(iface.name(), QHostAddress(), QHostAddress(), iface));
    }

    NX_MUTEX_LOCKER lock(&cache.guard);
    cache.timer.restart();
    cache.value = result;

    return result;
}

namespace {

static QString ipv6AddrStringWithIfaceNameToAddrStringWithIfaceId(
    const QString& ipv6AddrString,
    const QString& ifaceName,
    int ifaceIndex)
{
    int scopeIdDelimPos = ipv6AddrString.indexOf('%');
    if (scopeIdDelimPos == -1)
        return ipv6AddrString;

    NX_ASSERT(scopeIdDelimPos != 0 && scopeIdDelimPos != ipv6AddrString.length() - 1);
    if (scopeIdDelimPos == 0 || scopeIdDelimPos == ipv6AddrString.length() - 1)
        return QString();

    QString scopeIdTail = ipv6AddrString.mid(scopeIdDelimPos + 1);
    QString scopeIdFromIndex = QString::number(ifaceIndex);
    if (scopeIdTail == scopeIdFromIndex)
        return ipv6AddrString;

    NX_ASSERT(ifaceName == scopeIdTail);
    if (ifaceName != scopeIdTail)
        return QString();

    return ipv6AddrString.left(scopeIdDelimPos + 1) + scopeIdFromIndex;
}

} // namespace

QList<HostAddress> allLocalAddresses(AddressFilters filter)
{
    QList<HostAddress> result;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;

        if ((iface.flags().testFlag(QNetworkInterface::IsLoopBack)) &&
            (filter.testFlag(AddressFilter::noLoopback)))
        {
            continue;
        }

        for (const QNetworkAddressEntry& address: iface.addressEntries())
        {
            const bool isIpV4 = (address.ip().protocol() == QAbstractSocket::IPv4Protocol);
            const bool isIpV6 = (address.ip().protocol() == QAbstractSocket::IPv6Protocol);
            const bool isLocalIpV6 = isIpV6 && (address.ip() == QHostAddress::LocalHostIPv6);
            const bool isLocalIpV4 = isIpV4 && (address.ip() == QHostAddress::LocalHost);
            const bool isLocalHost = isLocalIpV6 || isLocalIpV4;

            if (isLocalHost && (filter.testFlag(AddressFilter::noLocal)))
                continue;

            if (!allowedAddresses.isEmpty() && !allowedAddresses.contains(address.ip()))
                continue;

            if ((isIpV4 && !filter.testFlag(AddressFilter::ipV4))
                || (isIpV6 && !filter.testFlag(AddressFilter::ipV6)))
            {
                continue;
            }

            if (isIpV4 && (filter.testFlag(AddressFilter::ipV4)))
                result << HostAddress(address.ip().toString().toStdString());

            if (isIpV6 && (filter.testFlag(AddressFilter::ipV6)))
            {
                QString addrString = ipv6AddrStringWithIfaceNameToAddrStringWithIfaceId(
                    address.ip().toString(),
                    iface.name(),
                    iface.index());
                result << HostAddress(addrString.toStdString());
            }

            if (filter.testFlag(AddressFilter::onePerDevice))
                break;
        }
    }

    return result;
}

QString getIfaceIPv4Addr(const QNetworkInterface& iface)
{
    for (const auto& addr: iface.addressEntries())
    {
        if (QAbstractSocket::IPv4Protocol == addr.ip().protocol() && // if it has IPV4
            addr.ip() != QHostAddress::LocalHost &&// if this is not 0.0.0.0 or 127.0.0.1
            addr.netmask().toIPv4Address() != 0) // and mask !=0
            return addr.ip().toString();
    }
    return QString();
}

// TODO: It is the same as allLocalAddresses(). Should be removed.
QSet<QString> getLocalIpV4AddressList()
{
    QSet<QString> result;

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface: interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            (iface.flags() & QNetworkInterface::IsLoopBack))
        {
            continue;
        }

        QString ipv4Addr = getIfaceIPv4Addr(iface);
        if (!ipv4Addr.isEmpty())
            result << ipv4Addr;
    }

    return result;
}

// TODO: Unit tests should be written here.
// NOTE: Contains redundant conversion: QHostAddress -> QString -> HostAddress.
std::set<HostAddress> getLocalIpV4HostAddressList()
{
    std::set<HostAddress> localIpList;
    for (const auto& ip : nx::network::getLocalIpV4AddressList())
        localIpList.insert(ip.toStdString());
    return localIpList;
}

//{ windows
#if defined(Q_OS_WIN)

// this is only works in local networks
//if net == true it returns the mac of the first device responded on ARP request; in case if net = true it might take time...
// if net = false it returns last device responded on ARP request
utils::MacAddress getMacByIP(const QHostAddress& ip, bool net)
{
    if (net)
    {
        HRESULT hr;
        IPAddr  ipAddr;
        ULONG   pulMac[2];
        ULONG   ulLen;

        ipAddr = htonl(ip.toIPv4Address());

        //ipAddr = inet_addr (ip.toLatin1().data());
        memset(pulMac, 0xff, sizeof(pulMac));
        ulLen = 6;

        hr = SendARP(ipAddr, 0, pulMac, &ulLen);

        if (ulLen == 0)
            return utils::MacAddress();

        return utils::MacAddress::fromRawData((unsigned char*)pulMac);
    }

    utils::MacAddress res;

    // from memory
    unsigned long ulSize = 0;
    GetIpNetTable(0, &ulSize, false);

    MIB_IPNETTABLE *mtb = new MIB_IPNETTABLE[ulSize / sizeof(MIB_IPNETTABLE) + 1];

    if (GetIpNetTable(mtb, &ulSize, false) == NO_ERROR)
    {
        in_addr addr;
        for (uint i = 0; i < mtb->dwNumEntries; ++i)
        {
            addr.S_un.S_addr = mtb->table[i].dwAddr;
            QString wip = QString::fromLatin1(inet_ntoa(addr)); // ### NLS support?
            if (wip == ip.toString())
            {
                res = utils::MacAddress::fromRawData((unsigned char*)(mtb->table[i].bPhysAddr));
                break;
            }
        }
    }

    delete[] mtb;

    return res;
}

#elif defined(Q_OS_MAC)
#if defined(Q_OS_IOS)
utils::MacAddress getMacByIP(const QHostAddress& /*ip*/, bool /*net*/)
{
    return utils::MacAddress();
}
#else // defined(Q_OS_IOS)

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

utils::MacAddress getMacByIP(const QHostAddress& ip, bool /*net*/)
{
    int mib[6];
    size_t needed;
    char *lim, *buf, *next;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_FLAGS;
    mib[5] = RTF_LLINFO;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
    {
        NX_ERROR(kLogTag, "sysctl: route-sysctl-estimate error");
        return utils::MacAddress();
    }

    if ((buf = (char*)malloc(needed)) == NULL)
    {
        return utils::MacAddress();
    }

    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
    {
        NX_ERROR(kLogTag, "actual retrieval of routing table failed");
        return utils::MacAddress();
    }

    lim = buf + needed;
    next = buf;
    while (next < lim)
    {
        struct rt_msghdr *rtm = (struct rt_msghdr *)next;
        struct sockaddr_inarp *sinarp = (struct sockaddr_inarp *)(rtm + 1);
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)((char *)sinarp + ROUNDUP(sinarp->sin_len));

        if (sdl->sdl_alen)
        {
            /* complete ARP entry */
            NX_DEBUG(kLogTag, nx::format("%1 ? %2").arg(ip.toIPv4Address()).arg(ntohl(sinarp->sin_addr.s_addr)));
            if (ip.toIPv4Address() == ntohl(sinarp->sin_addr.s_addr))
            {
                free(buf);
                return utils::MacAddress::fromRawData((unsigned char*)LLADDR(sdl));
            }
        }

        next += rtm->rtm_msglen;
    }

    free(buf);

    return utils::MacAddress();
}

#endif

#else // Linux
utils::MacAddress getMacByIP(const QHostAddress& ip, bool /*net*/)
{
    const auto interfaceList = getAllIPv4Interfaces(
        /* ignore usb */ false, InterfaceListPolicy::keepAllAddressesPerInterface);
    const auto it = std::find_if(interfaceList.begin(), interfaceList.end(),
        [&ip](const auto& iface) { return iface.isHostBelongToIpv4Network(ip); });
    if (it == interfaceList.end())
        return utils::MacAddress(); //< Can't proceed without interface name.

    arpreq req;
    memset(&req, 0, sizeof(req));

    auto sin = (sockaddr_in*) &req.arp_pa;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(ip.toIPv4Address());
    strncpy(req.arp_dev, it->name.toStdString().c_str(), sizeof(req.arp_dev) - 1);

    if (int s = socket(AF_INET, SOCK_DGRAM, 0); s != -1)
    {
        if (ioctl(s, SIOCGARP, &req) != -1)
        {
            if (req.arp_flags & ATF_COM)
            {
                auto rawData = (const unsigned char*) &req.arp_ha.sa_data[0];
                return utils::MacAddress::fromRawData(rawData);
            }
        }
        close(s);
    }

    return utils::MacAddress();
}

#endif

utils::MacAddress getMacByIP(const QString& host, bool net)
{
    return getMacByIP(resolveAddress(host), net);
}

QHostAddress resolveAddress(const QString& addrString)
{
    int ip4Addr = inet_addr(addrString.toLatin1().data());
    if (ip4Addr != 0 && ip4Addr != -1)
        return QHostAddress(ntohl(ip4Addr));

    QHostInfo hi = QHostInfo::fromName(addrString);
    if (!hi.addresses().isEmpty())
    {
        for (const QHostAddress& addr : hi.addresses())
        {
            if (addr.protocol() == QAbstractSocket::IPv4Protocol)
                return addr;
        }
    }

    return QHostAddress();
}

bool isNewDiscoveryAddressBetter(
    const HostAddress& host,
    const QHostAddress& newAddress,
    const QHostAddress& oldAddress)
{
    // TODO: Compare binary values, not strings!
    // TODO: Rename or move this function out of here.

    const auto eq1 = nx::utils::maxCommonPrefix(
        host.toString(),
        newAddress.toString().toStdString()).size();

    const auto eq2 = nx::utils::maxCommonPrefix(
        host.toString(),
        oldAddress.toString().toStdString()).size();

    return eq1 > eq2;
}

int getFirstMacAddress(char MAC_str[MAC_ADDR_LEN], char** host)
{
    memset(MAC_str, 0, MAC_ADDR_LEN);
    *host = 0;
    for (const auto& iface : QNetworkInterface::allInterfaces())
    {
        QByteArray addr = iface.hardwareAddress().toLocal8Bit().replace(':', '-');
        if (addr.isEmpty() || addr == QByteArray("00-00-00-00-00-00"))
            continue;

        memcpy(MAC_str, addr.constData(), qMin(addr.length(), MAC_ADDR_LEN));
        MAC_str[MAC_ADDR_LEN - 1] = 0;
        return 0;
    }

    return -1;
}

#ifdef _WIN32

int getMacFromPrimaryIF(char MAC_str[MAC_ADDR_LEN], char** host)
{
    return getFirstMacAddress(MAC_str, host);
}

#elif defined(__linux__)

int getMacFromEth0(char MAC_str[MAC_ADDR_LEN], char** host)
{
    memset(MAC_str, 0, MAC_ADDR_LEN);
#define HWADDR_len 6
    struct ifreq ifr;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1)
        return -1;
    auto SCOPED_GUARD_FUNC = [](int* socketHandlePtr) { close(*socketHandlePtr); };
    std::unique_ptr<int, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD(&s, SCOPED_GUARD_FUNC);

    //TODO #akolesnikov read interface name
    strcpy(ifr.ifr_name, "eth0");

    //reading mac address
    if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1)
        return -1;
    for (int i = 0; i < HWADDR_len; i++)
    {
        char buffer[4];
        snprintf(buffer, sizeof(buffer), "%02X-", ((unsigned char*)ifr.ifr_hwaddr.sa_data)[i]);
        memmove(&MAC_str[i * 3], buffer, 3);
    }
    MAC_str[MAC_ADDR_LEN - 1] = 0;

    //reading interface IP
    if (ioctl(s, SIOCGIFADDR, &ifr) == -1)
        return -1;
    const sockaddr_in* ip = (sockaddr_in*)&ifr.ifr_addr;
    //TODO #akolesnikov replace with inet_ntop?
    *host = inet_ntoa(ip->sin_addr);

    return 0;
}

int getMacFromPrimaryIF(char MAC_str[MAC_ADDR_LEN], char** host)
{
    int result = getMacFromEth0(MAC_str, host);
    if (result != 0 || MAC_str[0] == 0 || strcmp(MAC_str, "00-00-00-00-00-00") == 0)
        return getFirstMacAddress(MAC_str, host);
    else
        return result;
}

#else    //mac, bsd

int getMacFromPrimaryIF(char MAC_str[MAC_ADDR_LEN], char** host)
{
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0)
        return -1;

    std::map<std::string, std::string> ifNameToLinkAddress;
    std::map<std::string, std::string> ifNameToInetAddress;
    for (struct ifaddrs* ifaptr = ifap; ifaptr != NULL; ifaptr = (ifaptr)->ifa_next)
    {
        std::cout << "family = " << (int)((ifaptr)->ifa_addr)->sa_family << std::endl;
        switch (((ifaptr)->ifa_addr)->sa_family)
        {
            case AF_LINK:
            {
                unsigned char* ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifaptr)->ifa_addr);
                snprintf(MAC_str, MAC_ADDR_LEN * sizeof(char), "%02x:%02x:%02x:%02x:%02x:%02x",
                    *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4), *(ptr + 5));
                std::cout << "AF_LINK. " << ifaptr->ifa_name << ": " << MAC_str << std::endl;
                ifNameToLinkAddress[ifaptr->ifa_name] = MAC_str;
                break;
            }

            case AF_INET:
            {
                std::cout << "AF_INET. " << ifaptr->ifa_name << ": " << inet_ntoa(((struct sockaddr_in*)ifaptr->ifa_addr)->sin_addr) << std::endl;
                ifNameToInetAddress[ifaptr->ifa_name] = inet_ntoa(((struct sockaddr_in*)ifaptr->ifa_addr)->sin_addr);
                break;
            }
        }
    }

    freeifaddrs(ifap);

    if (ifNameToInetAddress.empty())
        return -1;    //no ipv4 address
    auto hwIter = ifNameToLinkAddress.find(ifNameToInetAddress.begin()->first);
    if (hwIter == ifNameToLinkAddress.end())
        return -1;    //ipv4 interface has no link-level address

    strncpy(MAC_str, hwIter->second.c_str(), MAC_ADDR_LEN - 1);
    if (host)
        *host = nullptr;

    return 0;
}
#endif

QString getMacFromPrimaryIF()
{
    char  mac[MAC_ADDR_LEN];
    char* host = 0;
    if (getMacFromPrimaryIF(mac, &host) != 0)
        return QString();
    return QString::fromLatin1(mac);
}

} // namespace network
} // namespace nx
