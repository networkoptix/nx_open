#include "stardot_resource_searcher.h"

#include <QtCore/QCoreApplication>

#include <QtNetwork/QUdpSocket>

#include "stardot_resource.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "core/resource/camera_resource.h"

static const int CL_BROAD_CAST_RETRY = 1;
static const int STARDOT_DISCOVERY_PORT = 7364;

extern QString getValueFromString(const QString& line);

QnStardotResourceSearcher::QnStardotResourceSearcher()
{
}


QString QnStardotResourceSearcher::manufacture() const
{
    return QLatin1String(QnStardotResource::MANUFACTURE);
}

// returns all available devices
QnResourceList QnStardotResourceSearcher::findResources()
{
    QnResourceList result;

    foreach (QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        if (shouldStop())
            return QnResourceList();

        QUdpSocket sock;

        if (!bindToInterface(sock, iface))
            continue;

        // sending broadcast
        QByteArray datagram;
        datagram.append('\0');
        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock.writeDatagram(datagram.data(), datagram.size(),QHostAddress::Broadcast, STARDOT_DISCOVERY_PORT);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);

        }

        // collecting response
        QTime time;
        time.start();
        QnSleep::msleep(300); // to avoid 100% cpu usage
        //while(time.elapsed()<150)
        {
            while (sock.hasPendingDatagrams())
            {
                QByteArray datagram;
                datagram.resize(sock.pendingDatagramSize());

                QHostAddress sender;
                quint16 senderPort;

                sock.readDatagram(datagram.data(), datagram.size(),    &sender, &senderPort);

                if (senderPort != STARDOT_DISCOVERY_PORT || !datagram.startsWith("StarDot")) // minimum response size
                    continue;

                int idPos = datagram.indexOf("id=");
                if (idPos < 0)
                    continue;
                idPos += 3;

                QByteArray mac = datagram.mid(idPos, 6) + datagram.mid(idPos+7, 6);

                int modelStart = 0; //datagram.indexOf('/');
                int modelEnd = datagram.indexOf(' ');
                if (modelStart == -1 || modelEnd == -1)
                    continue;
                QByteArray model = datagram.mid(modelStart, modelEnd - modelStart);

                int versionPos = datagram.indexOf("Version");
                QByteArray firmware;
                if (versionPos >= 0) {
                    int firmwareStart = datagram.indexOf(' ', versionPos);
                    int firmwareEnd = datagram.indexOf(' ', firmwareStart+1);
                    if (firmwareStart > 0 && firmwareEnd > 0)
                        firmware = datagram.mid(firmwareStart+1, firmwareEnd - firmwareStart-1);
                }


                // in any case let's HTTP do it's job at very end of discovery
                QnStardotResourcePtr resource( new QnStardotResource() );
                //resource->setName("AVUNKNOWN");
                QnId typeId = qnResTypePool->getResourceTypeId(lit(QnStardotResource::MANUFACTURE), lit("STARDOT_COMMON"));
                if (!typeId.isValid())
                    continue;
                resource->setTypeId(typeId);

                resource->setHostAddress(sender.toString(), QnDomainMemory);
                resource->setMAC(lit((const char*)mac));
                resource->setDiscoveryAddr(iface.address);
                resource->setModel(lit((const char*)model));
                resource->setName(lit((const char*)model));
                
                int delimPos = model.indexOf('/');
                if (delimPos >= 0)
                {
                    QByteArray shortModel = model.mid(delimPos+1);
                    if (shortModel.startsWith("NetCam"))
                        shortModel = shortModel .mid(6);
                    resource->setName(QString(lit("Stardot-%1")).arg(lit((const char*)shortModel)));
                }
                
                resource->setFirmware(lit((const char*)firmware));


                bool need_to_continue = false;
                foreach(QnResourcePtr res, result)
                {
                    if (res->getUniqueId() == resource->getUniqueId())
                    {
                        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();
                        if (isNewDiscoveryAddressBetter(net_res->getHostAddress(), iface.address.toString(), net_res->getDiscoveryAddr().toString()))
                            net_res->setDiscoveryAddr(iface.address);
                    
                        need_to_continue = true; //already has such
                        break;
                    }

                }

                if (need_to_continue)
                    continue;

                result.push_back(resource);
            }

            //QnSleep::msleep(2); // to avoid 100% cpu usage

        }

    }
    return result;

}

QnStardotResourceSearcher& QnStardotResourceSearcher::instance()
{
    static QnStardotResourceSearcher inst;
    return inst;
}

QnResourcePtr QnStardotResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    result = QnVirtualCameraResourcePtr(new QnStardotResource());
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Stardot camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;
}


QList<QnResourcePtr> QnStardotResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
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
    QString model = QString(QLatin1String(downloadFile(status, QLatin1String("get?model"), host, port, timeout, auth)));

    if (model.length()==0)
        return QList<QnResourcePtr>();

    QString modelRelease = QString(QLatin1String(downloadFile(status, QLatin1String("get?model=releasename"), host, port, timeout, auth)));


    if (modelRelease!=model)
    {
        //this camera supports release name
        model = modelRelease;
    }
    else
    {
        QString modelFull = QString(QLatin1String(downloadFile(status, QLatin1String("get?model=fullname"), host, port, timeout, auth)));

        if (modelFull.length())        
            model = modelFull;
    }

    model = getValueFromString(model);

    if (model.isEmpty())
        return QList<QnResourcePtr>();

    QnId rt = qnResTypePool->getResourceTypeId(manufacture(), model);
    if (!rt.isValid())
        return QList<QnResourcePtr>();



    QString mac = QString(QLatin1String(downloadFile(status, QLatin1String("get?mac"), host, port, timeout, auth)));
    mac = getValueFromString(mac);

    if (mac.isEmpty())
        return QList<QnResourcePtr>();

    QnStardotResourcePtr res(new QnStardotResource());

    res->setTypeId(rt);
    res->setName(model);
    res->setModel(model);
    res->setMAC(mac);
    res->setHostAddress(host, QnDomainMemory);
    res->setAuth(auth);

    QList<QnResourcePtr> resList;
    resList << res;
    return resList;
}
