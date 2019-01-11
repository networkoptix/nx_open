#include "common_update_manager.h"
#include <api/global_settings.h>
#include <nx/fusion/model_functions.h>
#include <common/common_module.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/utils/log/assert.h>
#include <api/runtime_info_manager.h>
#include <nx/network/socket_global.h>
#include <nx/update/common_update_installer.h>

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
        globalSettings(), &QnGlobalSettings::updateInformationChanged,
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

    bool shouldDownload = statusAppropriateForDownload(&package, &updateStatus);
    if (shouldDownload || !package.isValid())
    {
        m_downloaderFailed = false;
        for (const auto& file : downloader()->files())
        {
            if (file.startsWith("updates/"))
                downloader()->deleteFile(file);
        }
        installer()->stopSync();
    }

    if (!shouldDownload)
    {
        if (package.isValid()
            && downloader()->fileInformation(package.file).status == FileInformation::Status::downloaded
            && installer()->state() == CommonUpdateInstaller::State::idle)
        {
            installer()->prepareAsync(downloader()->filePath(package.file));
        }

        return updateStatus;
    }

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

    switch (downloader()->addFile(fileInformation))
    {
    case downloader::ResultCode::ok:
        return update::Status(
            peerId,
            update::Status::Code::downloading,
            "Started downloading update file");
    case downloader::ResultCode::fileDoesNotExist:
    case downloader::ResultCode::ioError:
    case downloader::ResultCode::invalidChecksum:
    case downloader::ResultCode::invalidFileSize:
    case downloader::ResultCode::invalidChunkIndex:
    case downloader::ResultCode::invalidChunkSize:
        m_downloaderFailed = true;
        return update::Status(
            peerId,
            update::Status::Code::error,
            "Downloader experienced internal error");
    case downloader::ResultCode::noFreeSpace:
        m_downloaderFailed = true;
        return update::Status(peerId,
            update::Status::Code::error,
            "Downloader experienced internal error");
    default:
        NX_ASSERT(false, "Unexpected Downloader::addFile() result");
        return update::Status(peerId, update::Status::Code::error, "Unknown error");
    }

    return update::Status();
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
    startUpdate("{}");
}

void CommonUpdateManager::onGlobalUpdateSettingChanged()
{
    start();
}

void CommonUpdateManager::startUpdate(const QByteArray& content)
{
    commonModule()->globalSettings()->setUpdateInformation(content);
    commonModule()->globalSettings()->synchronizeNowSync();
}

bool CommonUpdateManager::canDownloadFile(
    const QString& fileName,
    update::Status* outUpdateStatus)
{
    auto fileInformation = downloader()->fileInformation(fileName);
    const auto peerId = commonModule()->moduleGUID();

    if (m_downloaderFailed)
    {
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            "Downloader failed to download update file");
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
                QString("Downloading file %1").arg(fileInformation.name),
                fileInformation.calculateDownloadProgress());
            return false;
        case FileInformation::Status::uploading:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::downloading,
                QString("Uploading file %1").arg(fileInformation.name),
                fileInformation.calculateDownloadProgress());
            return false;
        case FileInformation::Status::downloaded:
            return installerState(outUpdateStatus, peerId);
        case FileInformation::Status::notFound:
            NX_ASSERT(false, "Unexpected state");
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                "Unexpected error");
            return true;
        case FileInformation::Status::corrupted:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::error,
                "Update file is corrupted");
            return true;
        }
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
            update::Status::Code::readyToInstall,
            "Update is ready for installation");
        return false;
    case CommonUpdateInstaller::State::inProgress:
    case CommonUpdateInstaller::State::idle:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::preparing,
            "Update file is being validated");
        return false;
    case CommonUpdateInstaller::State::cleanTemporaryFilesError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            "Failed to clean up temporary files");
        return true;
    case CommonUpdateInstaller::State::corruptedArchive:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            "Update archive is corrupted");
        return true;
    case CommonUpdateInstaller::State::noFreeSpace:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            "No enough free space on device");
        return true;
    case CommonUpdateInstaller::State::unknownError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            "Internal installer error");
        return true;
    case CommonUpdateInstaller::State::updateContentsError:
        *outUpdateStatus = update::Status(
            peerId,
            update::Status::Code::error,
            "Invalid update archive contents");
        return true;
    }

    return true;
}

update::FindPackageResult CommonUpdateManager::findPackage(
    update::Package* outPackage,
    QString* outMessage) const
{
    return update::findPackage(
        commonModule()->moduleGUID(),
        QnAppInfo::currentSystemInformation(),
        globalSettings()->updateInformation(),
        runtimeInfoManager()->localInfo().data.peer.isClient(),
        nx::network::SocketGlobals::cloud().cloudHost(),
        !globalSettings()->cloudSystemId().isEmpty(),
        outPackage,
        outMessage);
}

QList<QnUuid> CommonUpdateManager::participants() const
{
    nx::update::Information updateInformation;
    const auto deserializeResult = nx::update::fromByteArray(globalSettings()->updateInformation(),
        &updateInformation, nullptr);

    if (deserializeResult != nx::update::FindPackageResult::ok)
    {
        NX_DEBUG(this, "participants: Failed to deserialize");
        return QList<QnUuid>();
    }

    return updateInformation.participants;
}

bool CommonUpdateManager::setParticipants(const QList<QnUuid>& participants)
{
    nx::update::Information updateInformation;
    const auto deserializeResult = nx::update::fromByteArray(globalSettings()->updateInformation(),
        &updateInformation, nullptr);

    if (deserializeResult != nx::update::FindPackageResult::ok)
    {
        NX_DEBUG(this, "setParticipants: Failed to deserialize");
        return false;
    }

    updateInformation.participants = participants;
    QByteArray serializedUpdateInformation;

    QJson::serialize(updateInformation, &serializedUpdateInformation);
    globalSettings()->setUpdateInformation(serializedUpdateInformation);
    globalSettings()->synchronizeNowSync();

    return true;
}

bool CommonUpdateManager::statusAppropriateForDownload(
    nx::update::Package* outPackage,
    update::Status* outStatus)
{
    QString message;
    switch (findPackage(outPackage, &message))
    {
        case update::FindPackageResult::ok:
            return canDownloadFile(outPackage->file, outStatus);
        case update::FindPackageResult::otherError:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::error,
                message.isEmpty() ? "Failed to find a suitable update package" : message);
            return false;
        case update::FindPackageResult::noInfo:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::idle,
                message.isEmpty() ? "No update information found" : message);
            return false;
        case update::FindPackageResult::latestUpdateInstalled:
            *outStatus = update::Status(
                commonModule()->moduleGUID(),
                update::Status::Code::latestUpdateInstalled,
                message);
            return false;
        case update::FindPackageResult::notParticipant:
            *outStatus = update::Status(
            commonModule()->moduleGUID(),
                update::Status::Code::idle,
                message);
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

    m_downloaderFailed = true;
    downloader()->deleteFile(fileName);
}

void CommonUpdateManager::onDownloaderFinished(const QString& fileName)
{
    nx::update::Package package;
    if (findPackage(&package) != update::FindPackageResult::ok || package.file != fileName)
        return;

    installer()->prepareAsync(downloader()->filePath(fileName));
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
