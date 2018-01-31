#include "update_request_data_factory.h"
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

#include "updates2_manager_base.h"

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

using namespace vms::common::p2p::downloader;

namespace {

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

Updates2ManagerBase::Updates2ManagerBase()
{
}

void Updates2ManagerBase::atServerStart()
{
    using namespace std::placeholders;

    loadStatusFromFile();
    checkForGlobalDictionaryUpdate();

    // Amending initial state
    switch (m_currentStatus.state)
    {
        case api::Updates2StatusData::StatusCode::available:
        case api::Updates2StatusData::StatusCode::error:
        case api::Updates2StatusData::StatusCode::notAvailable:
            break;
        case api::Updates2StatusData::StatusCode::checking:
            setStatus(
                api::Updates2StatusData::StatusCode::notAvailable,
                "Update is unavailable");
            break;
        case api::Updates2StatusData::StatusCode::downloading:
            setStatus(
                api::Updates2StatusData::StatusCode::available,
                "Update is available");
            download();
            break;
        case api::Updates2StatusData::StatusCode::preparing:
            // #TODO: #akulikov implement
            break;
        case api::Updates2StatusData::StatusCode::readyToInstall:
            // #TODO: #akulikov implement
            break;
    }

    connectToSignals();

    m_timerManager.addNonStopTimer(
        std::bind(&Updates2ManagerBase::checkForRemoteUpdate, this, _1),
        std::chrono::milliseconds(refreshTimeout()),
        std::chrono::milliseconds(0));
}

api::Updates2StatusData Updates2ManagerBase::status()
{
    QnMutexLocker lock(&m_mutex);
    return m_currentStatus.base();
}

void Updates2ManagerBase::checkForGlobalDictionaryUpdate()
{
    auto globalRegistry = getGlobalRegistry();
    if (!globalRegistry)
        return;
    swapRegistries(std::move(globalRegistry));
    refreshStatusAfterCheck();
}

void Updates2ManagerBase::checkForRemoteUpdate(utils::TimerId /*timerId*/)
{
    auto onExitGuard = makeScopeGuard([this]() { remoteUpdateCompleted(); });
    {
        QnMutexLocker lock(&m_mutex);
        switch (m_currentStatus.state)
        {
            case api::Updates2StatusData::StatusCode::notAvailable:
            case api::Updates2StatusData::StatusCode::available:
            case api::Updates2StatusData::StatusCode::error:
                m_currentStatus = api::Updates2StatusData(
                    moduleGuid(),
                    api::Updates2StatusData::StatusCode::checking,
                    "Checking for update");
                break;
            default:
                return;
        }
    }

    auto remoteRegistry = getRemoteRegistry();
    if (remoteRegistry)
    {
        auto globalRegistry = getGlobalRegistry();
        if (!globalRegistry || isNewRegistryBetter(globalRegistry, remoteRegistry))
            updateGlobalRegistry(remoteRegistry->toByteArray());

        swapRegistries(std::move(remoteRegistry));
    }

    refreshStatusAfterCheck();
}

void Updates2ManagerBase::swapRegistries(update::info::AbstractUpdateRegistryPtr otherRegistry)
{
    QnMutexLocker lock(&m_mutex);
    if (isNewRegistryBetter(m_updateRegistry, otherRegistry))
        m_updateRegistry = std::move(otherRegistry);
}

void Updates2ManagerBase::refreshStatusAfterCheck()
{
    QnMutexLocker lock(&m_mutex);
    switch (m_currentStatus.state)
    {
        case api::Updates2StatusData::StatusCode::notAvailable:
        case api::Updates2StatusData::StatusCode::available:
        case api::Updates2StatusData::StatusCode::error:
        case api::Updates2StatusData::StatusCode::checking:
        {
            QnSoftwareVersion version;
            if (!m_updateRegistry)
            {
                setStatus(
                    api::Updates2StatusData::StatusCode::error,
                    "Failed to get updates data");
                return;
            }

            if (m_updateRegistry->latestUpdate(
                    detail::UpdateFileRequestDataFactory::create(),
                    &version) != update::info::ResultCode::ok)
            {
                setStatus(
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
                setStatus(
                    api::Updates2StatusData::StatusCode::error,
                    lit("Failed to get update file information: %1")
                        .arg(version.toString()));
                return;
            }

            setStatus(
                api::Updates2StatusData::StatusCode::available,
                lit("Update is available: %1").arg(version.toString()));

            break;
        }
        default:
            return;
    }
}

api::Updates2StatusData Updates2ManagerBase::download()
{
    QnMutexLocker lock(&m_mutex);
    if (m_currentStatus.state != api::Updates2StatusData::StatusCode::available)
        return m_currentStatus.base();

    NX_ASSERT((bool) m_updateRegistry);

    nx::update::info::FileData fileData;
    const auto result = m_updateRegistry->findUpdateFile(
        detail::UpdateFileRequestDataFactory::create(),
        &fileData);
    NX_ASSERT(result == update::info::ResultCode::ok);

    if (result != update::info::ResultCode::ok)
    {
        setStatus(
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
    fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::byPlatform;

    ResultCode resultCode = downloader()->addFile(fileInformation);
    switch (resultCode)
    {
        case ResultCode::fileAlreadyDownloaded:
            setStatus(
                api::Updates2StatusData::StatusCode::preparing,
                lit("Preparing update file: %1").arg(fileData.file));
            startPreparing();
        case ResultCode::fileAlreadyExists:
        case ResultCode::ok:
            setStatus(
                api::Updates2StatusData::StatusCode::downloading,
                lit("Downloading update file: %1").arg(fileData.file));
            m_currentStatus.files.insert(fileData.file);
            break;
        case ResultCode::fileDoesNotExist:
        case ResultCode::ioError:
        case ResultCode::invalidChecksum:
        case ResultCode::invalidFileSize:
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                lit("Downloader failed to start downloading: %1").arg(fileData.file));
            m_currentStatus.files.remove(fileData.file);
            break;
    }

    return m_currentStatus.base();
}

void Updates2ManagerBase::setStatus(
    api::Updates2StatusData::StatusCode code,
    const QString& message)
{
    auto newStatusData = detail::Updates2StatusDataEx(
        qnSyncTime->currentMSecsSinceEpoch(),
        moduleGuid(),
        code,
        message);

    writeStatusToFile(newStatusData);
    m_currentStatus = newStatusData;
}

void Updates2ManagerBase::startPreparing()
{
}

void Updates2ManagerBase::onDownloadFinished(const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);
    auto onExitGuard = makeScopeGuard([this]() { downloadFinished(); });

    using namespace vms::common::p2p::downloader;

    auto onError = [this, &fileName](const QString& errorMessage)
    {
        setStatus(api::Updates2StatusData::StatusCode::error, errorMessage);
    };

    if (!m_currentStatus.files.contains(fileName))
        return;

    NX_ASSERT(m_currentStatus.state == api::Updates2StatusData::StatusCode::downloading);
    if (m_currentStatus.state != api::Updates2StatusData::StatusCode::downloading)
        return onError("Unexpected update manager state");

    auto fileInformation = downloader()->fileInformation(fileName);
    NX_ASSERT(fileInformation.isValid());
    if (!fileInformation.isValid())
        return onError(lit("Downloader internal error for the file: %1").arg(fileName));

    switch (fileInformation.status)
    {
        case FileInformation::Status::notFound:
            return onError(lit("Failed to find update file: %1").arg(fileName));
        case FileInformation::Status::corrupted:
            return onError(lit("Update file is corrupted: %1").arg(fileName));
        default:
            NX_ASSERT(fileInformation.status == FileInformation::Status::downloaded);
            setStatus(
                api::Updates2StatusData::StatusCode::preparing,
                "Update has been downloaded and now is preparing for install");
            startPreparing();
            break;
    }
}

void Updates2ManagerBase::onDownloadFailed(const QString& fileName)
{

}

void Updates2ManagerBase::onFileAdded(const FileInformation& fileInformation)
{

}

void Updates2ManagerBase::onFileDeleted(const QString& fileName)
{

}

void Updates2ManagerBase::onFileInformationChanged(const FileInformation& fileInformation)
{

}

void Updates2ManagerBase::onFileInformationStatusChanged(const FileInformation& fileInformation)
{

}

void Updates2ManagerBase::onChunkDownloadFailed(const QString& fileName)
{

}

void Updates2ManagerBase::stopAsyncTasks()
{
    m_timerManager.stop();
}

} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
