
#include <iostream>
#include <sstream>
#include <memory>

#include <QtConcurrent/QtConcurrentMap>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QSettings>
#include <QtCore/QStringList>
#include <QtCore/QThreadPool>
#include <QtCore/QTime>
#include <QtNetwork/QHostInfo>

#include <nx/utils/log/log.h>

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
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <iphlpapi.h>
#endif

namespace {
    static QList<QHostAddress> allowedInterfaces;
} // namespace


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
    for (const QNetworkInterface &iface: interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

#if defined(Q_OS_LINUX) && defined(__arm__)
        /* skipping 1.2.3.4 address on ISD */
        if (iface.name() == lit("usb0") && interfaces.size() > 1)
            continue;
#endif

        bool addInterfaceAnyway = policy == InterfaceListPolicy::allowInterfacesWithoutAddress;
        QList<QNetworkAddressEntry> addresses = iface.addressEntries();
        for (const QNetworkAddressEntry& address: addresses)
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

QList<QHostAddress> allLocalAddresses()
{
    QList<QHostAddress> rez;

    // if nothing else works use first enabled hostaddr
    for(const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
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

QString MACToString (const unsigned char* mac)
{
    char t[4];

    QString result;

    for (int i = 0; i < 6; i++)
    {
        if (i<5)
            sprintf (t, ("%02X-"), mac[i]);
        else
            sprintf (t, ("%02X"), mac[i]);

        result += QString::fromLatin1(t);
    }

    return result;
}

unsigned char* MACsToByte(const QString& macs, unsigned char* pbyAddress, const char cSep)
{
    QByteArray arr = macs.toLatin1();
    const char *pszMACAddress = arr.data();

    for (int iConunter = 0; iConunter < 6; ++iConunter)
    {
        unsigned int iNumber = 0;
        char ch;

        //Convert letter into lower case.
        ch = tolower (*pszMACAddress++);

        if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
        {
            return 0;
        }

        //Convert into number.
        //       a. If character is digit then ch - '0'
        //    b. else (ch - 'a' + 10) it is done
        //    because addition of 10 takes correct value.
        iNumber = isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
        ch = tolower (*pszMACAddress);

        if ((iConunter < 5 && ch != cSep) ||
            (iConunter == 5 && ch != '\0' && !isspace (ch)))
        {
            ++pszMACAddress;

            if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
            {
                return NULL;
            }

            iNumber <<= 4;
            iNumber += isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
            ch = *pszMACAddress;

            if (iConunter < 5 && ch != cSep)
            {
                return NULL;
            }
        }
        /* Store result.  */
        pbyAddress[iConunter] = (unsigned char) iNumber;
        /* Skip cSep.  */
        ++pszMACAddress;
    }
    return pbyAddress;
}

unsigned char* MACsToByte2(const QString& macs, unsigned char* pbyAddress)
{
    QString lmac = macs.toUpper();

    for (int i = 0; i  < 6; ++i)
    {
        QString hexS = lmac.mid(i*2, 2);
        pbyAddress[i] = hexS.toUInt(0, 16);
    }

    return pbyAddress;
}


QList<QNetworkAddressEntry> getAllIPv4AddressEntries()
{
    QList<QNetworkInterface> inter_list = QNetworkInterface::allInterfaces(); // all interfaces

    QList<QNetworkAddressEntry> ipv4_enty_list;

    for (int il = 0; il < inter_list.size(); ++il)
    {
        // for each interface get addr entries
        const QList<QNetworkAddressEntry>& addr_enntry_list = inter_list.at(il).addressEntries();

        // navigate all entries for current interface and peek up IPV4 only
        for (int al = 0; al < addr_enntry_list.size(); ++al)
        {
            const QNetworkAddressEntry& adrentr = addr_enntry_list.at(al);
            if (QAbstractSocket::IPv4Protocol == adrentr.ip().protocol() && // if it has IPV4
                adrentr.ip()!=QHostAddress::LocalHost &&// if this is not 0.0.0.0 or 127.0.0.1
                adrentr.netmask().toIPv4Address()!=0) // and mask !=0
                ipv4_enty_list.push_back(addr_enntry_list.at(al));

        }
    }

    return ipv4_enty_list;
}

QString getIfaceIPv4Addr(const QNetworkInterface& iface)
{
    for (const auto& addr : iface.addressEntries())
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
    for (const QNetworkInterface &iface : interfaces)
    {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

        QString ipv4Addr = getIfaceIPv4Addr(iface);
        if (!ipv4Addr.isEmpty())
            result << ipv4Addr;
    }
    return result;
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
    quint32 addr;
    bool online;

    void f()
    {
        online = false;
        CLPing ping;
        if (ping.ping(QHostAddress(addr).toString(), 2, ping_timeout))
            online = true;
    }
};

QList<QHostAddress> pingableAddresses(const QHostAddress& startAddr, const QHostAddress& endAddr, int threads)
{
    NX_LOG(QLatin1String("about to find all ip responded to ping...."), cl_logINFO);
    QTime time;
    time.restart();

    quint32 curr = startAddr.toIPv4Address();
    quint32 maxaddr = endAddr.toIPv4Address();


    QList<PinagableT> hostslist;

    while(curr < maxaddr)
    {
        PinagableT t;
        t.addr = curr;
        hostslist.push_back(t);

        ++curr;
    }


    QThreadPool* global = QThreadPool::globalInstance();
    for (int i = 0; i < threads; ++i ) global->releaseThread();
    QtConcurrent::blockingMap(hostslist, &PinagableT::f);
    for (int i = 0; i < threads; ++i )global->reserveThread();

    QList<QHostAddress> result;

    for(const PinagableT& addr: hostslist)
    {
        if (addr.online)
            result.push_back(QHostAddress(addr.addr));
    }

    NX_LOG(lit("Done. time elapsed = %1").arg(time.elapsed()), cl_logINFO);
    NX_LOG(lm("Ping results %1").container(result), cl_logINFO);
    return result;
}



//{ windows
#if defined(Q_OS_WIN)

void removeARPrecord(const QHostAddress& ip)
{
    unsigned long ulSize = 0;
    GetIpNetTable(0, &ulSize, false);

    MIB_IPNETTABLE *mtb = new MIB_IPNETTABLE[ulSize/sizeof(MIB_IPNETTABLE) + 1];

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

    delete [] mtb;
}

// this is only works in local networks
//if net == true it returns the mac of the first device responded on ARP request; in case if net = true it might take time...
// if net = false it returns last device responded on ARP request
QString getMacByIP(const QHostAddress& ip, bool net)
{

    if (net)
    {
        //removeARPrecord(ip);

        HRESULT hr;
        IPAddr  ipAddr;
        ULONG   pulMac[2];
        ULONG   ulLen;

        ipAddr = htonl(ip.toIPv4Address());

        //ipAddr = inet_addr (ip.toLatin1().data());
        memset (pulMac, 0xff, sizeof (pulMac));
        ulLen = 6;

        hr = SendARP (ipAddr, 0, pulMac, &ulLen);

        if (ulLen==0)
            return QString();

        return MACToString((unsigned char*)pulMac);
    }

    QString res;

    // from memory
    unsigned long ulSize = 0;
    GetIpNetTable(0, &ulSize, false);

    MIB_IPNETTABLE *mtb = new MIB_IPNETTABLE[ ulSize/sizeof(MIB_IPNETTABLE) + 1];

    if (GetIpNetTable(mtb, &ulSize, false) == NO_ERROR)
    {
        //GetIpNetTable(mtb, &ulSize, TRUE);

        in_addr addr;
        for (uint i = 0; i < mtb->dwNumEntries; ++i)
        {
            addr.S_un.S_addr = mtb->table[i].dwAddr;
            QString wip = QString::fromLatin1(inet_ntoa(addr)); // ### NLS support?
            if (wip == ip.toString() )
            {
                res = MACToString((unsigned char*)(mtb->table[i].bPhysAddr));
                break;
            }
        }
    }

    delete [] mtb;

    return res;
}

/*
QHostAddress getGatewayOfIf( const QString& ip )
{
    NX_ASSERT( false );
    return QHostAddress();
}
*/

#elif defined(Q_OS_MAC)
void removeARPrecord(const QHostAddress& /*ip*/) {}

#ifdef Q_OS_IOS
QString getMacByIP(const QHostAddress& ip, bool /*net*/) {
    return QString();
}
#else

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

QString getMacByIP(const QHostAddress& ip, bool /*net*/)
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
        NX_LOG("sysctl: route-sysctl-estimate error", cl_logERROR);
        return QString();
    }

    if ((buf = (char*)malloc(needed)) == NULL)
    {
        return QString();
    }

    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
    {
        NX_LOG("actual retrieval of routing table failed", cl_logERROR);
        return QString();
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
            NX_LOG(lit("%1 ? %2").arg(ip.toIPv4Address()).arg(ntohl(sinarp->sin_addr.s_addr)), cl_logDEBUG1);
            if (ip.toIPv4Address() == ntohl(sinarp->sin_addr.s_addr)) {
                free(buf);
                return MACToString((unsigned char*)LLADDR(sdl));
            }
        }

        next += rtm->rtm_msglen;
    }

    free(buf);

    return QString();
}

#endif

/*
QHostAddress getGatewayOfIf(const QString& ip)
{
    return QHostAddress();
}
*/

#else // Linux
void removeARPrecord(const QHostAddress& ip) {Q_UNUSED(ip)}

QString getMacByIP(const QHostAddress& ip, bool net)
{
    Q_UNUSED(ip)
    Q_UNUSED(net)
    return QString();
}

/*
QHostAddress getGatewayOfIf(const QString& netIf)
{
    std::ostringstream cmd;
    cmd << "route -n | grep " << netIf.toStdString();
    FILE* fp = popen(cmd.str().c_str(), "r");

    char* line = NULL;
    size_t len = 0;
    while (getline(&line, &len, fp) != -1)
    {
        const auto info = QString(QLatin1String(line)).split(lit(" "), QString::SkipEmptyParts);
        QHostAddress addr(info[1]);
        if (info.size() > 1 && info[1] != QLatin1String("0.0.0.0"))
        {
            free(line);
            pclose(fp);
            return QHostAddress(info[1]);
        }

        free(line);
    }

    pclose(fp);
    return QHostAddress();
}
*/

#endif

QString getMacByIP(const QString& host, bool net)
{
    return getMacByIP(resolveAddress(host), net);
}

bool isIpv4Address(const QString& addr)
{
    int ip4Addr = inet_addr(addr.toLatin1().data());
    return ip4Addr != 0 && ip4Addr != -1;
}

QHostAddress resolveAddress(const QString& addrString)
{
    int ip4Addr = inet_addr(addrString.toLatin1().data());
    if (ip4Addr != 0 && ip4Addr != -1)
        return QHostAddress(ntohl(ip4Addr));

    QHostInfo hi = QHostInfo::fromName(addrString);
    if (!hi.addresses().isEmpty()) {
        for (const QHostAddress& addr: hi.addresses()) {
            if (addr.protocol() == QAbstractSocket::IPv4Protocol)
                return addr;
        }
    }

    return QHostAddress();
}

// TODO: #Elric I want to kill
int strEqualAmount(const char* str1, const char* str2)
{
    int rez = 0;
    while (*str1 && *str1 == *str2)
    {
        rez++;
        str1++;
        str2++;
    }
    return rez;
}

bool isNewDiscoveryAddressBetter(
    const HostAddress& host,
    const QHostAddress& newAddress,
    const QHostAddress& oldAddress )
{
    //TODO #ak compare binary values, not strings!
    int eq1 = strEqualAmount(host.toString().toLatin1().constData(), newAddress.toString().toLatin1().constData());
    int eq2 = strEqualAmount(host.toString().toLatin1().constData(), oldAddress.toString().toLatin1().constData());
    return eq1 > eq2;
}


int getFirstMacAddress(char  MAC_str[MAC_ADDR_LEN], char** host)
{
    memset(MAC_str, 0, MAC_ADDR_LEN);
    *host = 0;
    for (const auto& iface: QNetworkInterface::allInterfaces())
    {
        QByteArray addr = iface.hardwareAddress().toLocal8Bit().replace( ':', '-' );
        if (addr.isEmpty() || addr == QByteArray("00-00-00-00-00-00"))
            continue;

        memcpy(MAC_str, addr.constData(), qMin(addr.length(), MAC_ADDR_LEN));
        MAC_str[MAC_ADDR_LEN-1] = 0;
        return 0;
    }

    return -1;
}


#ifdef _WIN32

//TODO #ak refactor of function api is required
int getMacFromPrimaryIF(char  MAC_str[MAC_ADDR_LEN], char** host)
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
    if( s == -1 )
        return -1;
    auto SCOPED_GUARD_FUNC = []( int* socketHandlePtr ){ close(*socketHandlePtr); };
    std::unique_ptr<int, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( &s, SCOPED_GUARD_FUNC );

    //TODO #ak read interface name
    strcpy(ifr.ifr_name, "eth0");

    //reading mac address
    if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1)
        return -1;
    for (int i=0; i<HWADDR_len; i++)
        sprintf(&MAC_str[i*3],"%02X-",((unsigned char*)ifr.ifr_hwaddr.sa_data)[i]);
    MAC_str[MAC_ADDR_LEN-1] = 0;

    //reading interface IP
    if(ioctl(s, SIOCGIFADDR, &ifr) == -1)
        return -1;
    const sockaddr_in* ip = (sockaddr_in*) &ifr.ifr_addr;
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

#else	//mac, bsd

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
    for( struct ifaddrs* ifaptr = ifap; ifaptr != NULL; ifaptr = (ifaptr)->ifa_next)
    {
        std::cout<<"family = "<<(int)((ifaptr)->ifa_addr)->sa_family<<std::endl;
        switch( ((ifaptr)->ifa_addr)->sa_family )
        {
            case AF_LINK:
            {
                unsigned char* ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifaptr)->ifa_addr);
                sprintf(MAC_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                                  *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
                std::cout<<"AF_LINK. "<<ifaptr->ifa_name<<": "<<MAC_str<<std::endl;
                ifNameToLinkAddress[ifaptr->ifa_name] = MAC_str;
                break;
            }

            case AF_INET:
            {
                uint32_t ip4Addr = ((struct in_addr*)(ifaptr)->ifa_addr)->s_addr;
                std::cout<<"AF_INET. "<<ifaptr->ifa_name<<": "<<inet_ntoa( ((struct sockaddr_in*)ifaptr->ifa_addr)->sin_addr )<<std::endl;
                ifNameToInetAddress[ifaptr->ifa_name] = inet_ntoa( ((struct sockaddr_in*)ifaptr->ifa_addr)->sin_addr );
                break;
            }
        }
    }

    freeifaddrs(ifap);

    if( ifNameToInetAddress.empty() )
        return -1;	//no ipv4 address
    auto hwIter = ifNameToLinkAddress.find( ifNameToInetAddress.begin()->first );
    if( hwIter == ifNameToLinkAddress.end() )
        return -1;	//ipv4 interface has no link-level address

    strncpy( MAC_str, hwIter->second.c_str(), MAC_ADDR_LEN-1 );
    if( host )
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
