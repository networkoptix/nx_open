#include "droid_resource_searcher.h"
#include "droid_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/synctime.h"

const int androidRecvPort = 5559;
static const int READ_IF_TIMEOUT = 1000000ll * 30;

QnPlDroidResourceSearcher::QnPlDroidResourceSearcher():
    m_controlPortListener(QHostAddress::Any, DROID_CONTROL_TCP_SERVER_PORT)
{
    m_lastReadSocketTime = 0;
    m_controlPortListener.start();
}

QnPlDroidResourceSearcher& QnPlDroidResourceSearcher::instance()
{
    static QnPlDroidResourceSearcher inst;
    return inst;
}

QnResourceList QnPlDroidResourceSearcher::findResources(void)
{

    QSet<QHostAddress> foundDevSet; // to avoid duplicates
    QMap<QString, QnResourcePtr> result;

    qint64 time = qnSyncTime->currentMSecsSinceEpoch();
    if (time - m_lastReadSocketTime > READ_IF_TIMEOUT)
    {
        m_socketList.clear();
        QList<QnInterfaceAndAddr> ipaddrs = getAllIPv4Interfaces();
        for (int i = 0; i < ipaddrs.size();++i)
        {
            QSharedPointer<QUdpSocket> sock(new QUdpSocket());
            if (sock->bind(ipaddrs.at(i).address, androidRecvPort))
                m_socketList << sock;
            m_lastReadSocketTime = time;
        }
    }

    for (int i = 0; i < m_socketList.size(); ++i)
    {
        while (m_socketList[i]->hasPendingDatagrams())
        {
            QByteArray responseData;
            responseData.resize(m_socketList[i]->pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;

            m_socketList[i]->readDatagram(responseData.data(), responseData.size(),    &sender, &senderPort);

            QString response = QLatin1String(responseData);

            QStringList data = response.split(QLatin1Char(';'));
            if (data.size() < 3)
                continue;

            if (data[0] != QLatin1String("Network Optix Cam"))
                continue;

            QStringList ports = data[1].split(QLatin1Char(','));
            if (ports.size() < 2) {
                qWarning() << "Invalid droid response. Expected at least 4 ports";
                continue;
            }
            QStringList ipParams = ports[0].split(QLatin1Char(':'));
            if (ipParams.size() < 2) {
                qWarning() << "Invalid droid response. Expected IP:port";
                continue;
            }

            if (data[1].isEmpty() || data[2].isEmpty())
            {
                continue;
            }
        
            QHostAddress hostAddr(ipParams[0]);
            if (foundDevSet.contains(hostAddr))
                continue;

            foundDevSet << hostAddr;

            QnDroidResourcePtr resource ( new QnDroidResource() );

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("DroidLive"));
            if (!rt.isValid())
                continue;

            resource->setTypeId(rt);
            //resource->setName(QString("Droid device ") + ip);
            resource->setName(QLatin1String("DroidLive"));
            resource->setMAC(data[2].replace(QLatin1Char(':'), QLatin1Char('-')).toUpper());
            //resource->setHostAddress(hostAddr, QnDomainMemory);
            resource->setDiscoveryAddr(m_socketList[i]->localAddress());

            resource->setUrl(QLatin1String("raw://") + data[1]);

            result.insert(resource->getUrl(), resource);
        }
    }

    QnResourceList resList;
    foreach(QnResourcePtr resPtr, result)
        resList << resPtr;
    return resList;
}

QnResourcePtr QnPlDroidResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    if (!parameters.value(QLatin1String("url")).contains(QLatin1String("raw://")))
    {
        return result; // it is not a new droid resource
    }


    result = QnVirtualCameraResourcePtr( new QnDroidResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Droid camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnPlDroidResourceSearcher::manufacture() const
{
    return QLatin1String(QnDroidResource::MANUFACTURE);
}

QnResourcePtr QnPlDroidResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    return QnResourcePtr(0);
}

