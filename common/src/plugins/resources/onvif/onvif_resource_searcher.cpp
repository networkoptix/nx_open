#include "onvif_resource_searcher.h"
#include "core/resource/camera_resource.h"


OnvifResourceSearcher::OnvifResourceSearcher():
    wsddSearcher(OnvifResourceSearcherWsdd::instance()),
    mdnsSearcher(OnvifResourceSearcherMdns::instance())
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

    //Order is important! WS-Discovery should be the first (it provides more info, than mDNS)
    wsddSearcher.findResources(result);
    mdnsSearcher.findResources(result);

    return result;
}

QnResourcePtr OnvifResourceSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
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
        qDebug() << "Manufacture " << resourceType->getManufacture() << " != " << manufacture();
        return result;
    }

    result = QnVirtualCameraResourcePtr( new QnPlOnvifResource( ) );
    result->setTypeId(resourceTypeId);

    qDebug() << "Create ONVIF camera resource. TypeID: " << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;

}
