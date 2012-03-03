#include "nettools.h"
#include "ping.h"
#include "netstate.h"
#include "../common/log.h"

QList<QHostAddress> getAllIPv4Addresses()
{
    static QList<QHostAddress> lastResult;
    static QTime timer;
    static QMutex mutex;

    {
        // speed optimization
        QMutexLocker lock(&mutex);
        if (!lastResult.isEmpty() & timer.elapsed() < 5000)
            return lastResult;
    }

    QList<QHostAddress> ipaddrs = QNetworkInterface::allAddresses();
    QList<QHostAddress> result;

    for (int i = 0; i < ipaddrs.size(); ++i)
    {
        if (QAbstractSocket::IPv4Protocol == ipaddrs.at(i).protocol() && ipaddrs.at(i)!=QHostAddress::LocalHost)
        {
            static bool allowedInterfaceReady = false;
            static QList<QHostAddress> allowedInterfaces;
            if (!allowedInterfaceReady)
            {
                for (int j = 1; j < qApp->argc(); ++j)
                {
                    QString arg = qApp->argv()[j];
                    arg = arg.toLower();
                    while (arg.startsWith('-'))
                        arg = arg.mid(1);
                    if (arg.startsWith("if=")) {
                        QStringList tmp = arg.split('=')[1].split(';');
                        foreach(QString s, tmp)
                            allowedInterfaces << QHostAddress(s);
                    }
                }

                // check registry
                if (allowedInterfaces.isEmpty())
                {
                    QSettings settings;
                    QStringList tmp = settings.value("if").toString().split(';');
                    foreach(QString s, tmp) {
                        if (!s.isEmpty())
                            allowedInterfaces << QHostAddress(s);
                    }
                }
                if (!allowedInterfaces.isEmpty())
                    qDebug() << "Using net IF filter:" << allowedInterfaces;
                allowedInterfaceReady = true;
            }
            if (allowedInterfaces.isEmpty() || allowedInterfaces.contains(ipaddrs.at(i)))
                result.push_back(ipaddrs.at(i));
        }
    }

    QMutexLocker lock(&mutex);
    timer.restart();
    lastResult = result;

    return result;
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

unsigned char* MACsToByte(const QString& macs, unsigned char* pbyAddress)
{

    const char cSep = '-';
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

bool getNextAvailableAddr(CLSubNetState& state, const CLIPList& busy_lst)
{

    quint32 curr = state.currHostAddress.toIPv4Address();
    quint32 maxaddr = state.maxHostAddress.toIPv4Address();

    quint32 original = curr;

    CLPing ping;

    while(1)
    {

        ++curr;
        if (curr>maxaddr)
        {
            quint32 minaddr = state.minHostAddress.toIPv4Address();
            curr = minaddr; // start from min
        }

        if (curr==original)
            return false; // all addresses are busy

        if (busy_lst.contains(curr))// this ip is already in use
            continue;

        if (!ping.ping(QHostAddress(curr).toString(), 2, ping_timeout))
        {
            // is this free addr?
            // let's check with ARP request also; might be it's not pingable device

            //if (getMacByIP(QHostAddress(curr)).isEmpty()) // to long
            break;

        }
    }

    state.currHostAddress = QHostAddress(curr);
    return true;

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
    cl_log.log(QLatin1String("about to find all ip responded to ping...."), cl_logALWAYS);
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

    foreach(PinagableT addr, hostslist)
    {
        if (addr.online)
            result.push_back(QHostAddress(addr.addr));
    }

    cl_log.log(QLatin1String("Done. time elapsed = "), time.elapsed(), cl_logALWAYS);

    CL_LOG(cl_logDEBUG1)
    {
        cl_log.log(QLatin1String("ping results..."), cl_logALWAYS);
        foreach(QHostAddress addr, result)
            cl_log.log(addr.toString(), cl_logALWAYS);
    }



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
        for (int i = 0; i < mtb->dwNumEntries; ++i)
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
        for (int i = 0; i < mtb->dwNumEntries; ++i)
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

#elif defined(Q_OS_MAC)
void removeARPrecord(const QHostAddress& /*ip*/) {}

#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>


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
        cl_log.log("sysctl: route-sysctl-estimate error", cl_logERROR);
        return QString();
    }

    if ((buf = (char*)malloc(needed)) == NULL)
    {
        return QString();
    }

    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
    {
        cl_log.log("actual retrieval of routing table failed", cl_logERROR);
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
            cl_log.log(cl_logDEBUG1, "%d ? %d", ip.toIPv4Address(), ntohl(sinarp->sin_addr.s_addr));
            if (ip.toIPv4Address() == ntohl(sinarp->sin_addr.s_addr))
            {
                free(buf);
                return MACToString((unsigned char*)LLADDR(sdl));
            }
        }

        next += rtm->rtm_msglen;
    }

    free(buf);

    return QString();
}
#else // Linux
void removeARPrecord(const QHostAddress& /*ip*/) {}

QString getMacByIP(const QHostAddress& ip, bool /*net*/)
{
    return "";
}

#endif

