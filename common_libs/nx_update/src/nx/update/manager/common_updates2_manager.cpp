#include "common_updates2_manager.h"
#include "detail/update_request_data_factory.h"
#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/sync_update_checker.h>
#include <nx/update/info/update_registry_factory.h>
#include <common/common_module.h>
#include <utils/common/synctime.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <nx/vms/common/p2p/downloader/downloader.h>


namespace nx {
namespace update {

using namespace vms::common::p2p::downloader;

namespace {

static const qint64 kRefreshTimeoutMs = 24 * 60 * 60 * 1000; //< 1 day
static const QString kFileName = "update.status";

} // namespace

CommonUpdates2Manager::CommonUpdates2Manager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void CommonUpdates2Manager::loadStatusFromFile()
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        NX_VERBOSE(this, "Failed to open persistent update status data");
        return;
    }

    const QByteArray content = file.readAll();
    if (!QJson::deserialize(content, &m_currentStatus))
        NX_VERBOSE(this, "Failed to deserialize persistent update status data");
}

update::info::AbstractUpdateRegistryPtr CommonUpdates2Manager::getGlobalRegistry()
{
    auto globalRegistry = update::info::UpdateRegistryFactory::create();
    bool deserializeResult = globalRegistry->fromByteArray(globalSettings()->updates2Registry());
    if (!deserializeResult)
        return update::info::AbstractUpdateRegistryPtr();
    return globalRegistry;
}

QnUuid CommonUpdates2Manager::moduleGuid() const
{
    return commonModule()->moduleGUID();
}

void CommonUpdates2Manager::updateGlobalRegistry(const QByteArray& serializedRegistry)
{
    // #TODO #akulikov return here if called on the client

    globalSettings()->setUpdates2Registry(serializedRegistry);
    globalSettings()->synchronizeNow();
}

void CommonUpdates2Manager::writeStatusToFile(const detail::Updates2StatusDataEx& statusData)
{
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || !file.write(QJson::serialized(statusData)))
    {
        NX_WARNING(this, "Failed to save persistent update status data");
        return;
    }
}

} // namespace update
} // namespace nx
