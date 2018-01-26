#ifdef ENABLE_PULSE_CAMERA

#include "pulse_resource_searcher.h"

#include <memory>
#include <mutex>

#include "pulse_searcher_helper.h"
#include "../pulse/pulse_resource.h"
#include "core/resource/resource_data.h"
#include "core/resource_management/resource_data_pool.h"
#include "common/common_module.h"
#include <common/static_common_module.h>

QnPlPulseSearcher::QnPlPulseSearcher(QnCommonModule* commonModule):
    QnAbstractResourceSearcher(commonModule),
    QnAbstractNetworkResourceSearcher(commonModule)
{
}

QnResourceList QnPlPulseSearcher::findResources()
{
    QnResourceList result;

    QnPlPulseSearcherHelper helper;
    QList<QnPlPulseSearcherHelper::WSResult> onnvifResponses = helper.findResources();

    for(const QnPlPulseSearcherHelper::WSResult& r: onnvifResponses)
    {
        QnNetworkResourcePtr res = createResource(r.manufacture, r.name);
        if (!res)
            continue;

        QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacture(), r.name);
        if (resourceData.value<bool>(Qn::FORCE_ONVIF_PARAM_NAME))
            continue; // model forced by ONVIF

        res->setName(r.name);
        if (auto camera = res.dynamicCast<nx::mediaserver::resource::Camera>())
            camera->setModel(r.name);

        res->setMAC(QnMacAddress(r.mac));
        res->setHostAddress(r.ip);

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
