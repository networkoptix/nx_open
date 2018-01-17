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

namespace {

static const qint64 kRefreshTimeoutMs = 24 * 60 * 60 * 1000; //< 1 day
static const QString kFileName = "update.status";

static QString filePath()
{
    return qnServerModule->settings()->getDataDirectory()
        + QDir::separator()
        + kFileName;
}

static qint64 refreshTimeout()
{
    const auto settingsValue = qnServerModule->roSettings()->value(
        nx_ms_conf::CHECK_FOR_UPDATE_TIMEOUT).toLongLong();
    return settingsValue == 0 ? kRefreshTimeoutMs : settingsValue;
}

static bool isNewRegistryBetter(
    const update::info::AbstractUpdateRegistryPtr& oldRegistry,
    const update::info::AbstractUpdateRegistryPtr& newRegistry)
{
    if (!oldRegistry)
        return true;
    QnSoftwareVersion newRegistryServerVersion;
    if (newRegistry->latestUpdate(
        detail::UpdateFileRequestDataFactory::create(),
        &newRegistryServerVersion) != update::info::ResultCode::ok)
    {
        return false;
    }
    QnSoftwareVersion oldRegistryServerVersion;
    if (oldRegistry->latestUpdate(
        detail::UpdateFileRequestDataFactory::create(),
        &oldRegistryServerVersion) != update::info::ResultCode::ok
        || newRegistryServerVersion > oldRegistryServerVersion)
    {
        return true;
    }
    return false;
}

} // namespace

Updates2Manager::Updates2Manager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
    loadStatusFromFile();
}

void Updates2Manager::atServerStart()
{
    using namespace vms::common::p2p::downloader;
    using namespace std::placeholders;

    checkForGlobalDictionaryUpdate();

    // Amending initial state
    switch (m_currentStatus.status)
    {
        case api::Updates2StatusData::StatusCode::available:
        case api::Updates2StatusData::StatusCode::error:
        case api::Updates2StatusData::StatusCode::notAvailable:
            break;
        case api::Updates2StatusData::StatusCode::checking:
            setStatusUnsafe(
                api::Updates2StatusData::StatusCode::notAvailable,
                "Update is unavailable");
            break;
        case api::Updates2StatusData::StatusCode::downloading:
            setStatusUnsafe(
                api::Updates2StatusData::StatusCode::available,
                "Update is available");
            download();
            break;
        case api::Updates2StatusData::StatusCode::preparing:
            // TODO: implement
            break;
        case api::Updates2StatusData::StatusCode::readyToInstall:
            // TODO: implement
            break;
    }

    connect(
        globalSettings(), &QnGlobalSettings::updates2RegistryChanged,
        this, &Updates2Manager::checkForGlobalDictionaryUpdate);
    connect(
        qnServerModule->findInstance<Downloader>(), &Downloader::downloadFinished,
        this, &Updates2Manager::onDownloadFinished);

    m_timerManager.addNonStopTimer(
        std::bind(&Updates2Manager::checkForRemoteUpdate, this, _1),
        std::chrono::milliseconds(refreshTimeout()),
        std::chrono::milliseconds(0));
}

api::Updates2StatusData Updates2Manager::status()
{
    QnMutexLocker lock(&m_mutex);
    return m_currentStatus.base();
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

void Updates2Manager::checkForGlobalDictionaryUpdate()
{
    auto globalRegistry = update::info::UpdateRegistryFactory::create();
    bool deserializeResult = globalRegistry->fromByteArray(globalSettings()->updates2Registry());
    if (!deserializeResult)
        return;
    swapRegistries(std::move(globalRegistry));
    refreshStatusAfterCheck();
}

void Updates2Manager::checkForRemoteUpdate(utils::TimerId /*timerId*/)
{
    {
        QnMutexLocker lock(&m_mutex);
        switch (m_currentStatus.status)
        {
            case api::Updates2StatusData::StatusCode::notAvailable:
            case api::Updates2StatusData::StatusCode::available:
            case api::Updates2StatusData::StatusCode::error:
                m_currentStatus = api::Updates2StatusData(
                    commonModule()->moduleGUID(),
                    api::Updates2StatusData::StatusCode::checking,
                    "Checking for update");
                break;
            default:
                return;
        }
    }

    auto updateUrl= qnServerModule->roSettings()->value(
        nx_ms_conf::CHECK_FOR_UPDATE_URL).toString();
    updateUrl = updateUrl.isNull() ? update::info::kDefaultUrl : updateUrl;
    update::info::AbstractUpdateRegistryPtr updateRegistry = update::info::checkSync(updateUrl);

    if (updateRegistry)
    {
        auto globalRegistry = update::info::UpdateRegistryFactory::create();
        if (!globalRegistry->fromByteArray(globalSettings()->updates2Registry())
            || isNewRegistryBetter(globalRegistry, updateRegistry))
        {
            globalSettings()->setUpdates2Registry(updateRegistry->toByteArray());
            globalSettings()->synchronizeNow();
        }
        swapRegistries(std::move(updateRegistry));
    }

    refreshStatusAfterCheck();
}


void Updates2Manager::swapRegistries(update::info::AbstractUpdateRegistryPtr otherRegistry)
{
    QnMutexLocker lock(&m_mutex);
    if (isNewRegistryBetter(m_updateRegistry, otherRegistry))
        m_updateRegistry = std::move(otherRegistry);
}

void Updates2Manager::refreshStatusAfterCheck()
{
    QnMutexLocker lock(&m_mutex);
    switch (m_currentStatus.status)
    {
        case api::Updates2StatusData::StatusCode::notAvailable:
        case api::Updates2StatusData::StatusCode::available:
        case api::Updates2StatusData::StatusCode::error:
        case api::Updates2StatusData::StatusCode::checking:
        {
            QnSoftwareVersion version;
            if (!m_updateRegistry)
            {
                setStatusUnsafe(
                    api::Updates2StatusData::StatusCode::error,
                    "Failed to get updates data");
                return;
            }

            if (m_updateRegistry->latestUpdate(
                detail::UpdateFileRequestDataFactory::create(),
                &version) != update::info::ResultCode::ok)
            {
                setStatusUnsafe(
                    api::Updates2StatusData::StatusCode::notAvailable,
                    "Update is unavailable");
                return;
            }

            nx::update::info::FileData fileData;
            const auto result = m_updateRegistry->findUpdateFile(
                detail::UpdateFileRequestDataFactory::create(),
                &fileData);
            NX_ASSERT(result == update::info::ResultCode::ok);
            if (result != update::info::ResultCode::ok)
            {
                setStatusUnsafe(
                    api::Updates2StatusData::StatusCode::error,
                    lit("Failed to get update file information: %1")
                        .arg(version.toString()));
                return;
            }

            setStatusUnsafe(
                api::Updates2StatusData::StatusCode::available,
                lit("Update is available: %1").arg(version.toString()));

            break;
        }
        default:
            return;
    }
}

api::Updates2StatusData Updates2Manager::download()
{
    QnMutexLocker lock(&m_mutex);
    if (m_currentStatus.status != api::Updates2StatusData::StatusCode::available)
        return m_currentStatus.base();

    NX_ASSERT((bool) m_updateRegistry);

    nx::update::info::FileData fileData;
    const auto result = m_updateRegistry->findUpdateFile(
        detail::UpdateFileRequestDataFactory::create(),
        &fileData);
    NX_ASSERT(result == update::info::ResultCode::ok);

    if (result != update::info::ResultCode::ok)
    {
        setStatusUnsafe(
            api::Updates2StatusData::StatusCode::error,
            "Failed to get update file information");
        return m_currentStatus.base();
    }

    using namespace vms::common::p2p::downloader;

    FileInformation fileInformation;
    fileInformation.md5 = fileData.md5;
    fileInformation.name = fileData.file;
    fileInformation.size = fileData.size;
    fileInformation.url = fileData.url;
    fileInformation.peerPolicy = FileInformation::PeerPolicy::byPlatform;

    for (const auto file: m_currentStatus.downloadedFiles)
        qnServerModule->findInstance<Downloader>()->deleteFile(file);
    m_currentStatus.downloadedFiles.append(fileData.file);

    qnServerModule->findInstance<Downloader>()->addFile(fileInformation);
    setStatusUnsafe(
        api::Updates2StatusData::StatusCode::downloading,
        lit("Downloading update file: %1").arg(fileData.file));

    return m_currentStatus.base();
}

void Updates2Manager::setStatusUnsafe(
    api::Updates2StatusData::StatusCode code,
    const QString& message)
{
    auto newStatusData = detail::Updates2StatusDataEx(
        qnSyncTime->currentMSecsSinceEpoch(),
        commonModule()->moduleGUID(),
        code,
        message);

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || !file.write(QJson::serialized(newStatusData)))
    {
        NX_WARNING(this, "Failed to save persistent update status data");
        return;
    }

    m_currentStatus = newStatusData;
}

void Updates2Manager::onDownloadFinished(const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);
    using namespace vms::common::p2p::downloader;
    auto downloader = qnServerModule->findInstance<Downloader>();
    NX_ASSERT(downloader);

    auto onError = [this, downloader, &fileName](const QString& errorMessage)
    {
        setStatusUnsafe(api::Updates2StatusData::StatusCode::error, errorMessage);
    };

    NX_ASSERT(m_currentStatus.status == api::Updates2StatusData::StatusCode::downloading);
    if (m_currentStatus.status != api::Updates2StatusData::StatusCode::downloading)
        return onError("Unexpected update manager state");

    auto fileInformation = downloader->fileInformation(fileName);
    NX_ASSERT(fileInformation.isValid());
    if (!fileInformation.isValid())
        return onError(lit("Downloader internal error for file: %1").arg(fileName));

    switch (fileInformation.status)
    {
        case FileInformation::Status::notFound:
            return onError(lit("Failed to find update file: %1").arg(fileName));
        case FileInformation::Status::corrupted:
            return onError(lit("Update file is corrupted: %1").arg(fileName));
        default:
            NX_ASSERT(fileInformation.status == FileInformation::Status::downloaded);
            setStatusUnsafe(
                api::Updates2StatusData::StatusCode::preparing,
                "Update has been downloaded and now is preparing for install");
            break;
    }
}

void Updates2Manager::stopAsyncTasks()
{
    m_timerManager.stop();
}

} // namespace updates2
} // namespace mediaserver
} // namespace nx
