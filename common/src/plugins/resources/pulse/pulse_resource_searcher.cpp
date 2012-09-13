#include "pulse_resource_searcher.h"
#include "pulse_searcher_helper.h"
#include "../pulse/pulse_resource.h"


QnPlPulseSearcher::QnPlPulseSearcher()
{

}

QnPlPulseSearcher& QnPlPulseSearcher::instance()
{
    static QnPlPulseSearcher inst;
    return inst;
}

QnResourceList QnPlPulseSearcher::findResources()
{
    QnResourceList result;

    QnPlPulseSearcherHelper helper;
    QList<QnPlPulseSearcherHelper::WSResult> onnvifResponses = helper.findResources();

    foreach(QnPlPulseSearcherHelper::WSResult r, onnvifResponses)
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

QnResourcePtr QnPlPulseSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() == QLatin1String(QnPlPulseResource::MANUFACTURE))
    {
        result = QnVirtualCameraResourcePtr( new QnPlPulseResource() );
    }
    else
        return result;

    
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Pulse camera resource. typeID:" << resourceTypeId.toString() << ", Parameters: " << parameters;

    result->deserialize(parameters);

    return result;
}

QnResourcePtr QnPlPulseSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth)
{
    return QnResourcePtr(0);
}

QString QnPlPulseSearcher::manufacture() const
{
    return QString();
}


QnNetworkResourcePtr QnPlPulseSearcher::createResource(const QString& manufacture, const QString& name)
{

    QnNetworkResourcePtr result = QnNetworkResourcePtr(0);

    QnId rt = qnResTypePool->getResourceTypeId(manufacture, name);
    if (!rt.isValid())
        return result;

    if (manufacture == QLatin1String(QnPlPulseResource::MANUFACTURE))
    {
        result = QnNetworkResourcePtr(new QnPlPulseResource());
    }
        
    if (result)
        result->setTypeId(rt);

    return result;
}
