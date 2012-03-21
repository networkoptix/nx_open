#include "droid_resource_searcher.h"
#include "droid_resource.h"
#include "utils/network/nettools.h"

const int androidRecvPort = 5559;

QnPlDroidResourceSearcher::QnPlDroidResourceSearcher()
{
}

QnPlDroidResourceSearcher& QnPlDroidResourceSearcher::instance()
{
    static QnPlDroidResourceSearcher inst;
    return inst;
}

QnResourceList QnPlDroidResourceSearcher::findResources(void)
{

    QSet<QHostAddress> foundDevSet; // to avoid duplicates
    QnResourceList result;
    QUdpSocket recvSocket;
    if (recvSocket.bind(androidRecvPort))
    {
        while (recvSocket.hasPendingDatagrams())
        {
            QByteArray responseData;
            responseData.resize(recvSocket.pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;

            recvSocket.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);

            QString response(responseData);

            QStringList data = response.split(';');
            if (data.size() < 3)
                continue;

            if (data[0] != "Network Optix Cam")
                continue;

            QStringList ports = data[1].split(',');
            if (ports.size() < 4) {
                qWarning() << "Invalid droid response. Expected at least 4 ports";
                continue;
            }
            QStringList ipParams = ports[0].split(':');
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

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), "DroidLive");
            if (!rt.isValid())
                continue;

            resource->setTypeId(rt);
            //resource->setName(QString("Droid device ") + ip);
            resource->setName("DroidLive");
            resource->setMAC(data[2].replace(':', '-').toUpper());
            //resource->setHostAddress(hostAddr, QnDomainMemory);
            resource->setDiscoveryAddr(recvSocket.localAddress());

            resource->setUrl(QString("raw://") + data[1]);

            result.push_back(resource);
        }
    }
    else {
        qWarning() << "Can't open droid device search port " << androidRecvPort;
    }

    return result;
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

    result = QnVirtualCameraResourcePtr( new QnDroidResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Droid camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnPlDroidResourceSearcher::manufacture() const
{
    return QnDroidResource::MANUFACTURE;
}

QnResourcePtr QnPlDroidResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

