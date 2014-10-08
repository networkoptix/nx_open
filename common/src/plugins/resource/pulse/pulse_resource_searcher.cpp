#ifdef ENABLE_PULSE_CAMERA

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
        QnPhysicalCameraResourcePtr cameraRes = res.dynamicCast<QnPhysicalCameraResource>();
        
        res->setName(r.name);
        if (cameraRes)
            cameraRes->setModel(r.name);
        res->setMAC(QnMacAddress(r.mac));
        res->setHostAddress(r.ip);
        res->setDiscoveryAddr(QHostAddress(r.disc_ip));

        result.push_back(res);

    }
    

    return result;
}

QnResourcePtr QnPlPulseSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& /*params*/)
{
    QnNetworkResourcePtr result;

    QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);

    if (resourceType.isNull())
    {
        qDebug() << "No resource type for ID = " << resourceTypeId;

        return result;
    }

    if (resourceType->getManufacture() == QnPlPulseResource::MANUFACTURE)
    {
        result = QnVirtualCameraResourcePtr( new QnPlPulseResource() );
    }
    else
        return result;

    
    result->setTypeId(resourceTypeId);

    qDebug() << "Create Pulse camera resource. typeID:" << resourceTypeId.toString(); // << ", Parameters: " << parameters;

    //result->deserialize(parameters);

    return result;
}

QList<QnResourcePtr> QnPlPulseSearcher::checkHostAddr(const QUrl& url, const QAuthenticator& auth, bool doMultichannelCheck)
{
    Q_UNUSED(url)
    Q_UNUSED(auth)
    Q_UNUSED(doMultichannelCheck)
    return QList<QnResourcePtr>();
}

QString QnPlPulseSearcher::manufacture() const
{
    return QString();
}


QnNetworkResourcePtr QnPlPulseSearcher::createResource(const QString& manufacture, const QString& name)
{

    QnNetworkResourcePtr result = QnNetworkResourcePtr(0);

    QnUuid rt = qnResTypePool->getResourceTypeId(manufacture, name);
    if (rt.isNull())
        return result;

    if (manufacture == QnPlPulseResource::MANUFACTURE)
    {
        result = QnNetworkResourcePtr(new QnPlPulseResource());
    }
        
    if (result)
        result->setTypeId(rt);

    return result;
}

#endif // #ifdef ENABLE_PULSE_CAMERA
