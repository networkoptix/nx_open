#include "pulse_searcher_helper.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "utils/qtsoap/qtsoap.h"
#include "utils/network/mac_address.h"
#include "../pulse/pulse_resource.h"

#ifdef Q_OS_LINUX
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif

extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);
extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QnPlPulseSearcherHelper::QnPlPulseSearcherHelper()
{

}

QnPlPulseSearcherHelper::~QnPlPulseSearcherHelper()
{
    
}

QList<QnPlPulseSearcherHelper::WSResult> QnPlPulseSearcherHelper::findResources()
{
    QMap<QString, WSResult> result;

    QList<QHostAddress> localAddresses = getAllIPv4Addresses();

#ifdef Q_OS_LINUX
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        cl_log.log(cl_logWARNING, "QnPlPulseSearcherHelper::findResources(): Can't get interfaces list: %s", strerror(errno));
        return result.values();
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        family = ifa->ifa_addr->sa_family;

        if (family != AF_INET)
            continue;

        QUdpSocket socket;
        socket.bind(0);

        int res = setsockopt(socket.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, ifa->ifa_name, strlen(ifa->ifa_name));
        if (res != 0)
        {
            cl_log.log(cl_logWARNING, "QnPlPulseSearcherHelper::findResources(): Can't bind to interface %s: %s", ifa->ifa_name, strerror(errno));
            continue;
        }

        QHostAddress localAddress(ifa->ifa_addr);

#else // lif defined Q_OS_WIN
    foreach(QHostAddress localAddress, localAddresses)
    {
        QUdpSocket socket;

        bool bindSucceeded = socket.bind(localAddress, 0);
        if (!bindSucceeded)
            continue;
#endif

        QHostAddress groupAddress(QLatin1String("224.111.111.1"));
        if (!multicastJoinGroup(socket, groupAddress, localAddress)) continue;

		QByteArray requestDatagram;
		requestDatagram.resize(43);
		requestDatagram.fill(0);
		requestDatagram.insert(0, QByteArray("grandstream"));
		requestDatagram.insert(12, 2);
        requestDatagram.insert(13, QByteArray("127.0.0.1"));
 		requestDatagram.resize(43);

        socket.writeDatagram(requestDatagram.data(), requestDatagram.size(), groupAddress, 6789);

        QnSleep::msleep(150);
        while(socket.hasPendingDatagrams())
        {
            QByteArray reply;
            reply.resize(socket.pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;
            socket.readDatagram(reply.data(), reply.size(),	&sender, &senderPort);
            			
            WSResult res = parseReply(reply);
            if (res.mac!="")
            {
                res.ip = sender.toString();
                res.disc_ip = localAddress.toString();
                result[res.mac] = res;
            }

        }
        multicastLeaveGroup(socket, groupAddress);
    }

    
    

    return result.values();
}


//=========================================================================
QnPlPulseSearcherHelper::WSResult QnPlPulseSearcherHelper::parseReply(QByteArray& datagram)
{

    WSResult result;
    //QString reply = QString(datagram);
    if (!datagram.contains("grandstream") || !datagram.contains("IPCAMERA"))
    {
        return result;
    }

    result.manufacture = QnPlPulseResource::MANUFACTURE;

    int mac_index = datagram.indexOf("IPCAMERA");
    if (mac_index<0)
        return result;

    mac_index += 10;

    if (mac_index+6 > datagram.size()-1)
        return result;

    unsigned char* macChar = (unsigned char*)datagram.data() + mac_index;

    QnMacAddress mac(macChar);

    result.mac = mac.toString();

    
    result.name = "PFD-2000DV";

    return result;
}
