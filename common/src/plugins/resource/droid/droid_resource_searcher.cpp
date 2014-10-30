#ifdef ENABLE_DROID


#include "droid_resource_searcher.h"
#include "droid_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/synctime.h"

const int androidRecvPort = 5559;
static const int READ_IF_TIMEOUT = 1000000ll * 30;

QnPlDroidResourceSearcher::QnPlDroidResourceSearcher()
{
    m_lastReadSocketTime = 0;
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
            QSharedPointer<AbstractDatagramSocket> sock(SocketFactory::createDatagramSocket());
            if (sock->bind(ipaddrs.at(i).address.toString(), androidRecvPort))
                m_socketList << sock;
            m_lastReadSocketTime = time;
        }
    }

    for (int i = 0; i < m_socketList.size(); ++i)
    {
        while (m_socketList[i]->hasData())
        {
            QByteArray responseData;
            responseData.resize( AbstractDatagramSocket::MAX_DATAGRAM_SIZE );

            QString sender;
            quint16 senderPort;

            int readed = m_socketList[i]->recvFrom(responseData.data(), responseData.size(),  sender, senderPort);
            if (readed < 1)
                continue;
            QString response = QLatin1String(responseData.left(readed));

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

            QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), QLatin1String("DroidLive"));
            if (rt.isNull())
                continue;

            resource->setTypeId(rt);
            resource->setName(QLatin1String("DroidLive"));
            resource->setMAC(QnMacAddress(data[2].replace(QLatin1Char(':'), QLatin1Char('-')).toUpper()));

            resource->setUrl(QLatin1String("raw://") + data[1]);

            result.insert(resource->getUrl(), resource);
        }
    }

    QnResourceList resList;
    for(const QnResourcePtr& resPtr: result)
        resList << resPtr;
    return resList;
}

QnResourcePtr QnPlDroidResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
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

    if (!params.url.contains(QLatin1String("raw://")))
    {
        return result; // it is not a new droid resource
    }


    result = QnVirtualCameraResourcePtr( new QnDroidResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Droid camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

    return result;

}

QString QnPlDroidResourceSearcher::manufacture() const
{
    return QnDroidResource::MANUFACTURE;
}

QList<QnResourcePtr> QnPlDroidResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    Q_UNUSED(doMultichannelCheck)
    return QList<QnResourcePtr>();
}

#endif // #ifdef ENABLE_DROID
