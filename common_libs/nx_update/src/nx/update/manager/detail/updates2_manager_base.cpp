#include "updates2_manager_base.h"
#include "update_request_data_factory.h"
#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/sync_update_checker.h>
#include <nx/update/info/update_registry_factory.h>
#include <common/common_module.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <api/global_settings.h>
#include <nx/vms/common/p2p/downloader/downloader.h>


namespace nx {
namespace update {
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
            UpdateFileRequestDataFactory::create(),
            &newRegistryServerVersion) != update::info::ResultCode::ok)
    {
        return false;
    }

    QnSoftwareVersion oldRegistryServerVersion;
    if (oldRegistry->latestUpdate(
            UpdateFileRequestDataFactory::create(),
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
    NX_ASSERT(m_currentStatus.files.size() <= 1);
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
        case api::Updates2StatusData::StatusCode::preparing:
            setStatus(
                api::Updates2StatusData::StatusCode::available,
                "Update is available");
            download();
            break;
        case api::Updates2StatusData::StatusCode::readyToInstall:
        case api::Updates2StatusData::StatusCode::installing:
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
                m_currentStatus.fromBase(
                    api::Updates2StatusData(
                        moduleGuid(),
                        api::Updates2StatusData::StatusCode::checking,
                        "Checking for update"));
                break;
            default:
                return;
        }
    }

    auto remoteRegistry = getRemoteRegistry();
    if (remoteRegistry && isNewRegistryBetter(getGlobalRegistry(), remoteRegistry))
        updateGlobalRegistry(remoteRegistry->toByteArray());

    swapRegistries(std::move(remoteRegistry));
    refreshStatusAfterCheck();
}

void Updates2ManagerBase::swapRegistries(update::info::AbstractUpdateRegistryPtr otherRegistry)
{
    if (!otherRegistry)
        return;

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
                detail::UpdateFileRequestDataFactory::create(), &fileData);
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
        detail::UpdateFileRequestDataFactory::create(), &fileData);
    NX_ASSERT(result == update::info::ResultCode::ok);

    if (result != update::info::ResultCode::ok)
    {
        setStatus(
            api::Updates2StatusData::StatusCode::error,  "Failed to get update file information");
        return m_currentStatus.base();
    }

    using namespace vms::common::p2p::downloader;

    FileInformation fileInformation;
    fileInformation.md5 = QByteArray::fromHex(fileData.md5.toBase64());
    fileInformation.name = fileData.file;
    fileInformation.size = fileData.size;
    fileInformation.url = fileData.url;
    fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::byPlatform;

    downloader()->deleteFile(fileData.file);
    for (const auto& fileName : m_currentStatus.files)
        downloader()->deleteFile(fileName);

    ResultCode resultCode = downloader()->addFile(fileInformation);
    switch (resultCode)
    {
        case ResultCode::fileAlreadyDownloaded:
        case ResultCode::fileAlreadyExists:
            NX_ASSERT(false);
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                lit("Downloader internal error: File exists after preliminary deleting: %1")
                    .arg(fileData.file));
            break;
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
        case ResultCode::invalidChunkIndex:
        case ResultCode::invalidChunkSize:
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                lit("Downloader failed to start downloading: %1. Result code: %2")
                    .arg(fileData.file).arg(static_cast<int>(resultCode)));
            m_currentStatus.files.remove(fileData.file);
            break;
        case ResultCode::noFreeSpace:
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                lit("No free space on device. Failed to download: %1").arg(fileData.file));
            m_currentStatus.files.remove(fileData.file);
            break;
    }

    return m_currentStatus.base();
}

void Updates2ManagerBase::setStatus(
    api::Updates2StatusData::StatusCode code,
    const QString& message)
{
    detail::Updates2StatusDataEx newStatusData(
        QDateTime::currentMSecsSinceEpoch(),
        moduleGuid(),
        code,
        message);

    writeStatusToFile(newStatusData);

    m_currentStatus.lastRefreshTime = newStatusData.lastRefreshTime;
    m_currentStatus.message = newStatusData.message;
    m_currentStatus.progress = newStatusData.progress;
    m_currentStatus.serverId = newStatusData.serverId;
    m_currentStatus.state = newStatusData.state;
}

void Updates2ManagerBase::startPreparing(const QString& updateFilePath)
{
    installer()->prepareAsync(
        updateFilePath,
        [this](PrepareResult prepareResult)
        {
            switch (prepareResult)
            {
                case PrepareResult::ok:
                    return setStatus(
                        api::Updates2StatusData::StatusCode::readyToInstall,
                        lit("Update is ready for installation"));
                case PrepareResult::corruptedArchive:
                    return setStatus(
                        api::Updates2StatusData::StatusCode::error,
                        lit("Update archive is corrupted"));
                case PrepareResult::noFreeSpace:
                    return setStatus(
                        api::Updates2StatusData::StatusCode::error,
                        lit("Failed to prepare update file, no space left on device"));
                case PrepareResult::cleanTemporaryFilesError:
                    return setStatus(
                        api::Updates2StatusData::StatusCode::error,
                        lit("Failed to remove temporary files"));
                case PrepareResult::updateContentsError:
                    return setStatus(
                        api::Updates2StatusData::StatusCode::error,
                        lit("Update contents are not valid"));
                case PrepareResult::unknownError:
                    return setStatus(
                        api::Updates2StatusData::StatusCode::error,
                        lit("Failed to prepare update files"));
                case PrepareResult::alreadyStarted:
                    return;
            }
        });
}

void Updates2ManagerBase::onDownloadFinished(const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);
    using namespace vms::common::p2p::downloader;

    auto onError =
        [this, &fileName](const QString& errorMessage)
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

    NX_ASSERT(fileInformation.status == FileInformation::Status::downloaded);
    setStatus(
        api::Updates2StatusData::StatusCode::preparing,
        "Update has been downloaded and now is preparing for install");
    startPreparing(downloader()->filePath(fileName));
}

void Updates2ManagerBase::onDownloadFailed(const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);
    using namespace vms::common::p2p::downloader;

    auto onError =
        [this, &fileName](const QString& errorMessage)
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
        case FileInformation::Status::downloading:
            onError(lit("Failed to find update file: %1").arg(fileName));
            break;
        case FileInformation::Status::corrupted:
            onError(lit("Update file is corrupted: %1").arg(fileName));
            break;
        default:
            NX_ASSERT(false, "Wrong file information state");
            break;
    }

    downloader()->deleteFile(fileName, /*deleteData*/ true);
    m_currentStatus.files.remove(fileName);
}

void Updates2ManagerBase::onFileAdded(const FileInformation& /*fileInformation*/)
{
    // #TODO #akulikov implement
}

void Updates2ManagerBase::onFileDeleted(const QString& /*fileName*/)
{
    // #TODO #akulikov implement
}

void Updates2ManagerBase::onFileInformationChanged(const FileInformation& /*fileInformation*/)
{
    // #TODO #akulikov implement
}

void Updates2ManagerBase::onFileInformationStatusChanged(const FileInformation& fileInformation)
{
    if (fileInformation.status == FileInformation::Status::corrupted)
        onDownloadFailed(fileInformation.name);
}

void Updates2ManagerBase::onChunkDownloadFailed(const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_currentStatus.files.contains(fileName))
        return;

    NX_ASSERT(m_currentStatus.files.size() == 1);
    NX_ASSERT(m_currentStatus.state == api::Updates2StatusData::StatusCode::downloading);

    setStatus(
        api::Updates2StatusData::StatusCode::downloading,
        lit("Problems detected while downloading update file: %1. Trying to recover")
            .arg(fileName));
}

void Updates2ManagerBase::stopAsyncTasks()
{
    m_timerManager.stop();
}

api::Updates2StatusData Updates2ManagerBase::install()
{
    QnMutexLocker lock(&m_mutex);
    if (m_currentStatus.state != api::Updates2StatusData::StatusCode::readyToInstall)
        return m_currentStatus.base();

    if (installer()->install())
        setStatus(api::Updates2StatusData::StatusCode::installing, "Installing update");
    else
        setStatus(api::Updates2StatusData::StatusCode::error, "Error while installing update");

    return m_currentStatus.base();
}

} // namespace detail
} // namespace update
} // namespace nx
