#include "updates2_manager_base.h"
#include "update_request_data_factory.h"
#include <nx/api/updates2/updates2_status_data.h>
#include <nx/update/info/sync_update_checker.h>
#include <nx/update/info/update_registry_factory.h>
#include <nx/update/info/manual_file_data.h>
#include <common/common_module.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/literal.h>
#include <api/global_settings.h>
#include <nx/vms/common/p2p/downloader/downloader.h>


namespace nx {
namespace update {
namespace manager {
namespace detail {

using namespace vms::common::p2p::downloader;

Updates2ManagerBase::Updates2ManagerBase()
{
}

void Updates2ManagerBase::atServerStart()
{
    using namespace std::placeholders;

    m_updateRegistry = info::UpdateRegistryFactory::create(peerId());
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
        case api::Updates2StatusData::StatusCode::installing:
        case api::Updates2StatusData::StatusCode::downloading:
        case api::Updates2StatusData::StatusCode::preparing:
            setStatus(
                api::Updates2StatusData::StatusCode::notAvailable,
                "Update is unavailable");
            break;
        case api::Updates2StatusData::StatusCode::readyToInstall:
            break;
    }

    connectToSignals();

    m_timerManager.addNonStopTimer(
        std::bind(&Updates2ManagerBase::checkForRemoteUpdate, this, _1, false),
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

    {
        QnMutexLocker lock(&m_mutex);
        m_updateRegistry->merge(globalRegistry.get());
    }

    refreshStatusAfterCheck();
}

void Updates2ManagerBase::checkForRemoteUpdate(utils::TimerId /*timerId*/, bool forced)
{
    auto onExitGuard = makeScopeGuard([this]() { remoteUpdateCompleted(); });
    if (!forced)
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
    auto globalRegistry = getGlobalRegistry();
    QByteArray serializedUpdatedRegistry;
    {
        QnMutexLocker lock(&m_mutex);
        m_updateRegistry->merge(remoteRegistry.get());
        if (!globalRegistry || !m_updateRegistry->equals(globalRegistry.get()))
        {
            m_updateRegistry->merge(globalRegistry.get());
            serializedUpdatedRegistry = m_updateRegistry->toByteArray();
        }
    }

    if (!serializedUpdatedRegistry.isNull())
        updateGlobalRegistry(serializedUpdatedRegistry);

    refreshStatusAfterCheck();
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
            QList<api::TargetVersionWithEula> targetVersions;
            if (!m_updateRegistry)
            {
                setStatus(
                    api::Updates2StatusData::StatusCode::error,
                    "Failed to get updates data");
                return;
            }

            if (m_updateRegistry->latestUpdate(
                    detail::UpdateRequestDataFactory::create(isClient(), /* targetVersion */ nullptr),
                    &targetVersions) != update::info::ResultCode::ok)
            {
                setStatus(
                    api::Updates2StatusData::StatusCode::notAvailable,
                    "Update is unavailable");
                return;
            }

            info::ResultCode result;
            nx::update::info::FileData fileData;

            for (int i = 0; i < targetVersions.size(); ++i)
            {
                result = m_updateRegistry->findUpdateFile(
                    detail::UpdateRequestDataFactory::create(
                        isClient(),
                        &targetVersions[i].targetVersion),
                    &fileData);
                if (result == info::ResultCode::ok)
                    break;
            }

            NX_ASSERT(result == update::info::ResultCode::ok);
            NX_ASSERT(!fileData.isNull());

            if (result != update::info::ResultCode::ok || fileData.isNull())
            {
                setStatus(
                    api::Updates2StatusData::StatusCode::error,
                    "Failed to get update file information");
                return;
            }

            setStatus(
                api::Updates2StatusData::StatusCode::available,
                "Update is available",
                targetVersions);
            break;
        }
        default:
            return;
    }
}

api::Updates2StatusData Updates2ManagerBase::download(const QnSoftwareVersion& targetVersion)
{
    QnMutexLocker lock(&m_mutex);
    if (m_currentStatus.state != api::Updates2StatusData::StatusCode::available)
        return m_currentStatus.base();

    NX_ASSERT((bool) m_updateRegistry);

    nx::update::info::FileData fileData;
    const auto result = m_updateRegistry->findUpdateFile(
        detail::UpdateRequestDataFactory::create(isClient(), &targetVersion),
        &fileData);
    NX_ASSERT(result == update::info::ResultCode::ok);

    if (result != update::info::ResultCode::ok)
    {
        setStatus(api::Updates2StatusData::StatusCode::error,
            "Failed to get update file information");
        return m_currentStatus.base();
    }

    using namespace vms::common::p2p::downloader;
    NX_ASSERT(!fileData.isNull());
    if (fileData.isNull())
    {
        setStatus(
            api::Updates2StatusData::StatusCode::error,
            "File data is empty");
        return m_currentStatus.base();
    }

    auto fileInformation = downloader()->fileInformation(fileData.file);
    bool alreadyHasFileInDownLoader = fileInformation.isValid();
    if (!alreadyHasFileInDownLoader)
    {
        NX_ASSERT(!fileData.md5.isNull());
        NX_ASSERT(fileData.size != 0);
        if (fileData.md5.isNull() || fileData.size == 0)
        {
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                "File data is corrupted");
            return m_currentStatus.base();
        }

        fileInformation.md5 = QByteArray::fromHex(fileData.md5.toBase64());
        fileInformation.name = fileData.file;
        fileInformation.size = fileData.size;
        fileInformation.url = fileData.url;

        downloader()->deleteFile(fileData.file);
        if (!m_currentStatus.file.isEmpty())
            downloader()->deleteFile(m_currentStatus.file);

        m_currentStatus.file.clear();
    }
    fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::byPlatform;
    fileInformation.additionalPeers = m_updateRegistry->additionalPeers(fileInformation.name);

    ResultCode resultCode = downloader()->addFile(fileInformation);
    switch (resultCode)
    {
        case ResultCode::fileAlreadyDownloaded:
        case ResultCode::fileAlreadyExists:
            if (alreadyHasFileInDownLoader)
            {
                switch (fileInformation.status)
                {
                    case FileInformation::Status::downloaded:
                    case FileInformation::Status::uploading:
                        setStatus(
                            api::Updates2StatusData::StatusCode::preparing,
                            "Update has been downloaded and now is preparing for install");
                        startPreparing(downloader()->filePath(fileInformation.name));
                        break;
                    case FileInformation::Status::corrupted:
                        setStatus(
                            api::Updates2StatusData::StatusCode::error,
                            QString("Update file is corrupted: %1").arg(fileInformation.name));
                        downloader()->deleteFile(fileInformation.name, /*deleteData*/ true);
                        m_currentStatus.file.clear();
                        m_updateRegistry->removeFileData(fileInformation.name);
                        break;
                    case FileInformation::Status::downloading:
                        setStatus(
                            api::Updates2StatusData::StatusCode::downloading,
                            lit("Downloading update file: %1").arg(fileInformation.name));
                        m_currentStatus.file = fileInformation.name;
                        break;
                    case FileInformation::Status::notFound:
                        NX_ASSERT(false);
                        break;
                }
                break;
            }
            NX_ASSERT(false);
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                lit("Downloader internal error: File exists after preliminary deleting: %1")
                    .arg(fileInformation.name));
            break;
        case ResultCode::ok:
            setStatus(
                api::Updates2StatusData::StatusCode::downloading,
                lit("Downloading update file: %1").arg(fileInformation.name));
            m_currentStatus.file = fileInformation.name;
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
                    .arg(fileInformation.name).arg(static_cast<int>(resultCode)));
            m_currentStatus.file.clear();
            break;
        case ResultCode::noFreeSpace:
            setStatus(
                api::Updates2StatusData::StatusCode::error,
                lit("No free space on device. Failed to download: %1").arg(fileInformation.name));
            m_currentStatus.file.clear();
            break;
    }

    return m_currentStatus.base();
}

void Updates2ManagerBase::setStatus(
    api::Updates2StatusData::StatusCode code,
    const QString& message,
    const QList<api::TargetVersionWithEula> targets,
    double progress)
{
    detail::Updates2StatusDataEx newStatusData(
        QDateTime::currentMSecsSinceEpoch(),
        moduleGuid(),
        code,
        message,
        targets,
        progress);

    writeStatusToFile(newStatusData);

    m_currentStatus.lastRefreshTime = newStatusData.lastRefreshTime;
    m_currentStatus.message = newStatusData.message;
    m_currentStatus.targets = targets;
    m_currentStatus.progress = newStatusData.progress;
    m_currentStatus.serverId = newStatusData.serverId;
    m_currentStatus.state = newStatusData.state;
}

void Updates2ManagerBase::startPreparing(const QString& updateFilePath)
{
    using namespace installer::detail;
    installer()->prepareAsync(
        updateFilePath,
        [this](PrepareResult prepareResult)
        {
            QnMutexLocker lock(&m_mutex);
            if (m_currentStatus.state != api::Updates2StatusData::StatusCode::preparing)
                return;

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

    if (m_currentStatus.file != fileName)
        return;

    auto onExit = makeScopeGuard([this]() { m_currentStatus.file.clear(); });

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

    if (m_currentStatus.file != fileName)
        return;

    auto onExit = makeScopeGuard(
        [this, fileName]()
        {
            m_currentStatus.file.clear();
            downloader()->deleteFile(fileName, /*deleteData*/ true);
        });

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
}

void Updates2ManagerBase::onFileAdded(const FileInformation& fileInformation)
{
    auto manualFileData = update::info::ManualFileData::fromFileName(fileInformation.name);
    if (manualFileData.isNull())
        return;

    if (fileInformation.status == FileInformation::Status::notFound
        || fileInformation.status == FileInformation::Status::corrupted)
    {
        NX_VERBOSE(
            this,
            lm("External update file %1 added, but has an inappropriate status")
                .arg(fileInformation.name));
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);
        NX_VERBOSE(
            this,
            lm("External update file %1 was added to the Downloader").args(fileInformation.name));

        manualFileData.peers.append(peerId());
        m_updateRegistry->addFileData(manualFileData);
    }

    checkForRemoteUpdate(utils::TimerId(), /* forced */ true);
}

void Updates2ManagerBase::onFileDeleted(const QString& fileName)
{
    auto manualFileData = update::info::ManualFileData::fromFileName(fileName);
    if (manualFileData.isNull())
        return;

    {
        QnMutexLocker lock(&m_mutex);
        NX_VERBOSE(
            this,
            lm("External update file %1 was removed from the Downloader").args(fileName));

        m_updateRegistry->removeFileData(fileName);
    }

    checkForRemoteUpdate(utils::TimerId(), /* forced */ true);
}

void Updates2ManagerBase::onFileInformationChanged(const FileInformation& fileInformation)
{
    QnMutexLocker lock(&m_mutex);
    if (m_currentStatus.file != fileInformation.name)
        return;

    if (fileInformation.status == FileInformation::Status::downloading)
    {
        setStatus(
            api::Updates2StatusData::StatusCode::downloading,
            lit("Downloading update file: %1").arg(fileInformation.name), QList<api::TargetVersionWithEula>(),
            (double) fileInformation.downloadedChunks.count(true) / fileInformation.downloadedChunks.size());
    }
}

void Updates2ManagerBase::onFileInformationStatusChanged(const FileInformation& fileInformation)
{
    if (fileInformation.status == FileInformation::Status::corrupted)
        onDownloadFailed(fileInformation.name);
}

void Updates2ManagerBase::onChunkDownloadFailed(const QString& fileName)
{
    QnMutexLocker lock(&m_mutex);
    if (m_currentStatus.file != fileName)
        return;

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

api::Updates2StatusData Updates2ManagerBase::cancel()
{
    QnMutexLocker lock(&m_mutex);
    auto clearInternals =
        [this]()
        {
            if (!m_currentStatus.file.isEmpty())
            {
                m_updateRegistry->removeFileData(m_currentStatus.file);
                downloader()->deleteFile(m_currentStatus.file);
                m_currentStatus.file.clear();
            }
            setStatus(
                api::Updates2StatusData::StatusCode::notAvailable,
                "Update is unavailable");
        };

    switch (m_currentStatus.state)
    {
    case api::Updates2StatusData::StatusCode::notAvailable:
        break;
    case api::Updates2StatusData::StatusCode::available:
    case api::Updates2StatusData::StatusCode::error:
    case api::Updates2StatusData::StatusCode::checking:
        clearInternals();
        break;
    case api::Updates2StatusData::StatusCode::downloading:
        NX_ASSERT(!m_currentStatus.file.isEmpty());
        clearInternals();
        break;
    case api::Updates2StatusData::StatusCode::readyToInstall:
        clearInternals();
        break;
    case api::Updates2StatusData::StatusCode::preparing:
        break;
    case api::Updates2StatusData::StatusCode::installing:
        break;
    }

    return m_currentStatus.base();
}

api::Updates2StatusData Updates2ManagerBase::check()
{
    m_timerManager.addTimer(
        [this](utils::TimerId timerId) { checkForRemoteUpdate(timerId, /*forced*/ false); },
        std::chrono::milliseconds(1));
    QnMutexLocker lock(&m_mutex);
    return m_currentStatus.base();
}

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
