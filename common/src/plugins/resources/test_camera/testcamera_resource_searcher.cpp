#include "testcamera_resource_searcher.h"
#include "testcamera_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "testcamera_const.h"

static const qint64 SOCK_UPDATE_INTERVAL = 1000000ll * 60 * 5;

QnTestCameraResourceSearcher::QnTestCameraResourceSearcher(): m_sockUpdateTime(0)
{
}

QnTestCameraResourceSearcher::~QnTestCameraResourceSearcher()
{
    clearSocketList();
}

void QnTestCameraResourceSearcher::clearSocketList()
{
    foreach(const DiscoveryInfo& info, m_sockList)
        delete info.sock;
    m_sockList.clear();
}


QnTestCameraResourceSearcher& QnTestCameraResourceSearcher::instance()
{
    static QnTestCameraResourceSearcher inst;
    return inst;
}

bool QnTestCameraResourceSearcher::updateSocketList()
{
    qint64 curretTime = getUsecTimer();
    if (curretTime - m_sockUpdateTime > SOCK_UPDATE_INTERVAL)
    {
        clearSocketList();
        foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
        {
            DiscoveryInfo info(new QUdpSocket(), iface.address);
            if (info.sock->bind(iface.address, 0))
                m_sockList << info;
        }
        m_sockUpdateTime = curretTime;
        return true;
    }
    return false;
}

void QnTestCameraResourceSearcher::sendBroadcast()
{
    foreach (const DiscoveryInfo& info, m_sockList)
        info.sock->writeDatagram(TestCamConst::TEST_CAMERA_FIND_MSG, strlen(TestCamConst::TEST_CAMERA_FIND_MSG), QHostAddress::Broadcast, TestCamConst::DISCOVERY_PORT);
}

QnResourceList QnTestCameraResourceSearcher::findResources(void)
{
    if (updateSocketList()) {
        sendBroadcast();
        QnSleep::msleep(1000);
    }


    QSet<QHostAddress> foundDevSet; // to avoid duplicates
    QMap<QString, QnResourcePtr> resources;
    QSet<QString> processedMac;

    foreach(const DiscoveryInfo& info, m_sockList)
    {
        QUdpSocket* sock = info.sock;
        while (sock->hasPendingDatagrams())
        {
            QByteArray responseData;
            responseData.resize(sock->pendingDatagramSize());


            QHostAddress sender;
            quint16 senderPort;
            sock->readDatagram(responseData.data(), responseData.size(),    &sender, &senderPort);

            QList<QByteArray> params = responseData.split(';');
            if (params[0] != TestCamConst::TEST_CAMERA_ID_MSG || params.size() < 3)
                continue;

            int videoPort = params[1].toInt();
            const QString resName(tr("TestCameraLive"));
            for (int j = 2; j < params.size(); ++j)
            {
                QnTestCameraResourcePtr resource ( new QnTestCameraResource() );

                QnId rt = qnResTypePool->getResourceTypeId(manufacture(), resName);
                if (!rt.isValid())
                    continue;

                QLatin1String s(params[j]);

                resource->setTypeId(rt);
                resource->setName(resName);
                resource->setModel(resName);
                QString mac(s);
                if (processedMac.contains(mac))
                    continue;
                processedMac << mac;

                resource->setMAC(mac);
                resource->setDiscoveryAddr(info.ifAddr);
                resource->setUrl(QLatin1String("tcp://") + sender.toString() + QLatin1Char(':') + QString::number(videoPort) + QLatin1Char('/') + QLatin1String(params[j]));
                resources.insert(mac, resource);
            }
        }
    }
    QnResourceList rez;
    foreach(QnResourcePtr res, resources.values())
        rez << res;

    sendBroadcast();

    return rez;
}

QnResourcePtr QnTestCameraResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        //qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnTestCameraResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create test camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnTestCameraResourceSearcher::manufacture() const
{
    return QLatin1String(QnTestCameraResource::MANUFACTURE);
}

QList<QnResourcePtr> QnTestCameraResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    Q_UNUSED(doMultichannelCheck)
    return QList<QnResourcePtr>();
}

