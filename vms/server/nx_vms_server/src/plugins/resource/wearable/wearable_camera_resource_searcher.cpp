#include "wearable_camera_resource_searcher.h"
#include "wearable_camera_resource.h"

#include <nx/vms/api/data/camera_data.h>

QnWearableCameraResourceSearcher::QnWearableCameraResourceSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    QnAbstractNetworkResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnWearableCameraResourceSearcher::~QnWearableCameraResourceSearcher()
{
}

QString QnWearableCameraResourceSearcher::manufacture() const
{
    return QnWearableCameraResource::kManufacture;
}

bool QnWearableCameraResourceSearcher::isResourceTypeSupported(QnUuid resourceTypeId) const
{
    return resourceTypeId == nx::vms::api::CameraData::kWearableCameraTypeId;
}

QnResourcePtr QnWearableCameraResourceSearcher::createResource(const QnUuid &resourceTypeId, const QnResourceParams& params)
{
    QnWearableCameraResourcePtr result;

    auto resourceType = qnResTypePool->getResourceType(resourceTypeId);
    NX_ASSERT(resourceType, Q_FUNC_INFO, "Wearable camera resource type not found");
    if (!resourceType)
        return result;

    if (resourceType->getManufacture() != manufacture())
        return result;

    QUrl url(params.url);
    if (url.scheme() != lit("wearable"))
        return result;

    QString physicalId = url.path();
    while (physicalId.startsWith('/'))
        physicalId.remove(0, 1);

    result.reset(new QnWearableCameraResource(serverModule()));
    result->setTypeId(resourceTypeId);
    result->setId(params.resID);
    result->setPhysicalId(physicalId);
    result->setUrl(params.url);
    result->setVendor(params.vendor);

    return result;
}

QnResourceList QnWearableCameraResourceSearcher::findResources()
{
    return {};
}

QList<QnResourcePtr> QnWearableCameraResourceSearcher::checkHostAddr(const nx::utils::Url&, const QAuthenticator&, bool)
{
    return {};
}

