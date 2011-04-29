#include "nettools.h"
#include "../base/log.h"
#include "ping.h"
#include "netstate.h"

int ping_timeout = 100;

QList<QHostAddress> getAllIPv4Addresses()
{
	QList<QHostAddress> ipaddrs = QNetworkInterface::allAddresses();
	QList<QHostAddress> result;

	for (int i = 0; i < ipaddrs.size(); ++i)
	{
		if (QAbstractSocket::IPv4Protocol == ipaddrs.at(i).protocol() && ipaddrs.at(i)!=QHostAddress::LocalHost)
			result.push_back(ipaddrs.at(i));
	}

	return result;
}

QString MACToString (unsigned char* mac)
{
	char t[4];

	QString result;

	for (int i = 0; i < 6; i++)
	{
		if (i<5)
			sprintf (t, ("%02X-"), mac[i]);
		else
			sprintf (t, ("%02X"), mac[i]);

		result+=t;
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
		//	b. else (ch - 'a' + 10) it is done 
		//	because addition of 10 takes correct value.
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
			if (QAbstractSocket::IPv4Protocol == adrentr.ip().protocol() && adrentr.ip()!=QHostAddress::LocalHost)
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

		if (busy_lst.find(curr)!=busy_lst.end())// this ip is already in use
			continue;

		if (!ping.ping(QHostAddress(curr).toString(), 2, ping_timeout))
		{
			// is this free addr?
			// let's check with ARP request also; might be it's not pingable device

			//if (getMacByIP(QHostAddress(curr)) == "") // to long 
			break;

		}
	}

	state.currHostAddress = QHostAddress(curr);
	return true;

}

//{ windows
#ifdef _WIN32

void removeARPrecord(const QHostAddress& ip)
{
	MIB_IPNETTABLE* mtb = 0;
	unsigned long ulSize = 0;
	GetIpNetTable(0, &ulSize, false);

	mtb = new MIB_IPNETTABLE[ ulSize/sizeof(MIB_IPNETTABLE) + 1];

	if(NO_ERROR != GetIpNetTable(mtb, &ulSize, false))
		return;

	//GetIpNetTable(mtb, &ulSize, TRUE);

	in_addr addr;
	int i = 0;
	for(i = 0; i < mtb->dwNumEntries; i++)
	{
		addr.S_un.S_addr = mtb->table[i].dwAddr;

		QString wip;

		wip = (inet_ntoa(addr));
		if(wip == ip.toString() && NO_ERROR == DeleteIpNetEntry(&mtb->table[i]))
		{
			int old_size = mtb->dwNumEntries;

			GetIpNetTable(mtb, &ulSize, FALSE);

			if (mtb->dwNumEntries == old_size)
			{
				break;
			}

			i = 0;

		}
	}

	delete[] mtb;
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
			return "";

		return MACToString((unsigned char*)pulMac);
	}

	// from memorry
	MIB_IPNETTABLE* mtb = 0;
	unsigned long ulSize = 0;
	GetIpNetTable(0, &ulSize, false);

	mtb = new MIB_IPNETTABLE[ ulSize/sizeof(MIB_IPNETTABLE) + 1];

	if(NO_ERROR != GetIpNetTable(mtb, &ulSize, false))
		return "";

	//GetIpNetTable(mtb, &ulSize, TRUE);

	in_addr addr;
	int i = 0;
	for(i = 0; i < mtb->dwNumEntries; i++)
	{
		addr.S_un.S_addr = mtb->table[i].dwAddr;
		QString wip;
		wip = (inet_ntoa(addr));
		if(wip == ip.toString() )
		{
			return MACToString((unsigned char*)(mtb->table[i].bPhysAddr));
		}
	}

	delete[] mtb;

	return "";

}

#else
void removeARPrecord(const QHostAddress& ip) {}

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


QString getMacByIP(const QHostAddress& ip, bool net)
{
	return "";
	
	// This function is almost working in net=false mode
	// Need to fix ip comparison.
	
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
		return "";
//		errx(1, "route-sysctl-estimate");
	}
	
    if ((buf = (char*)malloc(needed)) == NULL)
	{
		return "";
//        errx(1, "malloc");
	}
	
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
	{
		cl_log.log("actual retrieval of routing table failed", cl_logERROR);
		return "";
//        errx(1, "actual retrieval of routing table");
	}
	
    lim = buf + needed;
    next = buf;
    while (next < lim) {
        struct rt_msghdr *rtm = (struct rt_msghdr *)next;
        struct sockaddr_inarp *sinarp = (struct sockaddr_inarp *)(rtm + 1);
        struct sockaddr_dl *sdl = (struct sockaddr_dl *)((char *)sinarp + ROUNDUP(sinarp->sin_len));
		
        if (sdl->sdl_alen) { /* complete ARP entry */
			cl_log.log(cl_logALWAYS, "%d ? %d", ip.toIPv4Address(), sinarp->sin_addr.s_addr);
			if (ip.toIPv4Address() == sinarp->sin_addr.s_addr)
			{
				free(buf);
				return MACToString((unsigned char*)LLADDR(sdl));
			}
			
//            printf("%s at ", inet_ntoa(sinarp->sin_addr));
//            printf("%s", ether_ntoa((struct ether_addr *)LLADDR(sdl)));
//            printf("\n");
        }
        next += rtm->rtm_msglen;
    }
	
    free(buf);
	
	return "";
}
#endif
//}

