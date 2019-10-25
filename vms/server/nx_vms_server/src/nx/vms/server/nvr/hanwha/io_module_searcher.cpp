#include "io_module_searcher.h"

#include <media_server/media_server_module.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/io_module_resource.h>

namespace nx::vms::server::nvr::hanwha {

static const QString kDefaultNvrIoModuleName("NVR IO block");
static const QString kDefaultNvrIoModuleModel("NVR-IO-module #dmishin take model from board id!");
static const QString kDefaultNvrIoModuleFirmware("#dmishin take firmware from driver version!");
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

    IoModuleResourcePtr ioModule(new IoModuleResource(serverModule()));
    ioModule->setTypeId(resourceType->getId());
    ioModule->setName(kDefaultNvrIoModuleName);
    ioModule->setModel(kDefaultNvrIoModuleModel);
    ioModule->setFirmware(kDefaultNvrIoModuleFirmware);
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
        return nullptr;
    }

    if (resourceType->getManufacturer() != manufacturer())
        return nullptr;

    QnResourcePtr result;
    result.reset(new IoModuleResource(serverModule()));
    result->setTypeId(resourceTypeId);

    NX_DEBUG(this, "Created Hanwha WAVE NVR IO module resource");
    return result;
}

} // namespace nx::vms::server::nvr::hanwha
