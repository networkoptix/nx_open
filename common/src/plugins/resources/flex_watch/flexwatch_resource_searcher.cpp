#include "flexwatch_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "flexwatch_resource.h"
#include "utils/common/sleep.h"
#include "plugins/resources/onvif/onvif_resource_searcher.h"
#include "utils/network/mac_address.h"

static const qint64 SOCK_UPDATE_INTERVAL = 1000000ll * 60 * 5;

QnFlexWatchResourceSearcher::QnFlexWatchResourceSearcher(): OnvifResourceSearcher()
    ,m_sockUpdateTime(0)
{
}

QnFlexWatchResourceSearcher::~QnFlexWatchResourceSearcher()
{
    clearSocketList();
}

void QnFlexWatchResourceSearcher::clearSocketList()
{
    foreach(QUdpSocket* sock, m_sockList)
        delete sock;
    m_sockList.clear();
}

QnFlexWatchResourceSearcher& QnFlexWatchResourceSearcher::instance()
{
    static QnFlexWatchResourceSearcher inst;
    return inst;
}

bool QnFlexWatchResourceSearcher::updateSocketList()
{
    qint64 curretTime = getUsecTimer();
    if (curretTime - m_sockUpdateTime > SOCK_UPDATE_INTERVAL)
    {
        clearSocketList();
        foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
        {
            QUdpSocket* sock = new QUdpSocket();
            if (!bindToInterface(*sock, iface, 51001)) {
                delete sock;
                continue;
            }
            m_sockList << sock;
        }
        m_sockUpdateTime = curretTime;
        return true;
    }
    return false;
}

void QnFlexWatchResourceSearcher::sendBroadcast()
{
    QByteArray requestPattertn("53464a001c0000000000000000000000____f850000101000000d976");
    foreach (QUdpSocket* sock, m_sockList)
    {
        if (shouldStop())
            break;

        QByteArray rndPattern = QByteArray::number(qrand(),16);
        while (rndPattern.size() < 4)
            rndPattern = "0" + rndPattern;
        QByteArray pattern = requestPattertn.replace("____", rndPattern);
        QByteArray request = QByteArray::fromHex(pattern);
        // sending broadcast
        sock->writeDatagram(request.data(), request.size(),QHostAddress::Broadcast, 51000);
    }
}

QnResourceList QnFlexWatchResourceSearcher::findResources()
{
    if (updateSocketList()) {
        sendBroadcast();
        QnSleep::msleep(1000);
    }

    QnResourceList result;

    QSet<QString> processedMac;

    foreach (QUdpSocket* sock, m_sockList)
    {
        if (shouldStop())
            return QnResourceList();

        while (sock->hasPendingDatagrams())
        {
            QByteArray datagram;
            datagram.resize(sock->pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;
            sock->readDatagram(datagram.data(), datagram.size(),    &sender, &senderPort);

            if (!datagram.startsWith("SFJ"))
                continue;
            if (datagram.size() < 64)
                continue;

            EndpointAdditionalInfo info;
            info.name = QLatin1String(datagram.data() + 36);
            info.manufacturer = QLatin1String(datagram.data() + 36 + 16);
            if (info.manufacturer != QLatin1String("flex encoder") && !info.manufacturer.toLower().contains(QLatin1String("system")))
                continue;
            //info.mac = QString::fromLocal8Bit(datagram.mid(30,6).toHex());
            info.mac = QnMacAddress((const unsigned char*) datagram.data() + 30).toString();

            if (processedMac.contains(info.mac))
                continue;
            processedMac << info.mac;

            info.uniqId = info.mac;
            info.discoveryIp = sender.toString();

            OnvifResourceInformationFetcher onfivFetcher;
            onfivFetcher.findResources(QString(QLatin1String("http://%1/onvif/device_service")).arg(info.discoveryIp), info, result);

        }

    }

    sendBroadcast();
    return result;
}
