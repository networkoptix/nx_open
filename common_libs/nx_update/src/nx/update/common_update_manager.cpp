#include "common_update_manager.h"
#include <api/global_settings.h>
#include <nx/fusion/model_functions.h>
#include <nx/update/update_information.h>
#include <common/common_module.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/utils/log/assert.h>
#include <api/runtime_info_manager.h>
#include <nx/update/common_update_installer.h>

namespace nx {

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::UpdateStatus, Code)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(UpdateStatus, (xml)(csv_record)(ubjson)(json), UpdateStatus_Fields)

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

UpdateStatus CommonUpdateManager::start()
{
    const auto peerId = commonModule()->moduleGUID();
    nx::update::Package package;
    UpdateStatus updateStatus;

    bool shouldDownload = statusAppropriateForDownload(&package, &updateStatus);
    if (shouldDownload || !package.isValid())
    {
        m_downloaderFailed = false;
        for (const auto& file : downloader()->files())
        {
            if (file.startsWith("update/"))
                downloader()->deleteFile(file);
        }
        installer()->stopSync();
    }

    if (!shouldDownload)
        return updateStatus;

    using namespace vms::common::p2p::downloader;
    FileInformation fileInformation;
    fileInformation.name = package.file;
    fileInformation.md5 = QByteArray::fromHex(package.md5.toLatin1());
    fileInformation.size = package.size;
    fileInformation.url = package.url;
    fileInformation.peerPolicy = FileInformation::PeerSelectionPolicy::all;

    switch (downloader()->addFile(fileInformation))
    {
    case ResultCode::ok:
        return UpdateStatus(
            peerId,
            UpdateStatus::Code::downloading,
            "Started downloading update file");
    case ResultCode::fileDoesNotExist:
    case ResultCode::ioError:
    case ResultCode::invalidChecksum:
    case ResultCode::invalidFileSize:
    case ResultCode::invalidChunkIndex:
    case ResultCode::invalidChunkSize:
        m_downloaderFailed = true;
        return UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "Downloader experienced internal error");
    case ResultCode::noFreeSpace:
        m_downloaderFailed = true;
        return UpdateStatus(peerId,
            UpdateStatus::Code::error,
            "Downloader experienced internal error");
    default:
        NX_ASSERT(false, "Unexpected Downloader::addFile() result");
        return UpdateStatus(peerId, UpdateStatus::Code::error, "Unknown error");
    }

    return UpdateStatus();
}

UpdateStatus CommonUpdateManager::status()
{
    update::Package package;
    UpdateStatus updateStatus;
    statusAppropriateForDownload(&package, &updateStatus);
    return updateStatus;
}

UpdateStatus CommonUpdateManager::cancel()
{
    return UpdateStatus();
}

void CommonUpdateManager::onGlobalUpdateSettingChanged()
{
    start();
}

bool CommonUpdateManager::findPackage(nx::update::Package* outPackage) const
{
    update::Information updateInformation;
    if (!QJson::deserialize(globalSettings()->updateInformation(), &updateInformation))
        return false;

    for (const auto& package : updateInformation.packages)
    {
        if (commonModule()->runtimeInfoManager()->localInfo().data.peer.isClient() !=
            (package.component == update::Component::client))
        {
            continue;
        }

        // #TODO #akulikov Take package.variant into account as well.
        if (package.arch == QnAppInfo::applicationArch()
            && package.platform == QnAppInfo::applicationPlatform())
        {
            *outPackage = package;
            return true;
        }
    }

    return false;
}

bool CommonUpdateManager::canDownloadFile(
    const QString& fileName,
    UpdateStatus* outUpdateStatus)
{
    using namespace vms::common::p2p::downloader;
    auto fileInformation = downloader()->fileInformation(fileName);
    const auto peerId = commonModule()->moduleGUID();

    if (m_downloaderFailed)
    {
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "Downloader failed to download update file");
        return true;
    }

    if (fileInformation.isValid())
    {
        switch (fileInformation.status)
        {
        case FileInformation::Status::downloading:
            *outUpdateStatus = UpdateStatus(
                peerId,
                UpdateStatus::Code::downloading,
                "Update file is downloading",
                (double) fileInformation.downloadedChunks.count(true) /
                    fileInformation.downloadedChunks.size());
            return false;
        case FileInformation::Status::uploading:
            *outUpdateStatus = UpdateStatus(
                peerId,
                UpdateStatus::Code::downloading,
                "Update file is being uploaded");
            return false;
        case FileInformation::Status::downloaded:
            return installerState(outUpdateStatus, peerId);
        case FileInformation::Status::notFound:
            NX_ASSERT(false, "Unexpected state");
            *outUpdateStatus = UpdateStatus(
                peerId,
                UpdateStatus::Code::error,
                "Unexpected error");
            return true;
        case FileInformation::Status::corrupted:
            *outUpdateStatus = UpdateStatus(
                peerId,
                UpdateStatus::Code::error,
                "Update file is corrupted");
            return true;
        }
    }

    return true;
}

bool CommonUpdateManager::installerState(UpdateStatus* outUpdateStatus, const QnUuid& peerId)
{
    switch (installer()->state())
    {
    case PrepareResult::ok:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::readyToInstall,
            "Update is prepared for installation");
        return false;
    case PrepareResult::inProgress:
    case PrepareResult::idle:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::preparing,
            "Update file is being validated");
        return false;
    case PrepareResult::cleanTemporaryFilesError:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "Failed to clean up temporary files");
        return true;
    case PrepareResult::corruptedArchive:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "Update archive is corrupted");
        return true;
    case PrepareResult::noFreeSpace:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "No enough free space on device");
        return true;
    case PrepareResult::unknownError:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "Internal installer error");
        return true;
    case PrepareResult::updateContentsError:
        *outUpdateStatus = UpdateStatus(
            peerId,
            UpdateStatus::Code::error,
            "Invalid update archive contents");
        return true;
    }

    return true;
}

bool CommonUpdateManager::statusAppropriateForDownload(
    nx::update::Package* outPackage,
    UpdateStatus* outStatus)
{
    const auto peerId = commonModule()->moduleGUID();
    if (!findPackage(outPackage))
    {
        *outStatus = UpdateStatus(peerId, UpdateStatus::Code::idle, kNotFoundMessage);
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

} // namespace nx
