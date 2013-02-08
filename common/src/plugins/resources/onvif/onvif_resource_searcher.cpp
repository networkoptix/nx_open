#include "onvif_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"
#include "core/resource_managment/resource_pool.h"
#include "core/dataprovider/live_stream_provider.h"

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

OnvifResourceSearcher::OnvifResourceSearcher():
    wsddSearcher(OnvifResourceSearcherWsdd::instance())
    //mdnsSearcher(OnvifResourceSearcherMdns::instance()),
{

}

OnvifResourceSearcher::~OnvifResourceSearcher()
{

}

OnvifResourceSearcher& OnvifResourceSearcher::instance()
{
    static OnvifResourceSearcher inst;
    return inst;
}

bool OnvifResourceSearcher::isProxy() const
{
    return false;
}

QString OnvifResourceSearcher::manufacture() const
{
    return QLatin1String(QnPlOnvifResource::MANUFACTURE);
}


QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    if (url.port() == -1)
    {
        for (uint i = 0; i < sizeof(ONVIF_SERVICE_DEFAULT_PORTS)/sizeof(int); ++i)
        {
            QUrl newUrl(url);
            newUrl.setPort(ONVIF_SERVICE_DEFAULT_PORTS[i]);
            QList<QnResourcePtr> result = checkHostAddrInternal(newUrl, auth, doMultichannelCheck);
            if (!result.isEmpty())
                return result;
        }
        return QList<QnResourcePtr>();
    }
    else {
        return checkHostAddrInternal(url, auth, doMultichannelCheck);
    }
}

QList<QnResourcePtr> OnvifResourceSearcher::checkHostAddrInternal(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    QString urlStr = url.toString();

    QList<QnResourcePtr> resList;
    QnResourceTypePtr typePtr = qnResTypePool->getResourceTypeByName(QLatin1String("ONVIF"));
    if (!typePtr)
        return resList;

    int onvifPort = url.port(80);
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
        int channel = url.queryItemValue(QLatin1String("channel")).toInt();
        
        if (channel == 0 && !hasRunningLiveProvider(rpResource)) {
            resource->calcTimeDrift();
            if (!resource->fetchAndSetDeviceInformation(true))
                return resList; // no answer from camera
        }
        else if (rpResource->getStatus() == QnResource::Offline)
            return resList; // do not add 1..N channels if resource is offline

        resource->setPhysicalId(rpResource->getPhysicalId());
        resource->update(rpResource, true);
        if (channel > 0)
            resource->updateToChannel(channel-1);

        QString userName = resource->getAuth().user();

        resList << resource;
        return resList;
    }

    resource->calcTimeDrift();
    if (resource->fetchAndSetDeviceInformation(false))
    {
        // Clarify resource type
        QString fullName = resource->getName();
        int manufacturerPos = fullName.indexOf(QLatin1String("-"));
        QString manufacturer = fullName.mid(0,manufacturerPos).trimmed();
        QString modelName = fullName.mid(manufacturerPos+1).trimmed().toLower();

        if (NameHelper::instance().isSupported(modelName))
            return resList;

        int modelNamePos = modelName.indexOf(QLatin1String(" "));
        if (modelNamePos >= 0)
        {
            modelName = modelName.mid(modelNamePos+1);
            if (NameHelper::instance().isSupported(modelName))
                return resList;
        }
        QnId rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer, false);
        if (rt)
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
                for (int i = 1; i < resource->getMaxChannels(); ++i) 
                {
                    QnPlOnvifResourcePtr res(new QnPlOnvifResource());
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
    wsddSearcher.pleaseStop();
}

QnResourceList OnvifResourceSearcher::findResources()
{

    QnResourceList result;

    //Order is important! mdns should be the first to avoid creating ONVIF resource, when special is expected
    //mdnsSearcher.findResources(result, specialResourceCreator);
    wsddSearcher.findResources(result);

    return result;
}

QnResourcePtr OnvifResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        qDebug() << "OnvifResourceSearcher::createResource: no resource type for ID = " << resourceTypeId;
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

    qDebug() << "OnvifResourceSearcher::createResource: create ONVIF camera resource. TypeID: "
             << resourceTypeId.toString() << ", Parameters: " << parameters;

    result->deserialize(parameters);

    return result;

}
