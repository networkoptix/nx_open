#ifdef ENABLE_STARDOT

#include "stardot_resource_searcher.h"

#include <QtCore/QCoreApplication>

#include "stardot_resource.h"
#include <nx/network/nettools.h>
#include "utils/common/sleep.h"
#include "core/resource/camera_resource.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <common/static_common_module.h>

static const int CL_BROAD_CAST_RETRY = 1;
static const int STARDOT_DISCOVERY_PORT = 7364;

extern QString getValueFromString(const QString& line);

QnStardotResourceSearcher::QnStardotResourceSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
}


QString QnStardotResourceSearcher::manufacture() const
{
    return QnStardotResource::MANUFACTURE;
}

// returns all available devices
QnResourceList QnStardotResourceSearcher::findResources()
{
    QnResourceList result;

    for (const nx::network::QnInterfaceAndAddr& iface: nx::network::getAllIPv4Interfaces())
    {
        if (shouldStop())
            return QnResourceList();

        std::unique_ptr<nx::network::AbstractDatagramSocket> sock( nx::network::SocketFactory::createDatagramSocket() );

        //if (!bindToInterface(sock, iface))
        //    continue;
        if (!sock->bind(iface.address.toString(), 0))
            continue;

        // sending broadcast
        QByteArray datagram;
        datagram.append('\0');
        for (int r = 0; r < CL_BROAD_CAST_RETRY; ++r)
        {
            sock->sendTo(datagram.data(), datagram.size(), nx::network::BROADCAST_ADDRESS, STARDOT_DISCOVERY_PORT);

            if (r!=CL_BROAD_CAST_RETRY-1)
                QnSleep::msleep(5);

        }

        // collecting response
        QnSleep::msleep(300); // to avoid 100% cpu usage
        {
            while (sock->hasData())
            {
                QByteArray datagram;
                datagram.resize(nx::network::AbstractDatagramSocket::MAX_DATAGRAM_SIZE);

                nx::network::SocketAddress senderEndpoint;
                int readed = sock->recvFrom(datagram.data(), datagram.size(), &senderEndpoint);
                if (readed < 1)
                    continue;
                datagram = datagram.left(readed);

                if (senderEndpoint.port != STARDOT_DISCOVERY_PORT || !datagram.startsWith("StarDot")) // minimum response size
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
                QnUuid typeId = qnResTypePool->getResourceTypeId(QnStardotResource::MANUFACTURE, lit("STARDOT_COMMON"));
                if (typeId.isNull())
                    continue;

                QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), QLatin1String(model));
                if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
                    continue; // model forced by ONVIF

                resource->setTypeId(typeId);

                resource->setHostAddress(senderEndpoint.address.toString());
                resource->setMAC(nx::network::QnMacAddress(mac));
                resource->setModel(QLatin1String(model));
                resource->setName(QLatin1String(model));

                int delimPos = model.indexOf('/');
                if (delimPos >= 0)
                {
                    QByteArray shortModel = model.mid(delimPos+1);
                    if (shortModel.startsWith("NetCam"))
                        shortModel = shortModel .mid(6);
                    resource->setName(QString(lit("Stardot-%1")).arg(QLatin1String(shortModel)));
                }

                resource->setFirmware(QLatin1String(firmware));


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

QnResourcePtr QnStardotResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
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

    qDebug() << "Create Stardot camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;
    //result->deserialize(parameters);

    return result;
}


QList<QnResourcePtr> QnStardotResourceSearcher::checkHostAddr(const nx::utils::Url& url, const QAuthenticator& auth, bool isSearchAction)
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

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture(), model);
    if (rt.isNull())
        return QList<QnResourcePtr>();



    QString mac = QString(QLatin1String(downloadFile(status, QLatin1String("get?mac"), host, port, timeout, auth)));
    mac = getValueFromString(mac);

    if (mac.isEmpty())
        return QList<QnResourcePtr>();

    QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), model);
    if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
        return QList<QnResourcePtr>(); // model forced by ONVIF


    QnStardotResourcePtr res(new QnStardotResource());

    res->setTypeId(rt);
    res->setName(model);
    res->setModel(model);
    res->setMAC(nx::network::QnMacAddress(mac));
    res->setHostAddress(host);
    res->setDefaultAuth(auth);

    QList<QnResourcePtr> resList;
    resList << res;
    return resList;
}

#endif // #ifdef ENABLE_STARDOT
