#include "testcamera_resource_searcher.h"
#include "testcamera_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "testcamera_const.h"

QnTestCameraResourceSearcher::QnTestCameraResourceSearcher()
{
}

QnTestCameraResourceSearcher& QnTestCameraResourceSearcher::instance()
{
    static QnTestCameraResourceSearcher inst;
    return inst;
}

QnResourceList QnTestCameraResourceSearcher::findResources(void)
{

    QSet<QHostAddress> foundDevSet; // to avoid duplicates
    QMap<QString, QnResourcePtr> resources;

    QList<QHostAddress> ipaddrs = getAllIPv4Addresses();
    for (int i = 0; i < ipaddrs.size();++i)
    {
        QUdpSocket sock;
        if (!sock.bind(ipaddrs.at(i), 0))
            continue;
        sock.writeDatagram(TestCamConst::TEST_CAMERA_FIND_MSG, strlen(TestCamConst::TEST_CAMERA_FIND_MSG), QHostAddress::Broadcast, TestCamConst::DISCOVERY_PORT);
        QnSleep::msleep(150);
        while (sock.hasPendingDatagrams())
        {
            QByteArray responseData;
            responseData.resize(sock.pendingDatagramSize());


            QHostAddress sender;
            quint16 senderPort;
            sock.readDatagram(responseData.data(), responseData.size(),	&sender, &senderPort);

            QList<QByteArray> params = responseData.split(';');
            if (params[0] != TestCamConst::TEST_CAMERA_ID_MSG || params.size() < 3)
                continue;

            int videoPort = params[1].toInt();
            const QString resName("TestCameraLive");
            for (int j = 2; j < params.size(); ++j)
            {
                QnTestCameraResourcePtr resource ( new QnTestCameraResource() );

                QnId rt = qnResTypePool->getResourceTypeId(manufacture(), resName);
                if (!rt.isValid())
                    continue;

                resource->setTypeId(rt);
                resource->setName(resName);
                QString mac(params[j]);
                resource->setMAC(mac);
                resource->setDiscoveryAddr(ipaddrs.at(i));
                resource->setUrl(QString("tcp://") + sender.toString() + QString(':') + QString::number(videoPort) + QString("/") + QString(params[j]));
                resources.insert(mac, resource);
            }
        }
    }
    QnResourceList rez;
    foreach(QnResourcePtr res, resources.values())
        rez << res;
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
        qDebug() << "Manufature " << resourceType->getManufacture() << " != " << manufacture();

        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnTestCameraResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "RTID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QString QnTestCameraResourceSearcher::manufacture() const
{
    return QnTestCameraResource::MANUFACTURE;
}

QnResourcePtr QnTestCameraResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

