#ifdef ENABLE_ARECONT

#include "av_resource_searcher.h"

#include <QtCore/QCoreApplication>

#include "av_resource.h"
#include "../tools/AVJpegHeader.h"
#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include "nx/utils/log/log.h"
#include "core/resource/camera_resource.h"
#include "plugins/resource/archive_camera/archive_camera.h"

#include "plugins/resource/mdns/mdns_resource_searcher.h"
#include "av_panoramic.h"
#include "av_singesensor.h"
#include <nx/network/socket.h>
#include <nx/utils/random.h>
#include "core/resource_management/resource_pool.h"

#define CL_BROAD_CAST_RETRY 1

extern QString getValueFromString(const QString& line);

QnPlArecontResourceSearcher::QnPlArecontResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
    // everything related to Arecont must be initialized here
    AVJpeg::Header::Initialize("ArecontVision", "CamLabs", "ArecontVision");
}


QString QnPlArecontResourceSearcher::manufacture() const
{
    return QnPlAreconVisionResource::MANUFACTURE;
}

QnNetworkResourcePtr
QnPlArecontResourceSearcher::findResourceHelper(const MacArray &mac,
                                                const nx::network::SocketAddress &addr)
{
    QnNetworkResourcePtr result;
    QString macAddress = nx::network::QnMacAddress(mac.data()).toString();
    auto rpRes = resourcePool()->getResourceByUniqueId<QnPlAreconVisionResource>(macAddress);

    if (rpRes)
        result = QnNetworkResourcePtr(QnPlAreconVisionResource::createResourceByName(rpRes->getModel()));

    if (result)
    {
        result->setMAC(nx::network::QnMacAddress(mac.data()));
        result->setHostAddress(addr.address.toString());
        result.dynamicCast<QnPlAreconVisionResource>()->setModel(rpRes->getModel());
        result->setName(rpRes->getName());
        result->setFlags(rpRes->flags());
    }
    else
    {
        QString model;
        QString model_release;

        result = QnNetworkResourcePtr(new QnPlAreconVisionResource());
        result->setMAC(nx::network::QnMacAddress(mac.data()));
        result->setHostAddress(addr.address.toString());

        if (!result->getParamPhysical(lit("model"), model))
            return QnNetworkResourcePtr(0);

        if (!result->getParamPhysical(lit("model=releasename"), model_release))
            return QnNetworkResourcePtr(0);

        if (model_release != model) {
            //this camera supports release name
            model = model_release;
        }
        else
        {
            //old camera; does not support release name; but must support fullname
            if (result->getParamPhysical(lit("model=fullname"), model_release))
                model = model_release;
        }

        result = QnNetworkResourcePtr(QnPlAreconVisionResource::createResourceByName(model));
        if (result)
        {
            result->setName(model);
            (result.dynamicCast<QnPlAreconVisionResource>())->setModel(model);
            result->setMAC(nx::network::QnMacAddress(mac.data()));
            result->setHostAddress(addr.address.toString());
        }
        else
        {
            NX_LOG( lit("Found unknown resource! %1").arg(model), cl_logWARNING);
            return QnNetworkResourcePtr(0);
        }
    }

    return result;
}

// returns all available devices
QnResourceList QnPlArecontResourceSearcher::findResources()
{
    QnResourceList result;

    for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
    {
        if (shouldStop())
            return QnResourceList();

        std::unique_ptr<nx::network::AbstractDatagramSocket> sock( nx::network::SocketFactory::createDatagramSocket() );

        if (!sock->bind(iface.address.toString(), 0))
            continue;

        // sending broadcast
        QByteArray datagram = "Arecont_Vision-AV2000\1";
        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock->sendTo(datagram.data(), datagram.size(), nx::network::BROADCAST_ADDRESS, 69);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);

        }

        // collecting response
        QnSleep::msleep(150); // to avoid 100% cpu usage
        {
            while (sock->hasData())
            {
                QByteArray datagram;
                datagram.resize( nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE );

                nx::network::SocketAddress remoteEndpoint;
                int readed = sock->recvFrom(datagram.data(), datagram.size(), &remoteEndpoint);

                if (remoteEndpoint.port!=69 || readed < 32) // minimum response size
                    continue;

                const unsigned char* data = (unsigned char*)(datagram.data());
                if (memcmp(data, "Arecont_Vision-AV2000", 21 )!=0)
                    continue; // this responde id not from arecont camera

                MacArray mac;
                memcpy(mac.data(), data + 22,6);

                /*/
                QString smac = nx::network::MACToString(mac);


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
                bool need_to_continue = false;
                for(const QnResourcePtr& res: result)
                {
                    if (memcmp(res->getUniqueId().toLatin1().constData(), mac.data(), 6) == 0)
                    {
                        need_to_continue = true; //already has such
                        break;
                    }
                }
                if (need_to_continue)
                    continue;

                QnNetworkResourcePtr resultRes = findResourceHelper(mac, remoteEndpoint);
                if (resultRes)
                    result.push_back(resultRes);
            }
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
        QnSleep::msleep(nx::utils::random::number(0, 50));
    }
    return rez;
}

QList<QnResourcePtr> QnPlArecontResourceSearcher::checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction )
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    QString host = url.host();
    int port = url.port(80);
    if (host.isEmpty())
        host = url.toString(); // in case if url just host address without protocol and port
    QString devUrl = QString(lit("http://%1:%2")).arg(url.host()).arg(url.port(80));
    int timeout = 2000;

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
    {
        if (model.left(2).toLower() == lit("av"))
        {
            auto unprefixed = model.mid(2);
            rt = qnResTypePool->getLikeResourceTypeId(manufacture(), unprefixed);
        }
    }

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
    res->setMAC(nx::network::QnMacAddress(mac));
    if (port == 80)
        res->setHostAddress(host);
    else
        res->setUrl(devUrl);
    res->setDefaultAuth(auth);

    QList<QnResourcePtr> resList;
    resList << res;
    return resList;
}

#endif
