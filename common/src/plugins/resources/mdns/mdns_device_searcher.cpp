#include "mdns_device_searcher.h"
#include "utils/network/nettools.h"
#include "utils/network/mdns.h"
#include "utils/common/sleep.h"

#ifndef Q_OS_WIN
#include <netinet/in.h>
#include <sys/socket.h>
#endif



quint16 MDNS_PORT = 5353;

// These functions added temporary as in Qt 4.8 they are already in QUdpSocket
bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress)
{
    struct ip_mreq imr;

    memset(&imr, 0, sizeof(imr));

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = htonl(localAddress.toIPv4Address());

    int res = setsockopt(udpSocket.socketDescriptor(), IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
        return false;

    return true;
}

bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress)
{
    struct ip_mreq imr;

    imr.imr_multiaddr.s_addr = htonl(groupAddress.toIPv4Address());
    imr.imr_interface.s_addr = INADDR_ANY;

    int res = setsockopt(udpSocket.socketDescriptor(), IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&imr, sizeof(struct ip_mreq));
    if (res == -1)
        return false;

    return true;
}




QnMdnsResourceSearcher::QnMdnsResourceSearcher()
{

}

QnMdnsResourceSearcher::~QnMdnsResourceSearcher()
{

};


bool QnMdnsResourceSearcher::isProxy() const
{
    return false;
}


QnResourceList QnMdnsResourceSearcher::findResources()
{
    QnResourceList result;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        QUdpSocket sock;
#ifdef Q_OS_LINUX
        sock.bind(0);

        int res = setsockopt(sock.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, iface.name.constData(), iface.name.length());
        if (res != 0)
        {
            cl_log.log(cl_logWARNING, "QnMdnsResourceSearcher::findResources(): Can't bind to interface %s: %s", iface.name.constData(), strerror(errno));
            continue;
        }
#else // lif defined Q_OS_WIN
        if (!sock.bind(iface.address, 0))
           continue;
#endif

        QHostAddress groupAddress(QLatin1String("224.0.0.251"));

        QUdpSocket sendSocket, recvSocket;

        bool bindSucceeded = recvSocket.bind(QHostAddress::Any, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        if (!bindSucceeded)
            continue;

        if (!multicastJoinGroup(recvSocket, groupAddress, iface.address))      continue;

        MDNSPacket request;
        MDNSPacket response;

        request.addQuery();

        QByteArray datagram;
        request.toDatagram(datagram);

        sendSocket.writeDatagram(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);

        QTime time;
        time.start();

        while(time.elapsed() < 200)
        {
            checkSocket(recvSocket, result, iface.address);
            checkSocket(sendSocket, result, iface.address);
            QnSleep::msleep(10); // to avoid 100% cpu usage
      
        }

        multicastLeaveGroup(recvSocket, groupAddress);
    }


    return result;
}


void QnMdnsResourceSearcher::checkSocket(QUdpSocket& sock, QnResourceList& result, QHostAddress localAddress)
{
    while (sock.hasPendingDatagrams())
    {
        QByteArray responseData;
        responseData.resize(sock.pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        sock.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);
        //cl_log.log(cl_logALWAYS, "size: %d\n", responseData.size());
        if (senderPort != MDNS_PORT)// || sender == localAddress)
            senderPort = senderPort;
        //continue;

        QnNetworkResourcePtr nresource = processPacket(result, responseData);

        if (nresource==0)
            continue;


        nresource->setHostAddress(sender, QnDomainMemory);
        nresource->setDiscoveryAddr(localAddress);


        result.push_back(nresource);

    }

}


