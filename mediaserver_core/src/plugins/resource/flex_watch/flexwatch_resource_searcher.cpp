#include "flexwatch_resource_searcher.h"

#ifdef ENABLE_ONVIF

#include <nx/network/mac_address.h>
#include <nx/utils/random.h>
#include <utils/common/sleep.h>
#include <utils/common/util.h>

#include "flexwatch_resource.h"

static const qint64 SOCK_UPDATE_INTERVAL = 1000000ll * 60 * 5;

QnFlexWatchResourceSearcher::QnFlexWatchResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    OnvifResourceSearcher(commonModule),
    m_sockUpdateTime(0)
{
}

QnFlexWatchResourceSearcher::~QnFlexWatchResourceSearcher()
{
    clearSocketList();
}

void QnFlexWatchResourceSearcher::clearSocketList()
{
    for(AbstractDatagramSocket* sock: m_sockList)
        delete sock;
    m_sockList.clear();
}

bool QnFlexWatchResourceSearcher::updateSocketList()
{
    qint64 curretTime = getUsecTimer();
    if (curretTime - m_sockUpdateTime > SOCK_UPDATE_INTERVAL)
    {
        clearSocketList();
        for (QnInterfaceAndAddr iface: getAllIPv4Interfaces())
        {
            auto sock = SocketFactory::createDatagramSocket();
            //if (!bindToInterface(*sock, iface, 51001)) {
            if (!sock->bind(iface.address.toString(), 51001)) {
                continue;
            }
            m_sockList << sock.release();
        }
        m_sockUpdateTime = curretTime;
        return true;
    }
    return false;
}

void QnFlexWatchResourceSearcher::sendBroadcast()
{
    QByteArray requestPattertn("53464a001c0000000000000000000000____f850000101000000d976");
    for (AbstractDatagramSocket* sock: m_sockList)
    {
        if (shouldStop())
            break;

        QByteArray rndPattern = QByteArray::number(nx::utils::random::number(), 16);
        while (rndPattern.size() < 4)
            rndPattern = "0" + rndPattern;
        QByteArray pattern = requestPattertn.replace("____", rndPattern);
        QByteArray request = QByteArray::fromHex(pattern);
        // sending broadcast
        sock->sendTo(request.data(), request.size(), BROADCAST_ADDRESS, 51000);
    }
}

QList<QnResourcePtr> QnFlexWatchResourceSearcher::checkHostAddr(const nx::utils::Url& /*url*/, const QAuthenticator& /*auth*/, bool /*doMultichannelCheck*/)
{
    return QList<QnResourcePtr>(); // do not duplicate resource with ONVIF discovery!
}

QnResourceList QnFlexWatchResourceSearcher::findResources()
{
    if (updateSocketList()) {
        sendBroadcast();
        QnSleep::msleep(1000);
    }

    QnResourceList result;

    QSet<QString> processedMac;

    for (AbstractDatagramSocket* sock: m_sockList)
    {
        if (shouldStop())
            return QnResourceList();

        while (sock->hasData())
        {
            QByteArray datagram;
            datagram.resize(AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

            SocketAddress senderEndpoint;
            int readed = sock->recvFrom(datagram.data(), datagram.size(), &senderEndpoint);

            if (readed < 4 || !datagram.startsWith("SFJ"))
                continue;
            if (datagram.size() < 64)
                continue;

            EndpointAdditionalInfo info;
            info.name = QLatin1String(datagram.data() + 36);
            info.manufacturer = QLatin1String(datagram.data() + 36 + 16);
            if (info.manufacturer != QLatin1String("flex encoder") && !info.manufacturer.toLower().contains(QLatin1String("system")))
                continue;
            //info.mac = QString::fromLatin1(datagram.mid(30,6).toHex());
            info.mac = QnMacAddress((const unsigned char*) datagram.data() + 30).toString();

            if (processedMac.contains(info.mac))
                continue;
            processedMac << info.mac;

            info.uniqId = info.mac;
            info.discoveryIp = senderEndpoint.address.toString();

            m_informationFetcher->findResources(QString(QLatin1String("http://%1/onvif/device_service")).arg(info.discoveryIp), info, result, discoveryMode());
        }

    }

    sendBroadcast();
    return result;
}

#endif //ENABLE_ONVIF
