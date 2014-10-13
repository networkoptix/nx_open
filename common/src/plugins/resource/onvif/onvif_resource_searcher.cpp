#ifdef ENABLE_ONVIF

#include "onvif_resource_searcher.h"

#include <QtCore/QUrlQuery>

#include <utils/common/log.h>
#include <utils/network/http/httptypes.h>

#include "core/resource/camera_resource.h"
#include "core/resource_management/resource_pool.h"
#include "core/dataprovider/live_stream_provider.h"

#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"

bool hasRunningLiveProvider(QnNetworkResourcePtr netRes)
{
    bool rez = false;
    netRes->lockConsumers();
    foreach(QnResourceConsumer* consumer, netRes->getAllConsumers())
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
    8032, // DW default port
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

    QnPlOnvifResourcePtr resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());
    resource->setTypeId(typePtr->getId());
    resource->setAuth(auth);
    QString deviceUrl = QString(QLatin1String("http://%1:%2/%3")).arg(url.host()).arg(onvifPort).arg(onvifUrl);
    resource->setUrl(deviceUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    // optimization. do not pull resource every time if resource already in pool
    QString urlBase = urlStr.left(urlStr.indexOf(QLatin1String("?")));
    QnPlOnvifResourcePtr rpResource = qnResPool->getResourceByUrl(urlBase).dynamicCast<QnPlOnvifResource>();
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
        resource->update(rpResource, true);
        if (channel > 0)
            resource->updateToChannel(channel-1);

        if (rpResource->getMaxChannels() > 1 ) {
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
        // Clarify resource type
        QString fullName = resource->getName();
        int manufacturerPos = fullName.indexOf(QLatin1String("-"));
        QString manufacturer = fullName.mid(0,manufacturerPos).trimmed();
        QString modelName = fullName.mid(manufacturerPos+1).trimmed();

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

        OnvifResourceInformationFetcher fetcher;
        QnUuid rt = fetcher.getOnvifResourceType(manufacturer, modelName);
        resource->setVendor( manufacturer );
        resource->setName( modelName );
        //QnUuid rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false);
        if (!rt.isNull())
            resource->setTypeId(rt);

        if(!resource->getUniqueId().isEmpty())
        {
            resource->detectVideoSourceCount();
            //if (channel > 0)
            //    resource->updateToChannel(channel-1);
            resList << resource;
            
            // checking for multichannel encoders
            if (doMultichannelCheck)
            {
                if (resource->getMaxChannels() > 1)
                {
                    resource->setGroupId(resource->getPhysicalId());
                    resource->setGroupName(resource->getModel() + QLatin1String(" ") + resource->getHostAddress());
                }

                for (int i = 1; i < resource->getMaxChannels(); ++i) 
                {
                    QnPlOnvifResourcePtr res(new QnPlOnvifResource());
                    res->setVendor( manufacturer );
                    res->setPhysicalId(resource->getPhysicalId());
                    res->update(resource, true);
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

    m_wsddSearcher.findResources(result);

    return result;
}

QnResourcePtr OnvifResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
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
    
    result = OnvifResourceInformationFetcher::createOnvifResourceByManufacture(resourceType->getName()); // use name instead of manufacture to instanciate child onvif resource
    if (!result )
        return result; // not found

    result->setTypeId(resourceTypeId);

    NX_LOG(lit("OnvifResourceSearcher::createResource: create ONVIF camera resource. TypeID: %1.").arg(resourceTypeId.toString()), cl_logDEBUG1);

    //result->deserialize(parameters);

    return result;

}

#endif //ENABLE_ONVIF
