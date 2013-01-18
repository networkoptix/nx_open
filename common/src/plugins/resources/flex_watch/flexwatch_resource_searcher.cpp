#include "flexwatch_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "flexwatch_resource.h"
#include "utils/common/sleep.h"
#include "plugins/resources/onvif/onvif_resource_searcher.h"
#include "utils/network/mac_address.h"

QnFlexWatchResourceSearcher::QnFlexWatchResourceSearcher(): OnvifResourceSearcher()
{
}

QnFlexWatchResourceSearcher& QnFlexWatchResourceSearcher::instance()
{
    static QnFlexWatchResourceSearcher inst;
    return inst;
}

QnResourceList QnFlexWatchResourceSearcher::findResources()
{
    QnResourceList result;

    QByteArray requestPattertn("53464a001c0000000000000000000000____f850000101000000d976");

    OnvifResourceInformationFetcher onfivFetcher;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        if (shouldStop())
            return QnResourceList();

        QByteArray rndPattern = QByteArray::number(rand(),16);
        while (rndPattern.size() < 4)
            rndPattern = "0" + rndPattern;
        QByteArray pattern = requestPattertn.replace("____", rndPattern);
        QByteArray request = QByteArray::fromHex(pattern);

        QUdpSocket sock;

        if (!bindToInterface(sock, iface, 51001))
            continue;
        
        // sending broadcast
        sock.writeDatagram(request.data(), request.size(),QHostAddress::Broadcast, 51000);

        QnSleep::msleep(1000); // to avoid 100% cpu usage

        while (sock.hasPendingDatagrams())
        {
            QByteArray datagram;
            datagram.resize(sock.pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;
            sock.readDatagram(datagram.data(), datagram.size(),    &sender, &senderPort);

            if (!datagram.startsWith("SFJ"))
                continue;
            if (datagram.size() < 64)
                continue;

            EndpointAdditionalInfo info;
            info.name = QLatin1String(datagram.data() + 36);
            info.manufacturer = QLatin1String(datagram.data() + 36 + 16);
            if (info.manufacturer != QLatin1String("flex encoder"))
                continue;
            //info.mac = QString::fromLocal8Bit(datagram.mid(30,6).toHex());
            info.mac = QnMacAddress((const unsigned char*) datagram.data() + 30).toString();
            info.uniqId = info.mac;
            info.discoveryIp = sender.toString();

            onfivFetcher.findResources(QString(QLatin1String("http://%1/onvif/device_service")).arg(info.discoveryIp), info, result);

        }

    }
    return result;
}
