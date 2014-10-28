/*#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include "onvif_resource_searcher_mdns.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/soapDeviceBindingProxy.h"
#include "onvif/wsseapi.h"

#include "utils/network/nettools.h"
#include "utils/network/mdns.h"
#include "utils/common/sleep.h"

#include <QtNetwork/QUdpSocket>

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


OnvifResourceSearcherMdns::OnvifResourceSearcherMdns():
    onvifFetcher(OnvifResourceInformationFetcher::instance())
{

}

OnvifResourceSearcherMdns& OnvifResourceSearcherMdns::instance()
{
    static OnvifResourceSearcherMdns inst;
    return inst;
}

void OnvifResourceSearcherMdns::findResources(QnResourceList& result, const OnvifSpecialResourceCreatorPtr& creator) const
{
    EndpointInfoHash endpointInfo;
    QList<QHostAddress> localAddresses = getAllIPv4Addresses();
#if 0
    CL_LOG(cl_logDEBUG1)
    {
        QString log;
        QTextStream(&log) << "OnvifResourceSearcher::findDevices  found " << localAddresses.size() << " adapter(s) with IPV4";
        NX_LOG(log, cl_logDEBUG1);

        for (int i = 0; i < localAddresses.size();++i)
        {
            QString slog;
            QTextStream(&slog) << localAddresses.at(i).toString();
            NX_LOG(slog, cl_logDEBUG1);
        }
    }
#endif


    for(const QHostAddress& localAddress: localAddresses)
    {
        QHostAddress groupAddress(QLatin1String("224.0.0.251"));

        QUdpSocket sendSocket, recvSocket;

        bool bindSucceeded = sendSocket.bind(localAddress, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);
        if (!bindSucceeded)
            continue;

        bindSucceeded = recvSocket.bind(QHostAddress::Any, MDNS_PORT, QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);

        if (!bindSucceeded)
            continue;

        if (!multicastJoinGroup(recvSocket, groupAddress, localAddress))      continue;

        MDNSPacket request;

        request.addQuery();

        QByteArray datagram;
        request.toDatagram(datagram);

        sendSocket.writeDatagram(datagram.data(), datagram.size(), groupAddress, MDNS_PORT);

        QTime time;
        time.start();

        while(time.elapsed() < 1000) {
            checkSocket(recvSocket, localAddress, endpointInfo);
            QnSleep::msleep(10); // to avoid 100% cpu usage
        }

        multicastLeaveGroup(recvSocket, groupAddress);
    }

    onvifFetcher.findResources(endpointInfo, result, creator);
}


void OnvifResourceSearcherMdns::checkSocket(QUdpSocket& sock, QHostAddress localAddress, EndpointInfoHash& endpointInfo) const
{
    while (sock.hasPendingDatagrams())
    {
        QByteArray responseData;
        responseData.resize(sock.pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        sock.readDatagram(responseData.data(), responseData.size(),    &sender, &senderPort);
        //NX_LOG(cl_logALWAYS, "size: %d\n", responseData.size());
        if (sender == localAddress) continue;

        QString endpointUrl(QnPlOnvifResource::createOnvifEndpointUrl(sender.toString()));
        if (endpointUrl.isEmpty() || endpointInfo.contains(endpointUrl)) {
            continue;
        }

        endpointInfo.insert(endpointUrl, EndpointAdditionalInfo(
            EndpointAdditionalInfo::MDNS, responseData, localAddress.toString()));
    }

}
*/
