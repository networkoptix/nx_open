#include "onvif_resource_searcher.h"
#include "core/resource/camera_resource.h"


OnvifResourceSearcher::OnvifResourceSearcher():
    wsddSearcher(OnvifResourceSearcherWsdd::instance()),
    mdnsSearcher(OnvifResourceSearcherMdns::instance()),
    specialResourceCreator(0)
{

}

OnvifResourceSearcher::~OnvifResourceSearcher()
{

}

void OnvifResourceSearcher::init(const OnvifSpecialResourceCreatorPtr& creator)
{
    specialResourceCreator = creator;
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
    wsddSearcher.findResources(result, specialResourceCreator);

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

    result = specialResourceCreator.isNull()? result: specialResourceCreator->createById(resourceType, parameters);
    if (!result.isNull()) {
        return result;
    }

    if (resourceType->getManufacture() != manufacture())
    {
        qDebug() << "OnvifResourceSearcher::createResource: manufacture " << resourceType->getManufacture()
                 << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnPlOnvifResource( ) );
    result->setTypeId(resourceTypeId);

    qDebug() << "OnvifResourceSearcher::createResource: create ONVIF camera resource. TypeID: "
             << resourceTypeId.toString() << ", Parameters: " << parameters;

    result->deserialize(parameters);

    return result;

}
