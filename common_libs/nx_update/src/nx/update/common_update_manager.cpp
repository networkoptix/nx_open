#include "common_update_manager.h"
#include <api/global_settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/update/update_check.h>
#include <common/common_module.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/utils/log/assert.h>
#include <api/runtime_info_manager.h>
#include <nx/update/common_update_installer.h>

namespace nx {

namespace {

const static QString kNotFoundMessage = "No update information found";

} // namespace

CommonUpdateManager::CommonUpdateManager(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void CommonUpdateManager::connectToSignals()
{
    connect(
        globalSettings(), &QnGlobalSettings::updates2RegistryChanged,
        this, &CommonUpdateManager::onGlobalUpdateSettingChanged, Qt::QueuedConnection);

    using namespace vms::common::p2p::downloader;
    connect(
        downloader(), &Downloader::downloadFailed,
        this, &CommonUpdateManager::onDownloaderFailed, Qt::QueuedConnection);

    connect(
        downloader(), &Downloader::downloadFinished,
        this, &CommonUpdateManager::onDownloaderFinished, Qt::QueuedConnection);
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

    using namespace vms::common::p2p::downloader;
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
    fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::all;

    switch (downloader()->addFile(fileInformation))
    {
    case ResultCode::ok:
        return update::Status(
            peerId,
            update::Status::Code::downloading,
            "Started downloading update file");
    case ResultCode::fileDoesNotExist:
    case ResultCode::ioError:
    case ResultCode::invalidChecksum:
    case ResultCode::invalidFileSize:
    case ResultCode::invalidChunkIndex:
    case ResultCode::invalidChunkSize:
        m_downloaderFailed = true;
        return update::Status(
            peerId,
            update::Status::Code::error,
            "Downloader experienced internal error");
    case ResultCode::noFreeSpace:
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
    commonModule()->globalSettings()->synchronizeNow();
}

bool CommonUpdateManager::canDownloadFile(
    const QString& fileName,
    update::Status* outUpdateStatus)
{
    using namespace vms::common::p2p::downloader;
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
                "Update file is downloading",
                (double) fileInformation.downloadedChunks.count(true) /
                    fileInformation.downloadedChunks.size());
            return false;
        case FileInformation::Status::uploading:
            *outUpdateStatus = update::Status(
                peerId,
                update::Status::Code::downloading,
                "Update file is being uploaded");
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

bool CommonUpdateManager::findPackage(update::Package* outPackage) const
{
    return update::findPackage(
        QnAppInfo::currentSystemInformation(),
        globalSettings()->updateInformation(),
        runtimeInfoManager()->localInfo().data.peer.isClient(),
        outPackage);
}

bool CommonUpdateManager::statusAppropriateForDownload(
    nx::update::Package* outPackage,
    update::Status* outStatus)
{
    const auto peerId = commonModule()->moduleGUID();
    if (!findPackage(outPackage))
    {
        *outStatus = update::Status(peerId, update::Status::Code::idle, kNotFoundMessage);
        return false;
    }

    if (!canDownloadFile(outPackage->file, outStatus))
        return false;

    return true;
}

void CommonUpdateManager::onDownloaderFailed(const QString& fileName)
{
    nx::update::Package package;
    if (!findPackage(&package) || package.file != fileName)
        return;

    m_downloaderFailed = true;
    downloader()->deleteFile(fileName);
}

void CommonUpdateManager::onDownloaderFinished(const QString& fileName)
{
    nx::update::Package package;
    if (!findPackage(&package) || package.file != fileName)
        return;

    installer()->prepareAsync(downloader()->filePath(fileName));
}

void CommonUpdateManager::install()
{
    if (installer()->state() != CommonUpdateInstaller::State::ok)
        return;

    installer()->install();
}
} // namespace nx
