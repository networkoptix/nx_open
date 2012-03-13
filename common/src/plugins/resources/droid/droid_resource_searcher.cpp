#include "droid_resource_searcher.h"
#include "droid_resource.h"
#include "utils/network/nettools.h"

const int androidRecvPort = 5559;

QnPlDroidResourceSearcher::QnPlDroidResourceSearcher()
{
    m_recvSocket.bind(androidRecvPort);
}

QnPlDroidResourceSearcher& QnPlDroidResourceSearcher::instance()
{
    static QnPlDroidResourceSearcher inst;
    return inst;
}

struct DroidSearcherHelper
{
    int videoPort;
    int dataPort;
};

QnResourceList QnPlDroidResourceSearcher::findResources(void)
{
    QMap<QString, DroidSearcherHelper> searchReslut;

    while (m_recvSocket.hasPendingDatagrams())
    {
        QByteArray responseData;
        responseData.resize(m_recvSocket.pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        m_recvSocket.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);

        QString response(responseData);

        QStringList data = response.split(';');
        if (data.size()<2)
            continue;

        if (data.at(0) != "Network Optix Cam")
            continue;
        
        QStringList ipPorts = data.at(1).split(":");
        if (ipPorts.size() < 2)
            continue;

        QString ip = ipPorts.at(0);

        QStringList ports = ipPorts.at(1).split(",");
        if (ports.size() < 2)
            continue;

        DroidSearcherHelper t;
        t.videoPort = ports.at(0).toInt();
        t.dataPort = ports.at(1).toInt();

        searchReslut[ip] = t;
    }


    QnResourceList result;

    QMap<QString, DroidSearcherHelper>::iterator it = searchReslut.begin();
    while(it != searchReslut.end())
    {
        QString ip = it.key();
        QHostAddress ha(ip);

        DroidSearcherHelper t = it.value();

        QString name = "DroidLive";

        //QString mac = "94-63-D1-27-55-B8";
        QString mac = "40-FC-89-31-89-26";

        QnDroidResourcePtr resource ( new QnDroidResource() );

        QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
        if (!rt.isValid())
            continue;

        resource->setTypeId(rt);
        resource->setName(name);
        resource->setMAC(mac);
        resource->setHostAddress(ha, QnDomainMemory);
        resource->setDiscoveryAddr(m_recvSocket.localAddress());

        resource->setVideoPort(t.videoPort);
        resource->setDataPort(t.dataPort);

        result.push_back(resource);

        ++it;
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
        qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();

        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnDroidResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "RTID" << resourceTypeId.toString() << ", Parameters: " << parameters;
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

