#ifdef ENABLE_ARECONT

#include "av_resource_searcher.h"

#include <QtCore/QCoreApplication>

#include "av_resource.h"
#include "../tools/AVJpegHeader.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "core/resource/camera_resource.h"

#include "plugins/resource/mdns/mdns_resource_searcher.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include "utils/network/socket.h"

#define CL_BROAD_CAST_RETRY 1

extern QString getValueFromString(const QString& line);

QnPlArecontResourceSearcher::QnPlArecontResourceSearcher()
{
    // everything related to Arecont must be initialized here
    AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");
}


QString QnPlArecontResourceSearcher::manufacture() const
{
    return QnPlAreconVisionResource::MANUFACTURE;
}

// returns all available devices
QnResourceList QnPlArecontResourceSearcher::findResources()
{
    QnResourceList result;

    for (const QnInterfaceAndAddr& iface: getAllIPv4Interfaces())
    {
        if (shouldStop())
            return QnResourceList();

        std::unique_ptr<AbstractDatagramSocket> sock( SocketFactory::createDatagramSocket() );

        if (!sock->bind(iface.address.toString(), 0))
            continue;

        // sending broadcast
        QByteArray datagram = "Arecont_Vision-AV2000\1";
        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock->sendTo(datagram.data(), datagram.size(), BROADCAST_ADDRESS, 69);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);

        }

        // collecting response
        QnSleep::msleep(150); // to avoid 100% cpu usage
        {
            while (sock->hasData())
            {
                QByteArray datagram;
                datagram.resize( AbstractDatagramSocket::MAX_DATAGRAM_SIZE );

                QString sender;
                quint16 senderPort;

                int readed = sock->recvFrom(datagram.data(), datagram.size(), sender, senderPort);

                if (senderPort!=69 || readed < 32) // minimum response size
                    continue;

                const unsigned char* data = (unsigned char*)(datagram.data());
                if (memcmp(data, "Arecont_Vision-AV2000", 21 )!=0)
                    continue; // this responde id not from arecont camera

                unsigned char mac[6];
                memcpy(mac,data + 22,6);

                /*/
                QString smac = MACToString(mac);


                QString id = "AVUNKNOWN";
                int model = 0;


                int shift = 32;

                CLAreconVisionDevice* resource = 0;

                if (datagram.size() > shift + 5)
                {
                    model = (unsigned char)data[shift+2] * 256 + (unsigned char)data[shift+3]; //4
                    QString smodel;
                    smodel.setNum(model);
                    smodel = smodel;
                    id = smodel; // this is not final version of the ID; it might/must be updated later
                    resource = deviceByID(id, model);

                    if (resource)
                        resource->setName(id);
                }
                else
                {
                    // very old cam; in future need to request model seporatly
                    resource = new CLAreconVisionDevice(AVUNKNOWN);
                    resource->setName("AVUNKNOWN");

                }
                /*/

                // in any case let's HTTP do it's job at very end of discovery
                QnNetworkResourcePtr resource( new QnPlAreconVisionResource() );
                //resource->setName("AVUNKNOWN");
                resource->setTypeId(qnResTypePool->getResourceTypeId(QnPlAreconVisionResource::MANUFACTURE, QLatin1String("ArecontVision_Abstract")));

                if (resource==0)
                    continue;

                resource->setHostAddress(sender);
                resource->setMAC(QnMacAddress(mac));
                resource->setName(QLatin1String("ArecontVision_Abstract"));


                bool need_to_continue = false;
                for(const QnResourcePtr& res: result)
                {
                    if (res->getUniqueId() == resource->getUniqueId())
                    {
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

QnResourcePtr QnPlArecontResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
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

    result = QnVirtualCameraResourcePtr(QnPlAreconVisionResource::createResourceByTypeId(resourceTypeId));
    result->setTypeId(resourceTypeId);

    qDebug() << "Create arecontVision camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

    return result;
}

QByteArray downloadFileWithRetry(CLHttpStatus& status, const QString& fileName, const QString& host, int port, unsigned int timeout, const QAuthenticator& auth)
{
    QByteArray rez;
    for (int i = 0; i < 4; ++i) {
        rez = downloadFile(status, fileName, host, port, timeout, auth);
        if (status != CL_HTTP_SERVICEUNAVAILABLE)
            break;
        QnSleep::msleep(rand() % 50);
    }
    return rez;
}

QList<QnResourcePtr> QnPlArecontResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
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
    QString model = QLatin1String(downloadFileWithRetry(status, QLatin1String("get?model"), host, port, timeout, auth));

    if (model.isEmpty())
        return QList<QnResourcePtr>();

    QString modelRelease = QLatin1String(downloadFileWithRetry(status, QLatin1String("get?model=releasename"), host, port, timeout, auth));
    if (!modelRelease.isEmpty() && modelRelease != model)
    {
        // this camera supports release name
        model = modelRelease;
    }
    else
    {
        QString modelFull = QLatin1String(downloadFileWithRetry(status, QLatin1String("get?model=fullname"), host, port, timeout, auth));
        if (!modelFull.isEmpty())        
            model = modelFull;
    }

    model = getValueFromString(model);

    if (model.isEmpty())
        return QList<QnResourcePtr>();


    QnUuid rt = qnResTypePool->getLikeResourceTypeId(manufacture(), model);
    if (rt.isNull())
        return QList<QnResourcePtr>();



    QString mac = QString(QLatin1String(downloadFileWithRetry(status, QLatin1String("get?mac"), host, port, timeout, auth)));
    mac = getValueFromString(mac);

    if (mac.isEmpty())
        return QList<QnResourcePtr>();

    QnPlAreconVisionResourcePtr res(0);

    if (QnPlAreconVisionResource::isPanoramic(qnResTypePool->getResourceType(rt)))
        res = QnPlAreconVisionResourcePtr(new QnArecontPanoramicResource(model));
    else
        res = QnPlAreconVisionResourcePtr(new CLArecontSingleSensorResource(model));

    res->setTypeId(rt);
    res->setName(model);
    res->setModel(model);
    res->setMAC(QnMacAddress(mac));
    res->setHostAddress(host);
    res->setAuth(auth);

    QList<QnResourcePtr> resList;
    resList << res;
    return resList;
}

#endif
