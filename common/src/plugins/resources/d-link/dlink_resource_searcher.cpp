#include "dlink_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "dlink_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"

unsigned char request[] = {0xfd, 0xfd, 0x06, 0x00, 0xa1, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
QByteArray barequest(reinterpret_cast<char *>(request), sizeof(request));

char DCS[] = {'D', 'C', 'S', '-'};

extern QString getValueFromString(const QString& line);

#define CL_BROAD_CAST_RETRY 1

QnPlDlinkResourceSearcher::QnPlDlinkResourceSearcher()
{
}

QnPlDlinkResourceSearcher& QnPlDlinkResourceSearcher::instance()
{
    static QnPlDlinkResourceSearcher inst;
    return inst;
}

QnResourcePtr QnPlDlinkResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr( new QnPlDlinkResource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create DLink camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;

    result->deserialize(parameters);

    return result;
}

QnResourceList QnPlDlinkResourceSearcher::findResources()
{
    QnResourceList result;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {

        if (shouldStop())
            return QnResourceList();

        QUdpSocket sock;

        if (!sock.bind(iface.address, 0))
            continue;

        // sending broadcast

        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock.writeDatagram(barequest.data(), barequest.size(),QHostAddress::Broadcast, 62976);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);
        }

        // collecting response
        QTime time;
        time.start();

        QnSleep::msleep(150);
        while (sock.hasPendingDatagrams())
        {
            QByteArray datagram;
            datagram.resize(sock.pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;

            sock.readDatagram(datagram.data(), datagram.size(),    &sender, &senderPort);

            if (senderPort != 62976 || datagram.size() < 32) // minimum response size
                continue;

            QString name  = QLatin1String("DCS-");

            int iqpos = datagram.indexOf("DCS-");

            if (iqpos<0)
                continue;

            iqpos+=name.length();

            while (iqpos < datagram.size() && datagram[iqpos] != (char)0)
            {
                name += QLatin1Char(datagram[iqpos]);
                ++iqpos;
            }

            const unsigned char* data = (unsigned char*)(datagram.data());

            unsigned char mac[6];
            memcpy(mac, data + 6, 6);

            QString smac = MACToString(mac);


            bool haveToContinue = false;
            foreach(QnResourcePtr res, result)
            {
                QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();

                if (net_res->getMAC().toString() == smac)
                {
                    haveToContinue = true;
                    break; // already found;
                }
            }

            if (haveToContinue)
                break;


            QnPlDlinkResourcePtr resource ( new QnPlDlinkResource() );

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
            if (!rt.isValid())
                continue;

            resource->setTypeId(rt);
            resource->setName(name);
            resource->setModel(name);
            resource->setMAC(smac);
            resource->setHostAddress(sender.toString(), QnDomainMemory);

            resource->setDiscoveryAddr(iface.address);

            result.push_back(resource);

        }

    }

    return result;
}

QString QnPlDlinkResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlDlinkResource::MANUFACTURE);
}


QList<QnResourcePtr> QnPlDlinkResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(doMultichannelCheck)
    QString host = url.host();
    int port = url.port();
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port

    int timeout = 2000;


    if (port < 0)
        port = 80;

    CLHttpStatus status;
    QString response = QString(QLatin1String(downloadFile(status, QLatin1String("common/info.cgi"), host, port, timeout, auth)));

    if (response.length()==0)
        return QList<QnResourcePtr>();

    QStringList lines = response.split(QLatin1String("\r\n"), QString::SkipEmptyParts);


    QString name;
    QString mac;

    foreach(QString line, lines)
    {
        if (line.contains(QLatin1String("name=")))
        {
            name = getValueFromString(line);
        }
        else if (line.contains(QLatin1String("macaddr=")))
        {
            mac = getValueFromString(line);
            mac.replace(QLatin1Char(':'),QLatin1Char('-'));
        }

    }

    if (mac.isEmpty() || name.isEmpty())
        return QList<QnResourcePtr>();


    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return QList<QnResourcePtr>();

    QnNetworkResourcePtr resource ( new QnPlDlinkResource() );

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(mac);
    (resource.dynamicCast<QnPlDlinkResource>())->setModel(name);
    resource->setHostAddress(host, QnDomainMemory);
    resource->setAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;
    return result;
}

