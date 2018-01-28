#include "detail/update_request_data_factory.h"
#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/sync_update_checker.h>
#include <nx/update/info/update_registry_factory.h>
#include <common/common_module.h>
#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <nx/vms/common/p2p/downloader/downloader.h>

#include "updates2_manager.h"

namespace nx {
namespace mediaserver {
namespace updates2 {

using namespace vms::common::p2p::downloader;

namespace {

static const qint64 kRefreshTimeoutMs = 24 * 60 * 60 * 1000; //< 1 day
static const QString kFileName = "update.status";

static QString filePath()
{
    return qnServerModule->settings()->getDataDirectory()
        + QDir::separator()
        + kFileName;
}

static qint64 refreshTimeoutImpl()
{
    const auto settingsValue = qnServerModule->roSettings()->value(
        nx_ms_conf::CHECK_FOR_UPDATE_TIMEOUT).toLongLong();
    return settingsValue == 0 ? kRefreshTimeoutMs : settingsValue;
}

} // namespace

Updates2Manager::Updates2Manager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

qint64 Updates2Manager::refreshTimeout()
{
    return refreshTimeoutImpl();
}

void Updates2Manager::loadStatusFromFile()
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

void Updates2Manager::connectToSignals()
{
    connect(
        globalSettings(), &QnGlobalSettings::updates2RegistryChanged,
        this, &Updates2Manager::checkForGlobalDictionaryUpdate);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::downloadFinished,
        this, &Updates2Manager::onDownloadFinished);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::downloadFailed,
        this, &Updates2Manager::onDownloadFailed);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::fileAdded,
        this, &Updates2Manager::onFileAdded);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::fileDeleted,
        this, &Updates2Manager::onFileDeleted);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::fileInformationChanged,
        this, &Updates2Manager::onFileInformationChanged);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::fileStatusChanged,
        this, &Updates2Manager::onFileInformationStatusChanged);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::chunkDownloadFailed,
        this, &Updates2Manager::onChunkDownloadFailed);
}

update::info::AbstractUpdateRegistryPtr Updates2Manager::getGlobalRegistry()
{
    auto globalRegistry = update::info::UpdateRegistryFactory::create();
    bool deserializeResult = globalRegistry->fromByteArray(globalSettings()->updates2Registry());
    if (!deserializeResult)
        return update::info::AbstractUpdateRegistryPtr();
    return globalRegistry;
}

QnUuid Updates2Manager::moduleGuid()
{
    return commonModule()->moduleGUID();
}

void Updates2Manager::updateGlobalRegistry(const QByteArray& serializedRegistry)
{
    globalSettings()->setUpdates2Registry(serializedRegistry);
    globalSettings()->synchronizeNow();
}

void Updates2Manager::writeStatusToFile(const detail::Updates2StatusDataEx& statusData)
{
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || !file.write(QJson::serialized(statusData)))
    {
        NX_WARNING(this, "Failed to save persistent update status data");
        return;
    }
}

vms::common::p2p::downloader::AbstractDownloader* Updates2Manager::downloader()
{
    return qnServerModule->findInstance<vms::common::p2p::downloader::Downloader>();
}

update::info::AbstractUpdateRegistryPtr Updates2Manager::getRemoteRegistry()
{
    auto updateUrl =
        qnServerModule->roSettings()->value(nx_ms_conf::CHECK_FOR_UPDATE_URL).toString();
    updateUrl = updateUrl.isNull() ? update::info::kDefaultUrl : updateUrl;

    return update::info::checkSync(updateUrl);
}

} // namespace updates2
} // namespace mediaserver
} // namespace nx
