#ifdef ENABLE_ONVIF

#include "onvif_resource_searcher.h"

#include <QtCore/QUrlQuery>

#include <nx/utils/log/log.h>
#include <nx/network/http/httptypes.h>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/dataprovider/live_stream_provider.h"

#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"

bool hasRunningLiveProvider(QnNetworkResourcePtr netRes)
{
    bool rez = false;
    netRes->lockConsumers();
    for(QnResourceConsumer* consumer: netRes->getAllConsumers())
    {
        QnLiveStreamProvider* lp = dynamic_cast<QnLiveStreamProvider*>(consumer);
        if (lp)
        {
            QnLongRunnable* lr = dynamic_cast<QnLongRunnable*>(lp);
            if (lr && lr->isRunning()) {
                rez = true;
                break;
            }
        }
    }

    netRes->unlockConsumers();
    return rez;
}

/*
*   Port list used for manual camera add
*/
static const int ONVIF_SERVICE_DEFAULT_PORTS[] =
{
    80,
    8090, //FLIR default port
    8032, // DW default port
    50080, // NEW DW cam default port
    9988 // Dahui default port
};

OnvifResourceSearcher::OnvifResourceSearcher()
{
}

OnvifResourceSearcher::~OnvifResourceSearcher()
{

}

bool OnvifResourceSearcher::isProxy() const
{
    return false;
}

QString OnvifResourceSearcher::manufacture() const
{
    return QnPlOnvifResource::MANUFACTURE;
}


QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool isSearchAction)
{
    if( !url.scheme().isEmpty() && isSearchAction)
        return QList<QnResourcePtr>();  //searching if only host is present, not specific protocol

    if (url.port() == -1)
    {
        for (uint i = 0; i < sizeof(ONVIF_SERVICE_DEFAULT_PORTS)/sizeof(int); ++i)
        {
            QUrl newUrl(url);
            newUrl.setPort(ONVIF_SERVICE_DEFAULT_PORTS[i]);
            QList<QnResourcePtr> result = checkHostAddrInternal(newUrl, auth, isSearchAction);
            if (!result.isEmpty())
                return result;
        }
        return QList<QnResourcePtr>();
    }
    else {
        return checkHostAddrInternal(url, auth, isSearchAction);
    }
}

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddrInternal(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    if (shouldStop())
        return QList<QnResourcePtr>();

    QString urlStr = url.toString();

    QList<QnResourcePtr> resList;
    QnResourceTypePtr typePtr = qnResTypePool->getResourceTypeByName(QLatin1String("ONVIF"));
    if (!typePtr)
        return resList;

    const int onvifPort = url.port(nx_http::DEFAULT_HTTP_PORT);
    QString onvifUrl(QLatin1String("onvif/device_service"));

    QString urlBase = urlStr.left(urlStr.indexOf(QLatin1String("?")));
    QnPlOnvifResourcePtr rpResource = qnResPool->getResourceByUrl(urlBase).dynamicCast<QnPlOnvifResource>();

    QnPlOnvifResourcePtr resource = createResource(rpResource ? rpResource->getTypeId() : typePtr->getId(), QnResourceParams()).dynamicCast<QnPlOnvifResource>();
    resource->setDefaultAuth(auth);
    QString deviceUrl = QString(QLatin1String("http://%1:%2/%3")).arg(url.host()).arg(onvifPort).arg(onvifUrl);
    resource->setUrl(deviceUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    // optimization. do not pull resource every time if resource already in pool
    if (rpResource)
    {
        int channel = QUrlQuery(url.query()).queryItemValue(QLatin1String("channel")).toInt();

        if (channel == 0 && !hasRunningLiveProvider(rpResource)) {
            resource->calcTimeDrift();
            if (!resource->readDeviceInformation())
                return resList; // no answer from camera
        }
        else if (rpResource->getStatus() == Qn::Offline)
            return resList; // do not add 1..N channels if resource is offline

        resource->setPhysicalId(rpResource->getPhysicalId());
        resource->update(rpResource);
        if (channel > 0)
            resource->updateToChannel(channel-1);

        auto resData = qnCommon
            ->dataPool()
            ->data(rpResource->getVendor(), rpResource->getModel());

        bool shouldAppearAsSingleChannel = resData.value<bool>(
            Qn::SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME);

        if (rpResource->getMaxChannels() > 1  && !shouldAppearAsSingleChannel) {
            resource->setGroupId(rpResource->getPhysicalId());
            resource->setGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
        }

        resList << resource;
        return resList;
    }

    resource->calcTimeDrift();
    if (shouldStop())
        return QList<QnResourcePtr>();

    if (resource->readDeviceInformation() && resource->getFullUrlInfo())
    {
        if (resource->getMAC().isNull())
            return resList; // MAC address is mandatory for manual search

        // Clarify resource type
        QString manufacturer = resource->getVendor();
        QString modelName = resource->getModel();

        const bool forceOnvif = QnPlOnvifResource::isCameraForcedToOnvif(manufacturer, modelName);
        if (!forceOnvif)
        {
            if (NameHelper::instance().isSupported(modelName))
                return resList;

            if (OnvifResourceInformationFetcher::ignoreCamera(manufacturer, modelName))
                return resList;

            if (OnvifResourceInformationFetcher::isModelSupported(manufacturer, modelName)) {
                qDebug() << "OnvifResourceInformationFetcher::findResources: skipping camera " << modelName;
                return resList;
            }
            int modelNamePos = modelName.indexOf(QLatin1String(" "));
            if (modelNamePos >= 0)
            {
                modelName = modelName.mid(modelNamePos+1);
                if (NameHelper::instance().isSupported(modelName))
                    return resList;
            }
        }

        OnvifResourceInformationFetcher fetcher;
        auto resData = qnCommon->dataPool()->data(manufacturer, modelName);
        auto manufacturerAlias = resData.value<QString>(Qn::ONVIF_VENDOR_SUBTYPE);

        manufacturer = manufacturerAlias.isEmpty() ? manufacturer : manufacturerAlias;

        QnUuid rt = fetcher.getOnvifResourceType(manufacturer, modelName);
        resource->setVendor( manufacturer );
        resource->setName( modelName );
        //QnUuid rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false);
        if (!rt.isNull() && rt != resource->getTypeId())
        {
            QnPlOnvifResourcePtr updatedResource = createResource(rt, QnResourceParams()).dynamicCast<QnPlOnvifResource>();
            updatedResource->update(resource);
            updatedResource->setTypeId(rt);
            updatedResource->setPhysicalId(resource->getPhysicalId());
            updatedResource->updateOnvifUrls(resource); // runtime resource data
            updatedResource->setTimeDrift(resource->getTimeDrift()); // runtime resource data
            resource = updatedResource;
        }

        if(!resource->getUniqueId().isEmpty())
        {
            auto maxChannels = resource->getMaxChannels();
            resource->detectVideoSourceCount();
            //if (channel > 0)
            //    resource->updateToChannel(channel-1);
            resList << resource;

            bool shouldAppearAsSingleChannel = resData
                .value<bool>(Qn::SHOULD_APPEAR_AS_SINGLE_CHANNEL_PARAM_NAME);

            // checking for multichannel encoders
            if (doMultichannelCheck && !shouldAppearAsSingleChannel)
            {
                if (resource->getMaxChannels() > 1)
                {
                    resource->setGroupId(resource->getPhysicalId());
                    resource->setGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
                }

                for (int i = 1; i < resource->getMaxChannels(); ++i)
                {
                    QnPlOnvifResourcePtr res = createResource(resource->getTypeId(), QnResourceParams()).dynamicCast<QnPlOnvifResource>();
                    res->setGroupId(resource->getPhysicalId());
                    res->setGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
                    res->setVendor( manufacturer );
                    res->setPhysicalId(resource->getPhysicalId());
                    res->update(resource);
                    res->updateToChannel(i);
                    resList << res;
                }
            }
        }
    }

    return resList;
}

void OnvifResourceSearcher::pleaseStop()
{
    QnAbstractNetworkResourceSearcher::pleaseStop();
    m_wsddSearcher.pleaseStop();
}

QnResourceList OnvifResourceSearcher::findResources()
{

    QnResourceList result;

    //Order is important! mdns should be the first to avoid creating ONVIF resource, when special is expected
    //mdnsSearcher.findResources(result, specialResourceCreator);
    if (shouldStop())
         return QnResourceList();

    m_wsddSearcher.findResources( result, discoveryMode() );

    return result;
}

QnResourcePtr OnvifResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        NX_LOG(lit("OnvifResourceSearcher::createResource: no resource type for ID = %1").arg(resourceTypeId.toString()), cl_logDEBUG1);
        return result;
    }

    //if (resourceType->getManufacture() != manufacture())
    /*
    if (!resourceType->getAllManufacturesIncludeAncessor().contains(manufacture()))
    {
        qDebug() << "OnvifResourceSearcher::createResource: manufacture " << resourceType->getManufacture()
                 << " != " << manufacture();
        return result;
    }
    */
    result = OnvifResourceInformationFetcher::createOnvifResourceByManufacture(
        resourceType->getName() == lit("ONVIF") && !params.vendor.isEmpty()
        ? params.vendor
        : resourceType->getName() ); // use name instead of manufacture to instanciate child onvif resource
    if (!result )
        return result; // not found

    result->setTypeId(resourceTypeId);

    NX_LOG(lit("OnvifResourceSearcher::createResource: create ONVIF camera resource. TypeID: %1.").arg(resourceTypeId.toString()), cl_logDEBUG1);

    //result->deserialize(parameters);

    return result;

}

#endif //ENABLE_ONVIF
