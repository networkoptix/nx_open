#include "io_module_searcher.h"

#include <media_server/media_server_module.h>
#include <core/resource/media_server_resource.h>

#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/vms/server/nvr/i_service.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/io/io_module_resource.h>

namespace nx::vms::server::nvr::hanwha {

static const QString kDefaultNvrIoModuleName("NVR IO block");
static const QString kDefaultNvrIoModuleModelPrefix("NVR-IO-module-");
static const QString kNvrIoModuleResourceTypeName("Hanwha_Wave_Nvr_Io_Module");


IoModuleSearcher::IoModuleSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QString IoModuleSearcher::manufacturer() const
{
    return kHanwhaPoeNvrDriverName;
}

QnResourceList IoModuleSearcher::findResources()
{
    const QnResourceTypePtr resourceType =
        qnResTypePool->getResourceTypeByName(kNvrIoModuleResourceTypeName);

    if (resourceType.isNull())
        return {};

    const IService* nvrService = serverModule()->nvrService();
    if (!NX_ASSERT(nvrService, "Unable to access the NVR service"))
        return {};

    const DeviceInfo deviceInfo = nvrService->deviceInfo();
    IoModuleResourcePtr ioModule(new IoModuleResource(serverModule()));

    ioModule->setTypeId(resourceType->getId());
    ioModule->setName(kDefaultNvrIoModuleName);
    ioModule->setModel(kDefaultNvrIoModuleModelPrefix + deviceInfo.model);
    ioModule->setPhysicalId(
        serverModule()->commonModule()->moduleGUID().toString() + "_io_module");
    ioModule->setVendor(kDefaultVendor);
    ioModule->setUrl(serverModule()->commonModule()->currentServer()->getUrl());

    QnResourceList result;
    result.push_back(std::move(ioModule));
    return result;
}

QnResourcePtr IoModuleSearcher::createResource(
    const QnUuid& resourceTypeId, const QnResourceParams& params)
{
    const QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resourceTypeId);
    if (resourceType.isNull())
    {
        NX_DEBUG(this, "No resource type for ID %1", resourceTypeId);
        return {};
    }

    if (resourceType->getManufacturer() != manufacturer())
        return {};

    QnResourcePtr result;
    result.reset(new IoModuleResource(serverModule()));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, "Created Hanwha WAVE NVR IO module resource");
    return result;
}

bool IoModuleSearcher::isVirtualResource() const
{
    // NVR IO-module should be discovered even if auto discovery is disabled.
    return true;
}

} // namespace nx::vms::server::nvr::hanwha
