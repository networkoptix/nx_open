
#include <iostream>
#include <sstream>
#include <memory>

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtNetwork/QHostInfo>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include "nettools.h"
#include "ping.h"

#if defined(Q_OS_LINUX)
#   include <arpa/inet.h>
#   include <sys/socket.h>
#   include <netdb.h>
#   ifndef Q_OS_ANDROID
#      include <ifaddrs.h>
#   endif
#   include <unistd.h>
#   include <net/if.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#   include <sys/ioctl.h>
#elif defined(Q_OS_MAC)
#   include <sys/file.h>
#   include <sys/socket.h>
#   include <sys/sysctl.h>
#   include <net/if.h>
#   include <net/if_dl.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <err.h>
#   include <stdio.h>
#   include <stdlib.h>
#   ifndef Q_OS_IOS
#      include <net/route.h>
#      include <netinet/if_ether.h>
#   endif
#elif defined(Q_OS_WIN)
#    include <winsock2.h>
#    include <ws2tcpip.h>
#    include <iphlpapi.h>
#endif

namespace {

static QList<QHostAddress> allowedInterfaces;

struct nettoolsFunctionsTag {};
static const nx::utils::log::Tag kLogTag(typeid(nettoolsFunctionsTag));

} // namespace

namespace nx {
namespace network {

static constexpr int kPingTimeoutMillis = 300;

void setInterfaceListFilter(const QList<QHostAddress>& ifList)
{
    allowedInterfaces = ifList;
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

QnInterfaceAndAddrList getAllIPv4Interfaces(InterfaceListPolicy policy)
{
    struct LocalCache
    {
        QnInterfaceAndAddrList value;
        QElapsedTimer timer;
        QnMutex guard;
    };

    static LocalCache caches[(int)InterfaceListPolicy::count];

    LocalCache &cache = caches[(int)policy];
    {
        // speed optimization
        QnMutexLocker lock(&cache.guard);
        enum { kCacheTimeout = 5000 };
        if (!cache.value.isEmpty() && (cache.timer.elapsed() < kCacheTimeout))
            return cache.value;
    }

    QList<QnInterfaceAndAddr> result;
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

#if defined(Q_OS_LINUX) && defined(__arm__)
        /* skipping 1.2.3.4 address on ISD */
        if (iface.name() == "usb0" && interfaces.size() > 1)
            continue;
#endif

        bool addInterfaceAnyway = policy == InterfaceListPolicy::allowInterfacesWithoutAddress;
        QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        for (const QNetworkAddressEntry& address : addresses)
        {
            const bool isLocalHost = (address.ip() == QHostAddress::LocalHost);
            const bool isIpV4 = (address.ip().protocol() == QAbstractSocket::IPv4Protocol);

            if (isIpV4 && !isLocalHost)
            {
                if (allowedInterfaces.isEmpty() || allowedInterfaces.contains(address.ip()))
                {
                    result.append(QnInterfaceAndAddr(iface.name(), address.ip(), address.netmask(), iface));
                    addInterfaceAnyway = false;
                    if (policy != InterfaceListPolicy::keepAllAddressesPerInterface)
                        break;
                }
            }
        }

        if (addInterfaceAnyway)
            result.append(QnInterfaceAndAddr(iface.name(), QHostAddress(), QHostAddress(), iface));
    }

    QnMutexLocker lock(&cache.guard);
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

            if (isIpV4 && (filter.testFlag(AddressFilter::ipV4)))
                result << HostAddress(address.ip().toString());

            if (isIpV6 && (filter.testFlag(AddressFilter::ipV6)))
            {
                QString addrString = ipv6AddrStringWithIfaceNameToAddrStringWithIfaceId(
                    address.ip().toString(),
                    iface.name(),
                    iface.index());
                result << HostAddress(addrString);
            }
        }
    }

    return result;
}

QList<QHostAddress> allLocalIpV4Addresses()
{
    QList<QHostAddress> rez;

    // if nothing else works use first enabled hostaddr
    for (const QnInterfaceAndAddr& iface : getAllIPv4Interfaces())
    {
        if (!(iface.netIf.flags() & QNetworkInterface::IsUp))
            continue;
        //if (!QUdpSocket().bind(iface.address, 0))
        //    continue;

        rez << iface.address;
    }

    if (rez.isEmpty())
        rez << QHostAddress(QLatin1String("127.0.0.1"));

    return rez;
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
        localIpList.insert(ip);
    return localIpList;
}

bool isInIPV4Subnet(QHostAddress addr, const QList<QNetworkAddressEntry>& ipv4_enty_list)
{
    for (int i = 0; i < ipv4_enty_list.size(); ++i)
    {
        const QNetworkAddressEntry& entr = ipv4_enty_list.at(i);
        if (entr.ip().isInSubnet(addr, entr.prefixLength()))
            return true;

    }
    
    return false;
}

struct PinagableT
{
    quint32 addr = 0;
    bool online = false;

    void f()
    {
        online = false;
        CLPing ping;
        if (ping.ping(QHostAddress(addr).toString(), 2, kPingTimeoutMillis))
            online = true;
    }
};

//{ windows
#if defined(Q_OS_WIN)

void removeARPrecord(const QHostAddress& ip)
{
    unsigned long ulSize = 0;
    GetIpNetTable(0, &ulSize, false);

    MIB_IPNETTABLE *mtb = new MIB_IPNETTABLE[ulSize / sizeof(MIB_IPNETTABLE) + 1];

    if (GetIpNetTable(mtb, &ulSize, false) == NO_ERROR)
    {
        //GetIpNetTable(mtb, &ulSize, TRUE);

        in_addr addr;
        for (uint i = 0; i < mtb->dwNumEntries; ++i)
        {
            addr.S_un.S_addr = mtb->table[i].dwAddr;

            QString wip = QString::fromLatin1(inet_ntoa(addr)); // ### NLS support?
            if (wip == ip.toString() && DeleteIpNetEntry(&mtb->table[i]) == NO_ERROR)
            {
                int old_size = mtb->dwNumEntries;

                GetIpNetTable(mtb, &ulSize, FALSE);

                if (mtb->dwNumEntries == old_size)
                    break;

                i = 0;
            }
        }
    }

    delete[] mtb;
}

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
void removeARPrecord(const QHostAddress& /*ip*/) {}

#if defined(Q_OS_IOS)
utils::MacAddress getMacByIP(const QHostAddress& /*ip*/, bool /*net*/)
{
    return MacAddress();
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
            NX_DEBUG(kLogTag, lm("%1 ? %2").arg(ip.toIPv4Address()).arg(ntohl(sinarp->sin_addr.s_addr)));
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
void removeARPrecord(const QHostAddress& /*ip*/) {}

utils::MacAddress getMacByIP(const QHostAddress& /*ip*/, bool /*net*/)
{
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
    // Introduce some maxPrefix(...) function and use it here.
    // E.g., auto eq1 = maxPrefix(host, newAddress).size();

    const auto eq1 = nx::utils::maxPrefix(
        host.toString().toStdString(),
        newAddress.toString().toStdString()).size();

    const auto eq2 = nx::utils::maxPrefix(
        host.toString().toStdString(),
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

    //TODO #ak read interface name
    strcpy(ifr.ifr_name, "eth0");

    //reading mac address
    if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1)
        return -1;
    for (int i = 0; i < HWADDR_len; i++)
        sprintf(&MAC_str[i * 3], "%02X-", ((unsigned char*)ifr.ifr_hwaddr.sa_data)[i]);
    MAC_str[MAC_ADDR_LEN - 1] = 0;

    //reading interface IP
    if (ioctl(s, SIOCGIFADDR, &ifr) == -1)
        return -1;
    const sockaddr_in* ip = (sockaddr_in*)&ifr.ifr_addr;
    //TODO #ak replace with inet_ntop?
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

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

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
                sprintf(MAC_str, "%02x:%02x:%02x:%02x:%02x:%02x",
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
