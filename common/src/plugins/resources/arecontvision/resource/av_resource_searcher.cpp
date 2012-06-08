#include "av_resource_searcher.h"

#include <QtCore/QCoreApplication>

#include <QtNetwork/QUdpSocket>

#include "av_resource.h"
#include "../tools/AVJpegHeader.h"
#include "utils/network/nettools.h"
#include "utils/common/sleep.h"
#include "core/resource/camera_resource.h"

#ifdef Q_OS_LINUX
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif

#define CL_BROAD_CAST_RETRY 1

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

    QList<QHostAddress> ipaddrs = getAllIPv4Addresses();
#if 0
    CL_LOG(cl_logDEBUG1)
    {
        QString log;
        QTextStream(&log) << "CLAreconVisionDevice::findDevices  found " << ipaddrs.size() << " adapter(s) with IPV4";
        cl_log.log(log, cl_logDEBUG1);

        for (int i = 0; i < ipaddrs.size();++i)
        {
            QString slog;
            QTextStream(&slog) << ipaddrs.at(i).toString();
            cl_log.log(slog, cl_logDEBUG1);
        }
    }
#endif

#ifdef Q_OS_LINUX
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        cl_log.log(cl_logWARNING, "QnPlArecontResourceSearcher::findResources(): Can't get interfaces list: %s", strerror(errno));
        return result;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        family = ifa->ifa_addr->sa_family;

        if (family != AF_INET)
            continue;

        QUdpSocket sock;
        sock.bind(0);

        int res = setsockopt(sock.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, ifa->ifa_name, strlen(ifa->ifa_name));
        if (res != 0)
        {
            cl_log.log(cl_logWARNING, "QnPlArecontResourceSearcher::findResources(): Can't bind to interface %s: %s", ifa->ifa_name, strerror(errno));
            continue;
        }

#else // elif defined Q_OS_WIN
    for (int i = 0; i < ipaddrs.size();++i)
    {
        QUdpSocket sock;

        if (!sock.bind(ipaddrs.at(i), 0))
           continue;
#endif


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

                sock.readDatagram(datagram.data(), datagram.size(),	&sender, &senderPort);

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
                resource->setTypeId(qnResTypePool->getResourceTypeId(QnPlAreconVisionResource::MANUFACTURE, "ArecontVision_Abstract"));

                if (resource==0)
                    continue;

                resource->setHostAddress(sender, QnDomainMemory);
                resource->setMAC(mac);

#ifdef Q_OS_LINUX
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s != 0) {
                    cl_log.log(cl_logWARNING, "QnPlArecontResourceSearcher::findResources(): getnameinfo() failed: %s", strerror(errno));
                }
                resource->setDiscoveryAddr(QHostAddress(host));
#elif defined Q_OS_WIN
                resource->setDiscoveryAddr(ipaddrs.at(i));
#endif

                resource->setName("ArecontVision_Abstract");


                bool need_to_continue = false;
                foreach(QnResourcePtr res, result)
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
    return QnResourcePtr(0);
}
