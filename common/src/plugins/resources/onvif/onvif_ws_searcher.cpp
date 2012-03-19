#include "onvif_ws_searcher.h"
#include "onvif_ws_searcher_helper.h"
#include "..\digitalwatchdog\digital_watchdog_resource.h"


QnPlOnvifWsSearcher::QnPlOnvifWsSearcher()
{

}

QnPlOnvifWsSearcher& QnPlOnvifWsSearcher::instance()
{
    static QnPlOnvifWsSearcher inst;
    return inst;
}

QnResourceList QnPlOnvifWsSearcher::findResources()
{
    QnResourceList result;

    QnPlOnvifWsSearcherHelper helper;
    QList<QnPlOnvifWsSearcherHelper::WSResult> onnvifResponses = helper.findResources();

    foreach(QnPlOnvifWsSearcherHelper::WSResult r, onnvifResponses)
    {
        QnNetworkResourcePtr res = createResource(r.manufacture, r.name);
        if (!res)
            continue;
        
        res->setName(r.name);
        res->setMAC(r.mac);
        res->setHostAddress(QHostAddress(r.ip), QnDomainMemory);
        res->setDiscoveryAddr(QHostAddress(r.disc_ip));

        result.push_back(res);

    }
    

    return result;
}

QnResourcePtr QnPlOnvifWsSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() == QnPlWatchDogResource::MANUFACTURE)
    {
        result = QnVirtualCameraResourcePtr( new QnPlWatchDogResource() );
    }
    else
        return result;

    
    result->setTypeId(resourceTypeId);

    qDebug() << "RTID" << resourceTypeId.toString() << ", Parameters: " << parameters;
    result->deserialize(parameters);

    return result;
}

QnResourcePtr QnPlOnvifWsSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

QString QnPlOnvifWsSearcher::manufacture() const
{
    return "";
}


QnNetworkResourcePtr QnPlOnvifWsSearcher::createResource(const QString& manufacture, const QString& name)
{

    QnNetworkResourcePtr result = QnNetworkResourcePtr(0);

    QnId rt = qnResTypePool->getResourceTypeId(manufacture, name);
    if (!rt.isValid())
        return result;

    if (manufacture == QnPlWatchDogResource::MANUFACTURE)
    {
        result = QnNetworkResourcePtr(new QnPlWatchDogResource());
    }
        
    if (result)
        result->setTypeId(rt);

    return result;
}