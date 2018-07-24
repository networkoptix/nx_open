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
#include <api/runtime_info_manager.h>
#include <nx/vms/common/p2p/downloader/downloader.h>
#include "detail/update_request_data_factory.h"
#include "common_updates2_manager.h"


namespace nx {
namespace update {

using namespace vms::common::p2p::downloader;

namespace {

static const QString kFileName = "update.status";

} // namespace

CommonUpdates2Manager::CommonUpdates2Manager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void CommonUpdates2Manager::connectToSignals()
{
    connect(
        globalSettings(), &QnGlobalSettings::updates2RegistryChanged,
        this, &CommonUpdates2Manager::checkForGlobalDictionaryUpdate, Qt::QueuedConnection);
    connect(
        downloader(), &Downloader::downloadFinished,
        this, &CommonUpdates2Manager::onDownloadFinished, Qt::QueuedConnection);
    connect(
        downloader(), &Downloader::downloadFailed,
        this, &CommonUpdates2Manager::onDownloadFailed, Qt::QueuedConnection);
    connect(
        downloader(), &Downloader::fileDeleted,
        this, &CommonUpdates2Manager::onFileDeleted, Qt::QueuedConnection);
    connect(
        downloader(), &Downloader::fileInformationChanged,
        this, &CommonUpdates2Manager::onFileInformationChanged, Qt::QueuedConnection);
    connect(
        downloader(), &Downloader::fileStatusChanged,
        this, &CommonUpdates2Manager::onFileInformationStatusChanged, Qt::QueuedConnection);
    connect(
        downloader(), &Downloader::chunkDownloadFailed,
        this, &CommonUpdates2Manager::onChunkDownloadFailed, Qt::QueuedConnection);
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
    auto globalRegistry = update::info::UpdateRegistryFactory::create(peerId());
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
    // #TODO #akulikov Return here if called on the client or figure out something better.

    globalSettings()->setUpdates2Registry(serializedRegistry);
    globalSettings()->synchronizeNow();
}

void CommonUpdates2Manager::writeStatusToFile(const manager::detail::Updates2StatusDataEx& statusData)
{
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || !file.write(QJson::serialized(statusData)))
    {
        NX_WARNING(this, "Failed to save persistent update status data");
        return;
    }
}

bool CommonUpdates2Manager::isClient() const
{
    return commonModule()->runtimeInfoManager()->localInfo().data.peer.isClient();
}

QnUuid CommonUpdates2Manager::peerId() const
{
    return commonModule()->moduleGUID();
}

} // namespace update
} // namespace nx
