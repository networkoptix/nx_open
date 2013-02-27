#include "vmax480_resource_searcher.h"
#include "vmax480_resource.h"
#include "utils/common/sleep.h"
#include "utils/network/simple_http_client.h"


extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);

extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QHostAddress groupAddress(QLatin1String("239.255.255.250"));


//====================================================================
QnPlVmax480ResourceSearcher::QnPlVmax480ResourceSearcher()
{

}

QnPlVmax480ResourceSearcher::~QnPlVmax480ResourceSearcher()
{
    foreach(QUdpSocket* sock, m_socketList)
        delete sock;
}

QnPlVmax480ResourceSearcher& QnPlVmax480ResourceSearcher::instance()
{
    static QnPlVmax480ResourceSearcher inst;
    return inst;
}

QUdpSocket* QnPlVmax480ResourceSearcher::sockByName(const QnInterfaceAndAddr& iface)
{
    QMap<QString, QUdpSocket*>::iterator it = m_socketList.find(iface.address.toString());
    if (it == m_socketList.end())
    {
        QUdpSocket* sock = new QUdpSocket();
        if (!bindToInterface(*sock, iface,1900, QUdpSocket::ReuseAddressHint))
        {
            delete sock;
            return 0;
        }

        if (!multicastJoinGroup(*sock, groupAddress, iface.address))
        {
            delete sock;
            return 0;
        }

        m_socketList.insert(iface.address.toString(), sock);

        return sock;
    }
    return it.value();
}

QnResourceList QnPlVmax480ResourceSearcher::findResources(void)
{
    
    QnResourceList result;
    return result;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        QUdpSocket* sock = sockByName(iface);

        QString tmp = iface.address.toString();
        
        
        if (!sock)
            continue;



        /*
        QByteArray requestDatagram;
        socket.writeDatagram(requestDatagram.data(), requestDatagram.size(), groupAddress, 1900);
        **/

        while(sock->hasPendingDatagrams())
        {
            QByteArray reply;
            reply.resize(sock->pendingDatagramSize());

            QHostAddress sender;
            quint16 senderPort;
            sock->readDatagram(reply.data(), reply.size(),    &sender, &senderPort);

            int index = reply.indexOf("Upnp-DW-VF");

            if (index < 0 || index + 25 > reply.size())
                continue;

            index += 10;

            int index2 = reply.indexOf("-", index);
            if (index2 < 0)
                continue;

            QByteArray channelsstr = reply.mid(index, index2 - index);

            QString name = QString(QLatin1String("DW-VF")) + QString(QLatin1String(channelsstr));

            int channles = channelsstr.toInt();

            index = index2 + 1;
            index2 = reply.indexOf(":", index);

            if (index2 < 0)
                continue;


            

            QByteArray macstr = reply.mid(index, index2 - index);

            QString mac = QnMacAddress(QString(QLatin1String(macstr))).toString();
            QString host = sender.toString();



            QAuthenticator auth;
            auth.setUser(QLatin1String("admin"));

            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
            if (!rt.isValid())
                continue;



            for (int i = 0; i < channles; ++i)
            {
                QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

                resource->setTypeId(rt);
                resource->setName(name);
                (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
                resource->setMAC(mac);

                resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(9010).arg(i+1));
                resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
                resource->setDiscoveryAddr(iface.address);
                resource->setAuth(auth);

                result << resource;
            }


        }
    }


    return result;
}

/*
QnResourceList QnPlVmax480ResourceSearcher::findResources(void)
{
    QnResourceList result;

    QFile file(QLatin1String("c:/vmax.txt")); // Create a file handle for the file named
    if (!file.exists())
        return result;

    QString line;

    if (!file.open(QIODevice::ReadOnly)) // Open the file
        return result;

    QTextStream stream(&file); // Set the stream to read from myFile

    QnInterfaceAndAddr iface = getAllIPv4Interfaces().first();

    while(1)
    {
        line = stream.readLine(); // this reads a line (QString) from the file

        if (line.trimmed().isEmpty())
            break;

        QStringList params = line.split(QLatin1Char(';'), QString::SkipEmptyParts);

        QString name = params[0];
        QString mac =  params[1];
        QString host =  params[2];
        QString port =  params[3];
        QAuthenticator auth;
        auth.setUser(QLatin1String("admin"));

        for (int i = 0; i < 8; ++i)
        {
            QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );
            QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);

            resource->setTypeId(rt);
            resource->setName(name);
            (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
            resource->setMAC(mac);
            resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(port).arg(i+1));
            resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
            resource->setDiscoveryAddr(iface.address);
            resource->setAuth(auth);

            result << resource;
        }

    }

    return result;
}
/**/

QnResourcePtr QnPlVmax480ResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr( new QnPlVmax480Resource() );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create test camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}

QList<QnResourcePtr> QnPlVmax480ResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    QList<QnResourcePtr> result;

    

    CLHttpStatus status;
    QByteArray reply = downloadFile(status, QLatin1String("dvrdevicedesc.xml"),  url.host(), 49152, 1000, auth);

    if (status != CL_HTTP_SUCCESS)
        return result;


    int index = reply.indexOf("Upnp-DW-VF");

    if (index < 0 || index + 25 > reply.size())
        return result;

    index += 10;

    int index2 = reply.indexOf("-", index);
    if (index2 < 0)
        return result;

    QByteArray channelsstr = reply.mid(index, index2 - index);

    QString name = QString(QLatin1String("DW-VF")) + QString(QLatin1String(channelsstr));

    int channles = channelsstr.toInt();

    index = index2 + 1;
    index2 = reply.indexOf("<", index);

    if (index2 < 0)
        return result;


    QByteArray macstr = reply.mid(index, index2 - index);

    QString mac = QnMacAddress(QString(QLatin1String(macstr))).toString();
    QString host = url.host();


    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), name);
    if (!rt.isValid())
        return result;



    for (int i = 0; i < channles; ++i)
    {
        QnPlVmax480ResourcePtr resource ( new QnPlVmax480Resource() );

        resource->setTypeId(rt);
        resource->setName(name);
        (resource.dynamicCast<QnPlVmax480Resource>())->setModel(name);
        resource->setMAC(mac);

        resource->setUrl(QString(QLatin1String("http://%1:%2?channel=%3")).arg(host).arg(9010).arg(i+1));
        resource->setPhysicalId(QString(QLatin1String("%1_%2")).arg(mac).arg(i+1));
        resource->setAuth(auth);

        result << resource;
    }


    return result;
}

QString QnPlVmax480ResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlVmax480Resource::MANUFACTURE);
}


