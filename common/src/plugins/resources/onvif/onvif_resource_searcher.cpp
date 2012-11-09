#include "onvif_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"

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


QnResourcePtr OnvifResourceSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth)
{
    if (url.port() == -1)
    {
        for (int i = 0; i < sizeof(ONVIF_SERVICE_DEFAULT_PORTS)/sizeof(int); ++i) 
        {
            QUrl newUrl(url);
            newUrl.setPort(ONVIF_SERVICE_DEFAULT_PORTS[i]);
            QnResourcePtr result = checkHostAddrInternal(newUrl, auth);
            if (result)
                return result;
        }
        return QnResourcePtr();
    }
    else {
        return checkHostAddrInternal(url, auth);
    }
}

QnResourcePtr OnvifResourceSearcher::checkHostAddrInternal(const QUrl& url, const QAuthenticator& auth)
{
    QnResourceTypePtr typePtr = qnResTypePool->getResourceTypeByName(QLatin1String("ONVIF"));
    if (!typePtr)
        return QnResourcePtr();

    int onvifPort = url.port(80);
    QString onvifUrl(QLatin1String("onvif/device_service"));

    QnPlOnvifResourcePtr resource = QnPlOnvifResourcePtr(new QnPlOnvifResource());
    resource->setTypeId(typePtr->getId());
    resource->setAuth(auth);
    //resource->setDiscoveryAddr(addr);
    QString deviceUrl = QString(QLatin1String("http://%1:%2/%3")).arg(url.host()).arg(onvifPort).arg(onvifUrl);
    resource->setUrl(deviceUrl);
    resource->setDeviceOnvifUrl(deviceUrl);

    resource->calcTimeDrift();
    if (resource->fetchAndSetDeviceInformation())
    {
        // Clarify resource type
        QString fullName = resource->getName();
        int manufacturerPos = fullName.indexOf(QLatin1String("-"));
        QString manufacturer = fullName.mid(0,manufacturerPos).trimmed();
        QString modelName = fullName.mid(manufacturerPos+1).trimmed().toLower();

        if (NameHelper::instance().isSupported(modelName))
            return QnResourcePtr();

        int modelNamePos = modelName.indexOf(QLatin1String(" "));
        if (modelNamePos >= 0)
        {
            modelName = modelName.mid(modelNamePos+1);
            if (NameHelper::instance().isSupported(modelName))
                return QnResourcePtr();
        }
        QnId rt = qnResTypePool->getResourceTypeId(QLatin1String("OnvifDevice"), manufacturer);
        if (rt)
            resource->setTypeId(rt);

        if(resource->getUniqueId().isEmpty())
            return QnResourcePtr();
        else
            return resource;
    }
    else 
        return QnResourcePtr();
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
