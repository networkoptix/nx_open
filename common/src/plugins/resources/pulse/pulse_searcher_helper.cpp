#ifdef ENABLE_PULSE_CAMERA

#include "pulse_searcher_helper.h"
#include "utils/network/nettools.h"
#include "utils/network/system_socket.h"
#include "utils/common/sleep.h"
#include "utils/network/mac_address.h"
#include "../pulse/pulse_resource.h"


QnPlPulseSearcherHelper::QnPlPulseSearcherHelper()
{

}

QnPlPulseSearcherHelper::~QnPlPulseSearcherHelper()
{
    
}

QList<QnPlPulseSearcherHelper::WSResult> QnPlPulseSearcherHelper::findResources()
{
    QMap<QString, WSResult> result;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        UDPSocket socket;

        if (!socket.setLocalAddressAndPort(iface.address.toString(), 0))
            continue;

        QString groupAddress(QLatin1String("224.111.111.1"));
        if (!socket.joinGroup(groupAddress, iface.address.toString())) 
            continue;

        QByteArray requestDatagram;
        requestDatagram.resize(43);
        requestDatagram.fill(0);
        requestDatagram.insert(0, QByteArray("grandstream"));
        requestDatagram.insert(12, 2);
        requestDatagram.insert(13, QByteArray("127.0.0.1"));
        requestDatagram.resize(43);

        socket.sendTo(requestDatagram.data(), requestDatagram.size(), groupAddress, 6789);

        QnSleep::msleep(150);
        while(socket.hasData())
        {
            QByteArray reply;
            reply.resize( AbstractDatagramSocket::MAX_DATAGRAM_SIZE );

            QString sender;
            quint16 senderPort;
            int readed = socket.recvFrom(reply.data(), reply.size(), sender, senderPort);
            if (readed < 1)
                continue;
                        
            WSResult res = parseReply(reply.left(readed));
            if (!res.mac.isEmpty())
            {
                res.ip = sender;
                res.disc_ip = iface.address.toString();
                result[res.mac] = res;
            }

        }
        socket.leaveGroup(groupAddress);
    }

    
    

    return result.values();
}


//=========================================================================
QnPlPulseSearcherHelper::WSResult QnPlPulseSearcherHelper::parseReply(const QByteArray& datagram)
{

    WSResult result;
    //QString reply = QString(datagram);
    if (!datagram.contains("grandstream") || !datagram.contains("IPCAMERA"))
    {
        return result;
    }

    result.manufacture = QLatin1String(QnPlPulseResource::MANUFACTURE);

    int mac_index = datagram.indexOf("IPCAMERA");
    if (mac_index<0)
        return result;

    mac_index += 10;

    if (mac_index+6 > datagram.size()-1)
        return result;

    unsigned char* macChar = (unsigned char*)datagram.data() + mac_index;

    QnMacAddress mac(macChar);

    result.mac = mac.toString();

    
    result.name = QLatin1String("PFD-2000DV");

    return result;
}

#endif // #ifdef ENABLE_PULSE_CAMERA
