#include "common_update_manager.h"
#include <api/global_settings.h>
#include <nx/fusion/model_functions.h>
#include <common/common_module.h>
#include <nx/utils/log/assert.h>
#include <nx/update/common_update_installer.h>
#include <utils/common/synctime.h>

namespace nx {

using vms::common::p2p::downloader::FileInformation;
using vms::common::p2p::downloader::Downloader;
namespace downloader = vms::common::p2p::downloader;

CommonUpdateManager::CommonUpdateManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void CommonUpdateManager::connectToSignals()
{
    connect(
        globalSettings(), &QnGlobalSettings::targetUpdateInformationChanged,
        this, &CommonUpdateManager::onGlobalUpdateSettingChanged, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::downloadFailed,
        this, &CommonUpdateManager::onDownloaderFailed, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::downloadFinished,
        this, &CommonUpdateManager::onDownloaderFinished, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::fileStatusChanged,
        this, &CommonUpdateManager::onDownloaderFileStatusChanged, Qt::QueuedConnection);
}

update::Status CommonUpdateManager::start()
{
    const auto peerId = commonModule()->moduleGUID();

    nx::update::Package package;
    update::Status updateStatus;
    const bool shouldDownloadFromScratch = statusAppropriateForDownload(&package, &updateStatus);

    NX_DEBUG(this, "Start update: Should download from scratch: %1, package valid: %2",
        shouldDownloadFromScratch, package.isValid());

    installer()->stopSync();
    if (!package.isValid())
        return updateStatus;

    if (!shouldDownloadFromScratch)
    {
        if (downloader()->fileInformation(package.file).status
                == FileInformation::Status::downloaded
            && installer()->state() == CommonUpdateInstaller::State::idle)
        {
            installer()->prepareAsync(downloader()->filePath(package.file));
        }

        return updateStatus;
    }

    downloader()->deleteFile(package.file);
    m_downloaderFailDetail = DownloaderFailDetail::noError;

    FileInformation fileInformation;
    fileInformation.name = package.file;
    fileInformation.md5 = QByteArray::fromHex(package.md5.toLatin1());
    fileInformation.size = package.size;
    fileInformation.url = package.url;

    auto downloaderPeers = globalSettings()->downloaderPeers();
    if (downloaderPeers.contains(fileInformation.name))
    {
        fileInformation.additionalPeers = downloaderPeers[fileInformation.name];
        fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::byPlatform;
    }
    else
    {
        fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::all;
    }

    const auto addFileResult = downloader()->addFile(fileInformation);
    NX_DEBUG(this, "Downloader::addFile (%1) called. Result is: %2",
        fileInformation.name, addFileResult);

    switch (addFileResult)
    {
        case downloader::ResultCode::ok:
            return update::Status(peerId, update::Status::Code::downloading);

        case downloader::ResultCode::fileDoesNotExist:
        case downloader::ResultCode::ioError:
        case downloader::ResultCode::invalidChecksum:
        case downloader::ResultCode::invalidFileSize:
        case downloader::ResultCode::invalidChunkIndex:
        case downloader::ResultCode::invalidChunkSize:
            m_downloaderFailDetail = DownloaderFailDetail::internalError;
            return update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::internalDownloaderError);

        case downloader::ResultCode::noFreeSpace:
            m_downloaderFailDetail = DownloaderFailDetail::noFreeSpace;
            return update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::noFreeSpaceToDownload);

        default:
            NX_ASSERT(false, "Unexpected Downloader::addFile() result");
            m_downloaderFailDetail = DownloaderFailDetail::internalError;
                return update::Status(
                    peerId,
                    update::Status::Code::error,
                    update::Status::ErrorCode::unknownError);
    }
}

update::Status CommonUpdateManager::status()
{
    update::Package package;
    update::Status updateStatus;
    statusAppropriateForDownload(&package, &updateStatus);
    return updateStatus;
}

void CommonUpdateManager::cancel()
{
    startUpdate("");
}

void CommonUpdateManager::retry(bool forceRedownload)
{
    if (forceRedownload)    {
        nx::update::Package package;
        if (findPackage(&package) == update::FindPackageResult::ok)
            downloader()->deleteFile(package.file);

        start();
        return;
    }

    const update::Status status = this->status();
    if (status.code != update::Status::Code::error)
        return;

    using ErrorCode = update::Status::ErrorCode;
    switch (status.errorCode)
    {
        case ErrorCode::noError:
        case ErrorCode::updatePackageNotFound:
        case ErrorCode::internalError:
        case ErrorCode::unknownError:
        case ErrorCode::applauncherError:
        case ErrorCode::invalidUpdateContents:
            // We can do nothing with these cases.
            break;

        case ErrorCode::noFreeSpaceToDownload:
        case ErrorCode::downloadFailed:
        case ErrorCode::corruptedArchive:
        case ErrorCode::internalDownloaderError:
            start();
            return;

        case ErrorCode::noFreeSpaceToExtract:
        case ErrorCode::extractionError:
            extract();
            break;
    }
}

void CommonUpdateManager::finish()
{
    commonModule()->globalSettings()->setInstalledUpdateInformation(
        commonModule()->globalSettings()->targetUpdateInformation());
    commonModule()->globalSettings()->synchronizeNowSync();
    startUpdate("");
}

void CommonUpdateManager::onGlobalUpdateSettingChanged()
{
    clearDownloader();
    start();
}

void CommonUpdateManager::startUpdate(const QByteArray& content)
{
    commonModule()->globalSettings()->setTargetUpdateInformation(content);
    commonModule()->globalSettings()->synchronizeNowSync();
}

bool CommonUpdateManager::canDownloadFile(
    const nx::update::Package& package, update::Status* outUpdateStatus)
{
    auto fileInformation = downloader()->fileInformation(package.file);
    const auto peerId = commonModule()->moduleGUID();

    switch (m_downloaderFailDetail)
    {
        case DownloaderFailDetail::noError:
            break;
        case DownloaderFailDetail::internalError:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::internalDownloaderError);
            return true;
        case DownloaderFailDetail::downloadFailed:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::downloadFailed);
            return true;
        case DownloaderFailDetail::noFreeSpace:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::noFreeSpaceToDownload);
            return true;
    }

    if (fileInformation.isValid())
    {
        switch (fileInformation.status)
        {
        case FileInformation::Status::downloading:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::downloading,
                update::Status::ErrorCode::noError,
                fileInformation.calculateDownloadProgress());
            return false;
        case FileInformation::Status::uploading:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::downloading,
                update::Status::ErrorCode::noError,
                fileInformation.calculateDownloadProgress());
            return false;
        case FileInformation::Status::downloaded:
            return installerState(outUpdateStatus, peerId);
        case FileInformation::Status::notFound:
            NX_ASSERT(false, "Unexpected state");
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::unknownError);
            return true;
        case FileInformation::Status::corrupted:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                update::Status::ErrorCode::corruptedArchive);
            return true;
        }
    }

    const int64_t requiredSpace = package.size * 2 * 1.2;
    const int64_t deviceFreeSpace = freeSpace(installer()->dataDirectoryPath());
    NX_DEBUG(
        this,
        "Checking if there is enough space to download and install the update package. Required space: %1Mb, free space on device: %2Mb",
        requiredSpace / (1024 * 1024), deviceFreeSpace / (1024 * 1024));
    if (deviceFreeSpace < requiredSpace)
    {
        NX_WARNING(
            this,
            "Can't start downloading an update package because lack of free space on disk. " \
            "Required: %1 Mb, free Space: %2 Mb",
            static_cast<double>(requiredSpace) / (1024 * 1024),
            static_cast<double>(deviceFreeSpace) / (1024 * 1024));
        *outUpdateStatus = nx::update::Status(
            peerId, update::Status::Code::error, update::Status::ErrorCode::noFreeSpaceToDownload);
        return false;
    }

    return true;
}

bool CommonUpdateManager::installerState(update::Status* outUpdateStatus, const QnUuid& peerId)
{
    switch (installer()->state())
    {
    case CommonUpdateInstaller::State::ok:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::readyToInstall);
        return false;
    case CommonUpdateInstaller::State::inProgress:
    case CommonUpdateInstaller::State::idle:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::preparing);
        return false;
    case CommonUpdateInstaller::State::cleanTemporaryFilesError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::internalError);
        return true;
    case CommonUpdateInstaller::State::noFreeSpace:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::noFreeSpaceToExtract);
        return true;
    case CommonUpdateInstaller::State::brokenZip:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::corruptedArchive);
        return true;
    case CommonUpdateInstaller::State::wrongDir:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Wrong directory";
        return true;
    case CommonUpdateInstaller::State::cantOpenFile:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Can't open file";
        return true;
    case CommonUpdateInstaller::State::otherError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Other error";
        return true;
    case CommonUpdateInstaller::State::stopped:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Installer was unexpectedly stopped";
        return true;
    case CommonUpdateInstaller::State::busy:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
            outUpdateStatus->message = "Installer is busy";
        return true;
    case CommonUpdateInstaller::State::unknownError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
            outUpdateStatus->message = "Internal installer error";
        return true;
    case CommonUpdateInstaller::State::updateContentsError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::invalidUpdateContents);
        return true;
    }

    return true;
}

update::FindPackageResult CommonUpdateManager::findPackage(
    update::Package* outPackage,
    QString* outMessage) const
{
    const auto result = update::findPackage(*commonModule(), outPackage, outMessage);
    NX_DEBUG(this, "Find package called. Result: %1, message: %2",
        (int) result, outMessage == nullptr ? "" : *outMessage);
    return result;
}

void CommonUpdateManager::extract()
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok)
        return;

    installer()->prepareAsync(downloader()->filePath(package.file));
}

void CommonUpdateManager::clearDownloader()
{
    NX_DEBUG(this, "Start downloader clean up");

    const QString prefix = update::rootUpdatesDirectoryForDownloader();

    nx::update::Package package;
    findPackage(&package);

    for (const QString& file: downloader()->files())
    {
        if (!file.startsWith(prefix) || file == package.file) //< Can be empty.
            continue;

        NX_INFO(this, "Deleting file: %1", file);
        downloader()->deleteFile(file);
    }
}

bool CommonUpdateManager::deserializedUpdateInformation(
    update::Information* outUpdateInformation, const QString& caller) const
{
    const auto deserializeResult = nx::update::fromByteArray(
        globalSettings()->targetUpdateInformation(), outUpdateInformation, nullptr);

    if (deserializeResult != nx::update::FindPackageResult::ok)
    {
        NX_DEBUG(this, "%1: Failed to deserialize", caller);
        return false;
    }

    return true;
}

bool CommonUpdateManager::participants(QList<QnUuid>* outParticipants) const
{
    nx::update::Information updateInformation;
    if (!deserializedUpdateInformation(&updateInformation, "participants"))
        return false;

    *outParticipants = updateInformation.participants;
    return true;
}

vms::api::SoftwareVersion CommonUpdateManager::targetVersion() const
{
    nx::update::Information updateInformation;
    if (!deserializedUpdateInformation(&updateInformation, "targetVersion"))
        return vms::api::SoftwareVersion();

    return vms::api::SoftwareVersion(updateInformation.version);
}

bool CommonUpdateManager::setParticipants(const QList<QnUuid>& participants)
{
    nx::update::Information updateInformation;
    if (!deserializedUpdateInformation(&updateInformation, "setParticipants"))
        return false;

    updateInformation.participants = participants;
    setUpdateInformation(updateInformation);

    return true;
}

void CommonUpdateManager::setUpdateInformation(const update::Information& updateInformation)
{
    QByteArray serializedUpdateInformation;

    QJson::serialize(updateInformation, &serializedUpdateInformation);
    globalSettings()->setTargetUpdateInformation(serializedUpdateInformation);
    globalSettings()->synchronizeNowSync();
}

bool CommonUpdateManager::updateLastInstallationRequestTime()
{
    nx::update::Information updateInformation;
    const auto deserializeResult = nx::update::fromByteArray(globalSettings()->targetUpdateInformation(),
        &updateInformation, nullptr);

    if (deserializeResult != nx::update::FindPackageResult::ok)
    {
        NX_DEBUG(this, "updateLastInstallationRequestTime: Failed to deserialize");
        return false;
    }

    updateInformation.lastInstallationRequestTime = qnSyncTime->currentMSecsSinceEpoch();
    setUpdateInformation(updateInformation);

    return true;
}

bool CommonUpdateManager::statusAppropriateForDownload(
    nx::update::Package* outPackage, update::Status* outStatus)
{
    QString message;
    switch (findPackage(outPackage, &message))
    {
        case update::FindPackageResult::ok:
            return canDownloadFile(*outPackage, outStatus);
        case update::FindPackageResult::otherError:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::error,
                update::Status::ErrorCode::updatePackageNotFound);
            outStatus->message =
                message.isEmpty() ? "Failed to find a suitable update package" : message;
            return false;
        case update::FindPackageResult::noInfo:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::idle);
            outStatus->message = message.isEmpty() ? "No update information found" : message;
            return false;
        case update::FindPackageResult::latestUpdateInstalled:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::latestUpdateInstalled);
            return false;
        case update::FindPackageResult::notParticipant:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::idle);
            return false;
    }

    NX_ASSERT(false, "Shouldn't be here");
    return false;
}

void CommonUpdateManager::onDownloaderFailed(const QString& fileName)
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok || package.file != fileName)
        return;

    m_downloaderFailDetail = DownloaderFailDetail::downloadFailed;
    downloader()->deleteFile(fileName);
}

void CommonUpdateManager::onDownloaderFinished(const QString& fileName)
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok || package.file != fileName)
        return;

    extract();
}

void CommonUpdateManager::onDownloaderFileStatusChanged(const FileInformation& fileInformation)
{
    if (fileInformation.status == FileInformation::Status::downloaded)
        onDownloaderFinished(fileInformation.name);
}

void CommonUpdateManager::install(const QnAuthSession& authInfo)
{
    if (installer()->state() != CommonUpdateInstaller::State::ok)
        return;

    installer()->install(authInfo);
}

} // namespace nx
