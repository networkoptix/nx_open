#include "onvif_device_searcher.h"
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




OnvifResourceSearcherOld::OnvifResourceSearcherOld()
{

}

OnvifResourceSearcherOld::~OnvifResourceSearcherOld()
{

};


bool OnvifResourceSearcherOld::isProxy() const
{
    return false;
}


QnResourceList OnvifResourceSearcherOld::findResources()
{
    QnResourceList result;

    QList<QHostAddress> localAddresses = getAllIPv4Addresses();
#if 0
    CL_LOG(cl_logDEBUG1)
    {
        QString log;
        QTextStream(&log) << "OnvifResourceSearcherOld::findDevices  found " << localAddresses.size() << " adapter(s) with IPV4";
        cl_log.log(log, cl_logDEBUG1);

        for (int i = 0; i < localAddresses.size();++i)
        {
            QString slog;
            QTextStream(&slog) << localAddresses.at(i).toString();
            cl_log.log(slog, cl_logDEBUG1);
        }
    }
#endif


    foreach(QHostAddress localAddress, localAddresses)
    {
        QHostAddress groupAddress(QLatin1String("224.0.0.251"));

        QUdpSocket sendSocket, recvSocket;

        bool bindSucceeded = sendSocket.bind(localAddress, 0);
        if (!bindSucceeded)
            continue;

        bindSucceeded = recvSocket.bind(QHostAddress::Any, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        
        if (!bindSucceeded)
            continue;

        if (!multicastJoinGroup(recvSocket, groupAddress, localAddress))      continue;

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
            checkSocket(recvSocket, result, localAddress);
            checkSocket(sendSocket, result, localAddress);
            QnSleep::msleep(10); // to avoid 100% cpu usage
      
        }

        multicastLeaveGroup(recvSocket, groupAddress);
    }


    return result;
}


void OnvifResourceSearcherOld::checkSocket(QUdpSocket& sock, QnResourceList& result, QHostAddress localAddress)
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


