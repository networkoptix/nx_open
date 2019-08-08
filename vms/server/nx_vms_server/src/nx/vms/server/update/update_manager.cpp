#include "update_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/server/root_fs.h>
#include <nx/vms/server/update/update_installer.h>
#include <media_server/media_server_module.h>
#include <api/global_settings.h>
#include <common/common_module.h>
#include <utils/common/synctime.h>

namespace nx::vms::server {

using vms::common::p2p::downloader::FileInformation;
using vms::common::p2p::downloader::Downloader;
namespace downloader = vms::common::p2p::downloader;

namespace  {

std::string toString(UpdateManager::InformationCategory category)
{
    switch (category)
    {
        case UpdateManager::InformationCategory::target: return "target";
        case UpdateManager::InformationCategory::installed: return "installed";
    }

    return {};
}

} // namespace

UpdateManager::UpdateManager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    m_installer(serverModule)
{
}

UpdateManager::~UpdateManager()
{
    m_installer.stopSync(/*clearAndReset*/false);
}

void UpdateManager::connectToSignals()
{
    connect(
        globalSettings(), &QnGlobalSettings::targetUpdateInformationChanged,
        this, &UpdateManager::onGlobalUpdateSettingChanged, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::downloadFailed,
        this, &UpdateManager::onDownloaderFailed, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::downloadFinished,
        this, &UpdateManager::onDownloaderFinished, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::fileStatusChanged,
        this, &UpdateManager::onDownloaderFileStatusChanged, Qt::QueuedConnection);
}

update::Status UpdateManager::status()
{
    update::Package package;
    update::Status updateStatus;
    statusAppropriateForDownload(&package, &updateStatus);
    return updateStatus;
}

void UpdateManager::cancel()
{
    setTargetUpdateInformation({});
}

void UpdateManager::retry(bool forceRedownload)
{
    if (forceRedownload)
    {
        nx::update::Package package;
        if (findPackage(&package) == update::FindPackageResult::ok)
            downloader()->deleteFile(package.file);

        start();
        return;
    }

    const update::Status status = this->status();
    if (!status.suitableForRetrying())
        return;

    using ErrorCode = update::Status::ErrorCode;
    switch (status.errorCode)
    {
        case ErrorCode::updatePackageNotFound:
        case ErrorCode::osVersionNotSupported:
        case ErrorCode::internalError:
        case ErrorCode::unknownError:
        case ErrorCode::applauncherError:
        case ErrorCode::invalidUpdateContents:
        case ErrorCode::noFreeSpaceToInstall:
            // We can do nothing with these cases.
            break;

        case ErrorCode::noError:
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

void UpdateManager::startUpdate(const QByteArray& content)
{
    const auto& info = QJson::deserialized<update::Information>(content);
    setTargetUpdateInformation(info);
}

bool UpdateManager::startUpdateInstallation(const QList<QnUuid>& participants)
{
    try
    {
        update::Information info = updateInformation(InformationCategory::target);
        info.participants = participants;
        info.lastInstallationRequestTime = qnSyncTime->currentMSecsSinceEpoch();
        setTargetUpdateInformation(info);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

void UpdateManager::install(const QnAuthSession& authInfo)
{
    if (m_installer.state() != UpdateInstaller::State::ok)
        return;

    m_installer.install(authInfo);
}

update::Information UpdateManager::updateInformation(
    UpdateManager::InformationCategory category) const
{
    update::Information information;
    std::optional<nx::update::FindPackageResult> result;

    switch (category)
    {
        case InformationCategory::target:
            result = nx::update::fromByteArray(
                globalSettings()->targetUpdateInformation(),
                &information,
                nullptr);
            break;
        case InformationCategory::installed:
            result = nx::update::fromByteArray(
                globalSettings()->installedUpdateInformation(),
                &information,
                nullptr);
            break;
    }

    NX_ASSERT(result);
    if (*result != nx::update::FindPackageResult::ok)
    {
        throw std::runtime_error(
            "Failed to deserialize \"" + toString(category) + "\" update information");
    }

    return information;
}

void UpdateManager::finish()
{
    globalSettings()->setInstalledUpdateInformation(globalSettings()->targetUpdateInformation());
    globalSettings()->setTargetUpdateInformation({});
    globalSettings()->synchronizeNow();
}

void UpdateManager::onGlobalUpdateSettingChanged()
{
    start();
}

void UpdateManager::onDownloaderFailed(const QString& fileName)
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok || package.file != fileName)
        return;

    m_downloaderFailDetail = DownloaderFailDetail::downloadFailed;
    downloader()->deleteFile(fileName);
}

void UpdateManager::onDownloaderFinished(const QString& fileName)
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok || package.file != fileName)
        return;

    extract();
}

void UpdateManager::onDownloaderFileStatusChanged(
    const downloader::FileInformation& fileInformation)
{
    if (fileInformation.status == FileInformation::Status::downloaded)
        onDownloaderFinished(fileInformation.name);
}

update::FindPackageResult UpdateManager::findPackage(
    update::Package* outPackage, QString* outMessage) const
{
    const auto result = update::findPackage(
        *serverModule()->commonModule(), outPackage, outMessage);
    NX_DEBUG(this, "Find package called. Result: %1, message: %2",
        (int) result, outMessage == nullptr ? "" : *outMessage);
    return result;
}

void UpdateManager::extract()
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok)
        return;

    m_installer.prepareAsync(downloader()->filePath(package.file));
}

void UpdateManager::clearDownloader(bool force)
{
    NX_DEBUG(this, "Start downloader clean up");

    const QString prefix = update::rootUpdatesDirectoryForDownloader();

    nx::update::Package package;
    findPackage(&package);

    for (const QString& file: downloader()->files())
    {
        if (!file.startsWith(prefix) || (file == package.file && !force))
            continue;

        NX_INFO(this, "Deleting file: %1", file);
        downloader()->deleteFile(file);
    }
}

bool UpdateManager::canDownloadFile(const update::Package& package, update::Status* outUpdateStatus)
{
    auto fileInformation = downloader()->fileInformation(package.file);
    const auto peerId = moduleGUID();

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
            {
                const bool result = installerState(outUpdateStatus, peerId);

                if (outUpdateStatus->code == update::Status::Code::readyToInstall)
                {
                    if (!m_installer.checkFreeSpaceForInstallation())
                    {
                        *outUpdateStatus = update::Status(
                            peerId,
                            update::Status::Code::error,
                            update::Status::ErrorCode::noFreeSpaceToInstall);
                        return false;
                    }
                }

                return result;
            }
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

    if (!m_installer.checkFreeSpace(
        downloader()->downloadsDirectory().absolutePath(),
        package.size + update::reservedSpacePadding()))
    {
        *outUpdateStatus = nx::update::Status(
            peerId, update::Status::Code::error, update::Status::ErrorCode::noFreeSpaceToDownload);
        return false;
    }

    *outUpdateStatus = nx::update::Status(
        peerId, update::Status::Code::idle, update::Status::ErrorCode::noError);

    return true;
}

bool UpdateManager::statusAppropriateForDownload(
    update::Package* outPackage, update::Status* outStatus)
{
    QString message;
    switch (findPackage(outPackage, &message))
    {
        case update::FindPackageResult::ok:
            return canDownloadFile(*outPackage, outStatus);
        case update::FindPackageResult::otherError:
            *outStatus = update::Status(
                moduleGUID(),
                update::Status::Code::error,
                update::Status::ErrorCode::updatePackageNotFound);
            outStatus->message =
                message.isEmpty() ? "Failed to find a suitable update package" : message;
            return false;
        case update::FindPackageResult::osVersionNotSupported:
            *outStatus = update::Status(
                moduleGUID(),
                update::Status::Code::error,
                update::Status::ErrorCode::osVersionNotSupported);
            return false;
        case update::FindPackageResult::noInfo:
            *outStatus = update::Status(
                moduleGUID(),
                update::Status::Code::idle);
            outStatus->message = message.isEmpty() ? "No update information found" : message;
            return false;
        case update::FindPackageResult::latestUpdateInstalled:
            *outStatus = update::Status(
                moduleGUID(),
                update::Status::Code::latestUpdateInstalled);
            return false;
        case update::FindPackageResult::notParticipant:
            *outStatus = update::Status(
                moduleGUID(),
                update::Status::Code::idle);
            return false;
    }

    NX_ASSERT(false, "Shouldn't be here");
    return false;
}

bool UpdateManager::installerState(update::Status* outUpdateStatus, const QnUuid& peerId)
{
    switch (m_installer.state())
    {
    case UpdateInstaller::State::ok:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::readyToInstall);
        return false;
    case UpdateInstaller::State::inProgress:
    case UpdateInstaller::State::idle:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::preparing);
        return false;
    case UpdateInstaller::State::cleanTemporaryFilesError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::internalError);
        return true;
    case UpdateInstaller::State::noFreeSpace:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::noFreeSpaceToExtract);
        return true;
    case UpdateInstaller::State::brokenZip:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::corruptedArchive);
        return true;
    case UpdateInstaller::State::wrongDir:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Wrong directory";
        return true;
    case UpdateInstaller::State::cantOpenFile:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Can't open file";
        return true;
    case UpdateInstaller::State::otherError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Other error";
        return true;
    case UpdateInstaller::State::stopped:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
        outUpdateStatus->message = "Installer was unexpectedly stopped";
        return true;
    case UpdateInstaller::State::busy:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
            outUpdateStatus->message = "Installer is busy";
        return true;
    case UpdateInstaller::State::unknownError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::extractionError);
            outUpdateStatus->message = "Internal installer error";
        return true;
    case UpdateInstaller::State::updateContentsError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            update::Status::ErrorCode::invalidUpdateContents);
        return true;
    }

    return true;
}

update::Status UpdateManager::start()
{
    const auto peerId = moduleGUID();

    nx::update::Package package;
    update::Status updateStatus;
    const bool shouldDownloadFromScratch = statusAppropriateForDownload(&package, &updateStatus);

    NX_DEBUG(this, "Start update: Should download from scratch: %1, package valid: %2",
        shouldDownloadFromScratch, package.isValid());

    const bool shouldClearFiles = !package.isValid() || shouldDownloadFromScratch;
    m_installer.stopSync(shouldClearFiles);
    clearDownloader(shouldClearFiles);
    if (!package.isValid())
        return updateStatus;

    if (!shouldDownloadFromScratch)
    {
        if (downloader()->fileInformation(package.file).status
                == FileInformation::Status::downloaded
            && m_installer.state() == UpdateInstaller::State::idle)
        {
            m_installer.prepareAsync(downloader()->filePath(package.file));
        }

        return updateStatus;
    }

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

vms::common::p2p::downloader::Downloader* UpdateManager::downloader()
{
    return serverModule()->findInstance<vms::common::p2p::downloader::Downloader>();
}

int64_t UpdateManager::freeSpace(const QString& path) const
{
    return serverModule()->rootFileSystem()->freeSpace(path);
}

void UpdateManager::setTargetUpdateInformation(const update::Information& information)
{
    QByteArray data;
    if (information.isValid())
        data = QJson::serialized(information);

    globalSettings()->setTargetUpdateInformation(data);
    globalSettings()->synchronizeNow();
}

} // namespace nx::vms::server
