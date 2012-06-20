#include "onvif_resource_searcher.h"
#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
#include "onvif_resource_information_fetcher.h"


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
    return QnPlOnvifResource::MANUFACTURE;
}


QnResourcePtr OnvifResourceSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
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
