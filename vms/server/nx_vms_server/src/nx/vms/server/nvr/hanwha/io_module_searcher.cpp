#include "io_module_searcher.h"

#include <media_server/media_server_module.h>

namespace nx::vms::server::nvr::hanwha {

static const QString kHanwhaNvrManufacturer = "Hanwha WAVE NVR";

IoModuleSearcher::IoModuleSearcher(QnMediaServerModule* serverModule):
    QnAbstractResourceSearcher(serverModule->commonModule()),
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QString IoModuleSearcher::manufacturer() const
{
    return kHanwhaNvrManufacturer;
}

QnResourceList IoModuleSearcher::findResources()
{
    // TODO: #dmishin implement.
    return {};
}

QnResourcePtr IoModuleSearcher::createResource(
    const QnUuid& resourceTypeId, const QnResourceParams& params)
{
    // TODO: #dmishin implement.
    return nullptr;
}

} // namespace nx::vms::server::nvr::hanwha
