#ifdef ENABLE_DLINK

#include "dlink_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "dlink_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "utils/network/socket.h"

unsigned char request[] = {0xfd, 0xfd, 0x06, 0x00, 0xa1, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
QByteArray barequest(reinterpret_cast<char *>(request), sizeof(request));

char DCS[] = {'D', 'C', 'S', '-'};

extern QString getValueFromString(const QString& line);

#define CL_BROAD_CAST_RETRY 1

QnPlDlinkResourceSearcher::QnPlDlinkResourceSearcher()
{
}

QnResourcePtr QnPlDlinkResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
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

    qDebug() << "Create DLink camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;

    //result->deserialize(parameters);

    return result;
}

QnResourceList QnPlDlinkResourceSearcher::findResources()
{
    QnResourceList result;

    std::unique_ptr<AbstractDatagramSocket> recvSocket( SocketFactory::createDatagramSocket() );
#ifdef Q_OS_WIN
    if (!recvSocket->bind(SocketAddress( HostAddress::anyHost, 0 )))
#else
	if (!recvSocket->bind(BROADCAST_ADDRESS, 0))
#endif
        return QnResourceList();

    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
    {

        if (shouldStop())
            return QnResourceList();

        std::unique_ptr<AbstractDatagramSocket> sock( SocketFactory::createDatagramSocket() );
        sock->setReuseAddrFlag(true);

        if (!sock->bind(iface.address.toString(), recvSocket->getLocalAddress().port))
            continue;

        // sending broadcast

        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock->sendTo(barequest.data(), barequest.size(), BROADCAST_ADDRESS, 62976);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);
        }

        // collecting response
        QnSleep::msleep(150);
        while (recvSocket->hasData())
        {
            QByteArray datagram;
            datagram.resize( AbstractDatagramSocket::MAX_DATAGRAM_SIZE );

            QString sender;
            quint16 senderPort;

            int readed = recvSocket->recvFrom(datagram.data(), datagram.size(),    sender, senderPort);

            if (senderPort != 62976 || readed < 32) // minimum response size
                continue;

            QString name  = QLatin1String("DCS-");

            int iqpos = datagram.indexOf("DCS-");

            if (iqpos<0)
                continue;

            iqpos+=name.length();

            while (iqpos < readed && datagram[iqpos] != (char)0)
            {
                name += QLatin1Char(datagram[iqpos]);
                ++iqpos;
            }

            const unsigned char* data = (unsigned char*)(datagram.data());

            unsigned char mac[6];
            memcpy(mac, data + 6, 6);

            QString smac = MACToString(mac);


            bool haveToContinue = false;
            for(const QnResourcePtr& res: result)
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

            QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
            if (rt.isNull())
                continue;

            resource->setTypeId(rt);
            resource->setName(name);
            resource->setModel(name);
            resource->setMAC(QnMacAddress(smac));
            resource->setHostAddress(sender);

            result.push_back(resource);

        }

    }

    return result;
}

QString QnPlDlinkResourceSearcher::manufacture() const
{
    return QnPlDlinkResource::MANUFACTURE;
}


QList<QnResourcePtr> QnPlDlinkResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

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

    for(const QString& line: lines)
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


    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (rt.isNull())
        return QList<QnResourcePtr>();

    QnNetworkResourcePtr resource ( new QnPlDlinkResource() );

    resource->setTypeId(rt);
    resource->setName(name);
    resource->setMAC(QnMacAddress(mac));
    (resource.dynamicCast<QnPlDlinkResource>())->setModel(name);
    resource->setHostAddress(host);
    resource->setDefaultAuth(auth);

    //resource->setDiscoveryAddr(iface.address);
    QList<QnResourcePtr> result;
    result << resource;
    return result;
}

#endif // ENABLE_DLINK
