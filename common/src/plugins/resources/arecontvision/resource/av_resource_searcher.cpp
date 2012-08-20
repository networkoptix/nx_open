#include "av_resource_searcher.h"

#include <QtCore/QCoreApplication>

#include <QtNetwork/QUdpSocket>

#include "av_resource.h"
#include "../tools/AVJpegHeader.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "core/resource/camera_resource.h"

#include "plugins/resources/mdns/mdns_device_searcher.h"

#define CL_BROAD_CAST_RETRY 1

QnPlArecontResourceSearcher::QnPlArecontResourceSearcher()
{
    // everything related to Arecont must be initialized here
    AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");
}


QString QnPlArecontResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlAreconVisionResource::MANUFACTURE);
}

// returns all available devices
QnResourceList QnPlArecontResourceSearcher::findResources()
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
        QByteArray datagram = "Arecont_Vision-AV2000\1";
        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock.writeDatagram(datagram.data(), datagram.size(),QHostAddress::Broadcast, 69);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);

        }

        // collecting response
        QTime time;
        time.start();
        QnSleep::msleep(150); // to avoid 100% cpu usage
        //while(time.elapsed()<150)
        {
            while (sock.hasPendingDatagrams())
            {
                QByteArray datagram;
                datagram.resize(sock.pendingDatagramSize());

                QHostAddress sender;
                quint16 senderPort;

                sock.readDatagram(datagram.data(), datagram.size(),    &sender, &senderPort);

                if (senderPort!=69 || datagram.size() < 32) // minimum response size
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
                resource->setTypeId(qnResTypePool->getResourceTypeId(QLatin1String(QnPlAreconVisionResource::MANUFACTURE), QLatin1String("ArecontVision_Abstract")));

                if (resource==0)
                    continue;

                resource->setHostAddress(sender, QnDomainMemory);
                resource->setMAC(mac);
                resource->setDiscoveryAddr(iface.address);
                resource->setName(QLatin1String("ArecontVision_Abstract"));


                bool need_to_continue = false;
                foreach(QnResourcePtr res, result)
                {
                    if (res->getUniqueId() == resource->getUniqueId())
                    {
                        QnNetworkResourcePtr net_res = res.dynamicCast<QnNetworkResource>();
                        if (isNewDiscoveryAddressBetter(net_res->getHostAddress().toString(), iface.address.toString(), net_res->getDiscoveryAddr().toString()))
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

QnPlArecontResourceSearcher& QnPlArecontResourceSearcher::instance()
{
    static QnPlArecontResourceSearcher inst;
    return inst;
}

QnResourcePtr QnPlArecontResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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

    qDebug() << "Create arecontVision camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;
}


QnResourcePtr QnPlArecontResourceSearcher::checkHostAddr(QHostAddress addr)
{
    Q_UNUSED(addr)
    return QnResourcePtr(0);
}
