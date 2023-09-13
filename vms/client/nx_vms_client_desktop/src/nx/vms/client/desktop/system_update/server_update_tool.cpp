// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_update_tool.h"

#include <future>

#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QStorageInfo>
#include <QtCore/QThread>

#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <core/resource_management/resource_pool.h>
#include <nx/branding.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/app_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/api/data/system_settings.h>
#include <nx/vms/client/core/network/certificate_verifier.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/settings/system_settings_manager.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/resources_changes_manager.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_update/requests.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>
#include <nx/vms/common/p2p/downloader/private/internet_only_peer_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/update/nx_system_updates_ini.h>
#include <nx/vms/common/update/tools.h>
#include <nx/vms/update/update_check.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/synctime.h>

#include "server_updates_model.h"
#include "update_verification.h"

using namespace std::chrono;

using namespace nx::vms::common::p2p::downloader;

namespace {

// Trying to find a file in root directory or in one of subdirectories.
QDir findFolderForFile(QDir root, QString file)
{
    if (QFileInfo fileInfo(root.filePath(file)); fileInfo.exists())
        return root;

    // Trying to find specified file in subdirectories, but no deeper than 1 level.
    for (const auto& info: root.entryInfoList(QDir::Dirs))
    {
        QDir dir(info.absoluteFilePath());
        if (QFile::exists(dir.filePath(file)))
            return dir;
    }
    return root;
}

static const QString kPackageIndexFile = "packages.json";
static constexpr auto kReadBufferSizeBytes = 1024 * 1024;

} // namespace

namespace nx::vms::client::desktop {

ServerUpdateTool::ServerUpdateTool(SystemContext* systemContext, QObject* parent):
    base_type(parent),
    SystemContextAware(systemContext),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/offline"))
{
    // Expecting paths like "/temp/nx_updates/offline/rand_file".
    QString path = m_outputDir.path();
    NX_VERBOSE(this) << "ServerUpdateTool will output temp files to " << path;

    m_uploadManager.reset(new UploadManager());
    m_stateTracker.reset(new PeerStateTracker(this));

    connect(this, &ServerUpdateTool::moduleInformationReceived,
        m_stateTracker.get(), &PeerStateTracker::setVersionInformation);

    m_downloader.reset(new Downloader(
        m_outputDir, systemContext, {new InternetOnlyPeerManager()}));

    connect(m_downloader.get(), &Downloader::fileStatusChanged,
        this, &ServerUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::fileInformationChanged,
        this, &ServerUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::chunkDownloadFailed,
        this, &ServerUpdateTool::atChunkDownloadFailed);

    connect(m_downloader.get(), &Downloader::downloadFailed,
        this, &ServerUpdateTool::atDownloadFailed);

    connect(m_downloader.get(), &Downloader::downloadFinished,
        this, &ServerUpdateTool::atDownloadFinished);

    connect(m_downloader.get(), &Downloader::downloadStalledChanged,
        this, &ServerUpdateTool::atDownloadStalled);
}

ServerUpdateTool::~ServerUpdateTool()
{
    if (auto api = connectedServerApi(); api && m_settingsRequest > 0)
        api->cancelRequest(m_settingsRequest);

    saveInternalState();
    m_downloader->disconnect(this);
    m_serverConnection.reset();
    NX_VERBOSE(this) << "~ServerUpdateTool() done";
}

void ServerUpdateTool::onConnectToSystem(QnUuid systemId)
{
    m_systemId = systemId;
    if (!branding::customReleaseListUrl().isEmpty())
    {
        auto callback =
            [this](bool /*success*/, rest::Handle requestId)
            {
                NX_ASSERT(m_settingsRequest == requestId || m_settingsRequest == 0);
                m_settingsRequest = 0;
            };

        NX_ASSERT(m_settingsRequest == 0);
        systemContext()->systemSettings()->customReleaseListUrl = branding::customReleaseListUrl();
        m_settingsRequest = systemContext()->systemSettingsManager()->saveSystemSettings(
            callback,
            this);
    }
    loadInternalState();
    m_downloader->startDownloads();
}

void ServerUpdateTool::loadInternalState()
{
    // The following assert causes a lot of problems
    //NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::initial);
    NX_ASSERT(!m_systemId.isNull());

    const SystemUpdateState storedState = systemContext()->localSettings()->updateState();

    m_initiatedUpdate = !storedState.file.isEmpty();
    if (!m_initiatedUpdate)
        return;

    NX_VERBOSE(this, "loadInternalState() we have initiated update to this system.");
    auto state = (OfflineUpdateState) storedState.state;
    switch (state)
    {
        case OfflineUpdateState::ready:
        case OfflineUpdateState::done:
        case OfflineUpdateState::push:
            // We have no idea whether update files are still good on server.
            // The most simple solution - to restart upload process.
            // TODO: Check if we really need more robust state restoration.
            NX_DEBUG(this, "loadInternalState() - restoring offline update from %1",
                storedState.file);
            m_checkFileUpdate = checkUpdateFromFile(storedState.file);
            break;

        default:
            NX_DEBUG(this, "loadInternalState() - got state %1, going to 'initial'", state);
            m_offlineUpdaterState = OfflineUpdateState::initial;
    }
}

void ServerUpdateTool::saveInternalState()
{
    NX_VERBOSE(this, "saveInternalState(%1)", m_offlineUpdaterState);

    if (!m_initiatedUpdate)
    {
        systemContext()->localSettings()->updateState = {};
        return;
    }

    SystemUpdateState state;

    switch (m_offlineUpdaterState)
    {
        case OfflineUpdateState::unpack:
        case OfflineUpdateState::ready:
            // These states occur during the update check (not the update itself) and should not be
            // stored.
            state.state = (int) OfflineUpdateState::initial;
            break;
        default:
            state.state = (int) m_offlineUpdaterState;
            break;
    }

    state.version = m_remoteUpdateManifest.version;

    if (state.state != (int) OfflineUpdateState::initial)
        state.file = m_localUpdateFile;

    systemContext()->localSettings()->updateState = state;
}

bool ServerUpdateTool::hasInitiatedThisUpdate() const
{
    return m_initiatedUpdate;
}

void ServerUpdateTool::onDisconnectFromSystem()
{
    saveInternalState();
}

std::future<UpdateContents> ServerUpdateTool::checkUpdateFromFile(const QString& file)
{
    NX_VERBOSE(this, "checkUpdateFromFile(%1)", file);

    // Clean up existing folder for updates.
    m_outputDir.removeRecursively();
    if (!m_outputDir.exists())
        m_outputDir.mkpath(".");

    m_localUpdateFile = file;
    changeUploadState(OfflineUpdateState::unpack);
    m_extractor = std::make_shared<nx::zip::Extractor>(file, m_outputDir.path());
    m_offlineUpdateCheckResult = std::promise<UpdateContents>();
    m_extractor->start();
    connect(m_extractor.get(), &nx::zip::Extractor::finished, this,
        &ServerUpdateTool::atExtractFilesFinished);

    return m_offlineUpdateCheckResult.get_future();
}

std::future<UpdateContents> ServerUpdateTool::checkMediaserverUpdateInfo()
{
    const auto connection = connectedServerApi();
    if (!connection)
    {
        NX_WARNING(this) << "checkMediaserverUpdateInfo() - have no connection to the server";
        std::promise<UpdateContents> emptyPromise;
        auto result = emptyPromise.get_future();
        emptyPromise.set_value(UpdateContents());
        return result;
    }

    auto promise = std::make_shared<std::promise<UpdateContents>>();
    auto result = promise->get_future();

    const auto handleResponse =
        [this, promise = std::move(promise)](
            bool success, rest::Handle /*handle*/, const nx::network::rest::JsonResult& result)
        {
            UpdateContents contents;

            if (success)
            {
                if (result.error != network::rest::Result::NoError)
                {
                    NX_DEBUG(this,
                        "checkMediaserverUpdateInfo: "
                            "An error in response to the /ec2/updateInformation request: %1",
                        result.errorString);
                }

                contents.info = result.deserialized<common::update::Information>();
                m_timeStartedInstall = contents.info.lastInstallationRequestTime;
                if (contents.info.lastInstallationRequestTime > 0ms)
                    m_serversAreInstalling = nx::utils::toQSet(contents.info.participants);
            }

            contents.sourceType = UpdateSourceType::mediaservers;
            promise->set_value(contents);
        };

    // Requesting remote update info.
    system_update::requestUpdateInformation(connection,
        common::update::TargetVersionParams{},
        nx::utils::guarded(this, handleResponse));
    return result;
}

std::future<UpdateContents> ServerUpdateTool::takeUpdateCheckFromFile()
{
    return std::move(m_checkFileUpdate);
}


void ServerUpdateTool::changeUploadState(OfflineUpdateState newState)
{
    if (m_offlineUpdaterState == newState)
        return;

    m_offlineUpdaterState = newState;
    m_offlineUpdateStateChanged = true;
    saveInternalState();
}

void ServerUpdateTool::readUpdateManifest(
    const QString& path, UpdateContents& result)
{
    result.error = common::update::InformationError::brokenPackageError;
    result.sourceType = UpdateSourceType::file;

    QFile file(path);

    if (!file.open(QFile::ReadOnly))
    {
        NX_ERROR(typeid(ServerUpdateTool),
            "readUpdateManifest(%1) - failed to open file for reading", path);
        return;
    }

    QByteArray buf(kReadBufferSizeBytes, 0);

    qint64 read = file.read(buf.data(), kReadBufferSizeBytes);
    if (read == -1)
    {
        NX_ERROR(typeid(ServerUpdateTool),
            "readUpdateManifest(%1) - failed to read any data from the file", path);
        return;
    }

    buf.truncate(read);
    if (!QJson::deserialize(buf, &result.info))
    {
        NX_ERROR(typeid(ServerUpdateTool),
            "readUpdateManifest(%1) - failed to deserealize update data", path);
        return;
    }

    result.error = common::update::InformationError::noError;
}

// NOTE: We are probably not in the UI thread.
void ServerUpdateTool::atExtractFilesFinished(nx::zip::Extractor::Error code)
{
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::unpack);

    UpdateContents contents;
    contents.sourceType = UpdateSourceType::file;
    contents.source = QString("file://%1").arg(m_localUpdateFile);

    if (code != nx::zip::Extractor::Ok)
    {
        NX_VERBOSE(this, "atExtractFilesFinished() err=%1", code);
        changeUploadState(OfflineUpdateState::initial);
        contents.error = common::update::InformationError::missingPackageError;
        m_offlineUpdateCheckResult.set_value(contents);
        return;
    }

    NX_VERBOSE(this, "atExtractFilesFinished() status = Ready");

    // Find a subfolter containing update manifest
    QDir packageDir = findFolderForFile(m_outputDir, kPackageIndexFile);
    contents.storageDir = packageDir;

    QString manifestPath = packageDir.filePath(kPackageIndexFile);
    readUpdateManifest(manifestPath, contents);

    if (contents.error == common::update::InformationError::noError)
    {
        // TODO: Provide proper installed versions.
        if (verifyUpdateManifest(contents, {}))
        {
            contents.error = common::update::InformationError::noError;
            if (!contents.eulaPath.isEmpty())
            {
                QString eulaPath = contents.storageDir.filePath(contents.eulaPath);
                NX_VERBOSE(this, "atExtractFilesFinished() fixing EULA path from %1 to %2",
                    contents.eulaPath, eulaPath);
                contents.eulaPath = eulaPath;
            }
            changeUploadState(OfflineUpdateState::ready);
        }
        else
        {
            changeUploadState(OfflineUpdateState::initial);
            if (contents.error == common::update::InformationError::noError)
                contents.error = common::update::InformationError::missingPackageError;
        }
    }
    else
    {
        changeUploadState(OfflineUpdateState::initial);
    }
    m_offlineUpdateCheckResult.set_value(contents);
}

QnMediaServerResourceList ServerUpdateTool::getServersForUpload()
{
    QnMediaServerResourceList result = {currentServer()};
#if 0 // Disabled for now, until 4.1 happens.
    auto items = m_stateTracker->allItems();
    for (const auto& record: items)
    {
        auto server = m_stateTracker->getServer(record);
        if (!server)
            continue;
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);

        auto status = server->getStatus();
        bool online = status == nx::vms::api::ResourceStatus::online;

        if (isOurServer && online && record->storeUpdates)
            result.push_back(server);
    }
#endif
    return result;
}

ServerUpdateTool::OfflineUpdateState ServerUpdateTool::getUploaderState() const
{
    return m_offlineUpdaterState;
}

QDir ServerUpdateTool::getDownloadDir() const
{
    return m_outputDir;
}

static qint64 reservedStorageBytesForUpdate()
{
    // NOTE: The same values are specified in the Server setting reservedStorageBytesForUpdate.
    // Reasoning for such constant values: QA team asked to make them so.
    constexpr qint64 kDefaultReservedSpace = 100 * 1024 * 1024;
    constexpr qint64 kWindowsReservedSpace = 500 * 1024 * 1024;
    constexpr qint64 kArmReservedSpace = 5 * 1024 * 1024;

    if (nx::build_info::isWindows())
        return kWindowsReservedSpace;

    if (nx::build_info::isArm())
        return kArmReservedSpace;

    return kDefaultReservedSpace;
}

uint64_t ServerUpdateTool::getAvailableSpace() const
{
    auto downloadDir = getDownloadDir();
    QStorageInfo storageInfo(downloadDir);
    uint64_t bytesAvailable = storageInfo.bytesAvailable();
    uint64_t bytesReserved = reservedStorageBytesForUpdate();
    return bytesReserved > bytesAvailable ? 0 : (bytesAvailable - bytesReserved);
}

void ServerUpdateTool::startManualDownloads(const UpdateContents& contents)
{
    NX_ASSERT(m_downloader);

    m_packageProperties = contents.packageProperties;

    if (contents.sourceType == UpdateSourceType::file)
        return;

    NX_DEBUG(this, "startManualDownloads(): Starting...");

    // TODO: Stop previous manual downloads
    m_manualPackages = contents.manualPackages;

    bool havePersistentServerWithoutInternet = false;
    for (const QnUuid& serverId: systemSettings()->targetPersistentUpdateStorage().servers)
    {
        if (const auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId))
        {
            if (!server->hasInternetAccess())
            {
                havePersistentServerWithoutInternet = true;
                break;
            }
        }
    }

    if (havePersistentServerWithoutInternet)
    {
        NX_DEBUG(this,
            "startManualDownloads(): There are persistent storage servers without internet.");

        for (auto package: contents.info.packages)
        {
            if (package.component == update::Component::client
                || package.component == update::Component::customClient)
            {
                NX_VERBOSE(this, "startManualDownloads(): Adding %1 to manual downloads...",
                    package.file);
                m_manualPackages.insert(package);
            }
        }
    }

    for (auto& package: m_manualPackages)
    {
        FileInformation info;
        info.md5 = QByteArray::fromHex(package.md5.toLatin1());
        info.size = package.size;
        info.name = package.file;
        info.url = package.url;

        NX_ASSERT(info.isValid());
        m_issuedDownloads.insert(package.file);
        m_activeDownloads.insert(std::make_pair(package.file, 0));
        // NOTE: It can emit signals like 'downloadFinished'. So we be sure we have filled in
        // m_activeDownloads and m_issuedDownloads
        auto code = m_downloader->addFile(info);
        QString file =  m_downloader->filePath(package.file);
        m_packageProperties[package.file].localFile = file;
        m_downloader->startDownloads();
        using Code = vms::common::p2p::downloader::ResultCode;

        switch (code)
        {
            case Code::ok:
                NX_VERBOSE(this, "startManualDownloads() - downloading package %1 from url %2",
                    info.name, package.url);
                break;
            case Code::fileAlreadyExists:
            case Code::fileAlreadyDownloaded:
            {
                auto fileInfo = m_downloader->fileInformation(package.file);
                atDownloaderStatusChanged(fileInfo);
                break;
            }
            default:
            // Some sort of an error here.
            {
                NX_VERBOSE(this,
                    "startManualDownloads() - failed start downloading package %1: %2",
                    info.name, code);
                m_activeDownloads.erase(package.file);
                emit atDownloadFailed(package.file);
                break;
            }
        }
    }
}

bool ServerUpdateTool::hasManualDownloads() const
{
    return !m_issuedDownloads.empty();
}

QSet<QnUuid> ServerUpdateTool::getTargetsForPackage(const update::Package& package) const
{
    QSet<QnUuid> result = m_packageProperties[package.file].targets;
    if (package.component == update::Component::client)
    {
        const QList<QnUuid> persistentStorageServers =
            systemSettings()->targetPersistentUpdateStorage().servers;
        result.unite(
            QSet<QnUuid>(persistentStorageServers.begin(), persistentStorageServers.end()));
    }
    return result;
}

int ServerUpdateTool::uploadPackageToRecipients(
    const nx::vms::update::Package& package,
    const QDir& storageDir)
{
    NX_ASSERT(!package.file.isEmpty());

    QString localFile = m_packageProperties[package.file].localFile;
    NX_ASSERT(!localFile.isEmpty());

    auto targets = getTargetsForPackage(package);

    if (targets.empty())
    {
        NX_DEBUG(this, "uploadPackage(%1) - no server wants this package", localFile);
        return 0;
    }

    int toUpload = 0;

    NX_INFO(this, "uploadPackage(%1) - going to upload package to servers", localFile);

    for (const auto& serverId: targets)
    {
        auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
        if (!server)
        {
            NX_ERROR(this, "uploadPackage(%1) - lost server %2 for upload", localFile, serverId);
            continue;
        }

        if (uploadPackageToServer(serverId, package, storageDir))
            toUpload++;
    }

    return toUpload;
}

bool ServerUpdateTool::uploadPackageToServer(const QnUuid& serverId,
    const nx::vms::update::Package& package, QDir storageDir)
{
    QString localFile = m_packageProperties[package.file].localFile;
    UploadState config;
    config.source = storageDir.absoluteFilePath(localFile);
    // Updates should land to updates/version/file_name.
    config.destination = package.file;
    // This should mean 'infinite time'.
    config.ttl = -1;
    // Server should create file by itself.
    config.allowFileCreation = false;

    auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
    {
        NX_ERROR(this, "uploadPackageToServer(%1) - lost server %2 for upload", localFile, serverId);
        return false;
    }

    auto callback =
        [tool = QPointer<ServerUpdateTool>(this), serverId](const UploadState& state)
        {
            if (tool)
                tool->atUploadWorkerState(serverId, state);
        };

    auto id = m_uploadManager->addUpload(server, config, this, callback);

    if (!id.isEmpty())
    {
        NX_INFO(this, "uploadPackageToServer(%1) - started uploading file to server %2",
            package.file, serverId);
        m_uploadStateById[id] = config;
        m_activeUploads.insert(id);
        return true;
    }
    NX_WARNING(this, "uploadPackageToServer(%1) - failed to start uploading file=%2 reason=%3",
        package.file, localFile, config.errorMessage);
    m_completedUploads.insert(id);
    return false;
}

void ServerUpdateTool::atUploadWorkerState(QnUuid serverId, const UploadState& state)
{
    if (!m_uploadStateById.count(state.id))
    {
        NX_VERBOSE(this) << "atUploadWorkerState() no upload state with id=" << state.id;
        return;
    }

    switch (state.status)
    {
        case UploadState::Done:
            NX_VERBOSE(this) << "atUploadWorkerState() uploaded file="
                << state.destination << "server:" << serverId;
            markUploadCompleted(state.id);
            break;
        case UploadState::Uploading:
            //NX_VERBOSE(this) << "atUploadWorkerState() uploading file="
            //    << state.destination << "bytes"
            //    << state.uploaded << "of" << state.size
            //    << "server:" << serverId;
            break;
        case UploadState::Error:
            NX_VERBOSE(this) << "atUploadWorkerState() error with file="
                << state.destination
                << "error:" << state.errorMessage
                << "server:" << serverId;
            markUploadCompleted(state.id);
            break;
        default:
            break;
    }

    m_uploadStateById[state.id] = state;
    m_offlineUpdateStateChanged = true;
}

void ServerUpdateTool::markUploadCompleted(const QString& uploadId)
{
    m_activeUploads.erase(uploadId);
    m_completedUploads.insert(uploadId);

    if (m_activeUploads.empty() && !m_completedUploads.empty())
        changeUploadState(OfflineUpdateState::done);
}

bool ServerUpdateTool::hasActiveUploadsTo(const QnUuid& id) const
{
    return std::any_of(m_uploadStateById.begin(), m_uploadStateById.end(),
        [id](const std::pair<QString, nx::vms::client::desktop::UploadState>& pair)
        {
            return pair.second.uuid == id;
        });
}

bool ServerUpdateTool::startUpload(const UpdateContents& contents, bool cleanExisting, bool force)
{
    NX_VERBOSE(this, "startUpload() clean=%1", cleanExisting);
    QnMediaServerResourceList recipients = getServersForUpload();

    if (recipients.empty())
    {
        NX_WARNING(this, "startUpload(%1) - no recipients for update", contents.info.version);
        return false;
    }

    if (cleanExisting)
    {
        for (const auto& id: m_activeUploads)
            m_uploadManager->cancelUpload(id);

        m_activeUploads.clear();
        m_completedUploads.clear();
        m_uploadStateById.clear();
    }

    int toUpload = 0;
    if (!force && contents.filesToUpload.isEmpty())
    {
        NX_WARNING(this, "startUpload(%1) - nothing to upload", contents.info.version);
    }
    else
    {
        for (const auto& package: contents.info.packages)
            toUpload += uploadPackageToRecipients(package, contents.storageDir);
    }

    if (toUpload > 0)
        NX_VERBOSE(this, "startUpload(%1) - started %2 uploads", contents.info.version, toUpload);

    if (m_activeUploads.empty() && !m_completedUploads.empty())
        changeUploadState(OfflineUpdateState::done);
    else
        changeUploadState(OfflineUpdateState::push);

    return true;
}

void ServerUpdateTool::stopAllUploads()
{
    if (m_offlineUpdaterState != OfflineUpdateState::push)
        return;

    for (const auto& record: m_uploadStateById)
        m_uploadManager->cancelUpload(record.first);

    m_activeUploads.clear();
    m_completedUploads.clear();
    m_uploadStateById.clear();
    NX_VERBOSE(this) << "stopUpload()";
    changeUploadState(OfflineUpdateState::ready);
}

void ServerUpdateTool::startUploadsToServer(const UpdateContents& contents, const QnUuid &peer)
{
    bool started = false;

    for (const auto& package: contents.info.packages)
    {
        if (getTargetsForPackage(package).contains(peer))
        {
            started = uploadPackageToServer(peer, package, contents.storageDir);
            break;
        }
    }
    if (started)
        NX_VERBOSE(this, "startUploadsToServer(%1) - started uploading", peer);
    else
        NX_VERBOSE(this, "startUploadsToServer(%1) - not uploading anything", peer);
}

void ServerUpdateTool::stopUploadsToServer(const QnUuid &peer)
{
    QStringList idsToRemove;
    for (const auto& record: m_uploadStateById)
    {
        if (record.second.uuid == peer)
            idsToRemove.push_back(record.first);
    }

    if (idsToRemove.empty())
        NX_VERBOSE(this, "stopUploadsToServer(%1) no uploads to stop", peer);
    else
        NX_VERBOSE(this, "stopUploadsToServer(%1) stopping %2 uploads", peer, idsToRemove.size());

    for (const auto& id: idsToRemove)
    {
        m_uploadManager->cancelUpload(id);
        m_completedUploads.erase(id);
        m_activeUploads.erase(id);
        m_uploadStateById.erase(id);
    }
}

bool ServerUpdateTool::verifyUpdateManifest(
    UpdateContents& contents,
    const std::set<nx::utils::SoftwareVersion>& clientVersions,
    bool checkClient) const
{
    NX_ASSERT(m_stateTracker);
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers = m_stateTracker->activeServers();

    ClientVerificationData clientData;
    clientData.fillDefault();
    clientData.clientId = checkClient
        ? m_stateTracker->getClientPeerId(systemContext())
        : QnUuid();
    clientData.installedVersions = clientVersions;
    VerificationOptions options;
    options.systemContext = systemContext();
    return verifyUpdateContents(contents, activeServers, clientData, options);
}

void ServerUpdateTool::calculateManualDownloadProgress(ProgressInfo& progress)
{
    if (m_issuedDownloads.empty())
        return;

    progress.downloadingForServers = !m_activeDownloads.empty();

    for (const auto& record: m_activeDownloads)
    {
        progress.current += record.second;
        ++progress.active;
    }

    progress.current += 100 * m_completeDownloads.size();
    progress.done += m_completeDownloads.size();
    progress.max += 100 * m_issuedDownloads.size();
}

bool ServerUpdateTool::hasRemoteChanges() const
{
    bool result = false;
    if (!m_remoteUpdateStatus.empty())
        result = true;
    // ...
    return result;
}

bool ServerUpdateTool::hasOfflineUpdateChanges() const
{
    return m_offlineUpdateStateChanged;
}

bool ServerUpdateTool::getServersStatusChanges(RemoteStatus& status)
{
    if (m_remoteUpdateStatus.empty())
        return false;
    status = std::move(m_remoteUpdateStatus);
    return true;
}

bool ServerUpdateTool::haveActiveUpdate() const
{
    return m_remoteUpdateManifest.isValid();
}

ServerUpdateTool::TimePoint::duration ServerUpdateTool::getInstallDuration() const
{
    return qnSyncTime->value() - m_timeStartedInstall;
}

void ServerUpdateTool::dropAllRequests(const QString& reason)
{
    if (!m_activeRequests.empty())
    {
        m_skippedRequests.unite(m_activeRequests);
        m_expectedStatusHandle = 0;
        NX_VERBOSE(this, "%1 will skip requests %2", reason, m_skippedRequests);
    }
}

bool ServerUpdateTool::requestStopAction()
{
    if (m_requestingStop)
    {
        NX_WARNING(this, "requestStopAction() - previous request is not complete");
        return false;
    }

    const auto connection = connectedServerApi();
    if (!connection)
    {
        NX_ERROR(this, "requestStopAction() - no conenction to the server.");
        emit cancelUpdateComplete(false, toString(InternalError::noConnection));
        return false;
    }

    NX_INFO(this, "requestStopAction() - sending request");
    dropAllRequests("requestStopAction()");

    const auto handleResponse =
        [this](
            bool success,
            rest::Handle /*requestId*/,
            const rest::ServerConnection::EmptyResponseType& /*response*/)
        {
            NX_VERBOSE(this, "requestStopAction() - success=%1", success);

            if (success)
            {
                m_remoteUpdateManifest = common::update::Information();
                if (m_offlineUpdaterState == OfflineUpdateState::push
                    || m_offlineUpdaterState == OfflineUpdateState::done)
                {
                    changeUploadState(OfflineUpdateState::ready);
                }
            }

            m_requestingStop = false;
            const auto error = success ? InternalError::noError : InternalError::networkError;

            emit cancelUpdateComplete(success, toString(error));
        };

    connection->postEmptyResult("/ec2/cancelUpdate",
        network::rest::Params{},
        QByteArray{},
        nx::utils::guarded(this, handleResponse),
        thread());

    for (const auto& file: m_activeDownloads)
        m_downloader->deleteFile(file.first);

    m_activeDownloads.clear();
    m_stateTracker->clearState();
    return true;
}

bool ServerUpdateTool::requestRetryAction()
{
    const auto connection = connectedServerApi();
    if (!connection)
        return false;

    const auto handleResponse =
        [this](
            bool success,
            rest::Handle handle,
            const network::rest::JsonResult result)
        {
            if (result.error != network::rest::Result::NoError)
            {
                NX_VERBOSE(this,
                    "requestRetryAction: Error in response /ec2/retryUpdate: code=%1, error=%2",
                    result.error, result.errorString);
                return;
            }

            atUpdateStatusResponse(
                success, handle, result.deserialized<common::update::StatusList>());
        };

    NX_INFO(this, "requestRetryAction() - sending request");
    m_expectedStatusHandle = connection->postJsonResult("/ec2/retryUpdate",
        network::rest::Params{},
        QByteArray{},
        nx::utils::guarded(this, handleResponse),
        thread());

    return m_expectedStatusHandle != 0;
}

bool ServerUpdateTool::requestFinishUpdate(bool skipActivePeers)
{
    if (m_requestingFinish)
    {
        NX_WARNING(this, "requestFinishUpdate(%1) - previous request is not complete",
            skipActivePeers);
        return false;
    }

    m_requestingFinish = true;
    const auto connection = connectedServerApi();
    if (!connection)
    {
        NX_ERROR(this, "requestFinishUpdate(%1) - no connection to the server", skipActivePeers);
        emit finishUpdateComplete(false, toString(InternalError::noConnection));
        return false;
    }

    NX_VERBOSE(this, "requestFinishUpdate(%1) - sending request", skipActivePeers);

    const auto handleResponse =
        [this, skipActivePeers](
            bool success,
            rest::Handle /*requestId*/,
            const network::rest::JsonResult& result)
        {
            NX_VERBOSE(this, "requestFinishUpdate(%1) - got response, success=%2",
                skipActivePeers, success);
            m_requestingFinish = false;
            auto error = InternalError::noError;
            if (!success)
            {
                error = InternalError::networkError;
                NX_ERROR(this, "requestFinishUpdate(%1) - failed to send a request",
                    skipActivePeers);
            }
            else if (result.error != network::rest::Result::NoError)
            {
                success = false;
                error = InternalError::serverError;
                NX_ERROR(this, "requestFinishUpdate(%1) - server responded with %2",
                    skipActivePeers, result.errorString);
            }

            m_localUpdateFile = QString();
            emit finishUpdateComplete(success, toString(error));
        };

    network::rest::Params params;
    if (skipActivePeers)
        params.insert("ignorePendingPeers", "true");

    connection->postJsonResult("/ec2/finishUpdate",
        params,
        QByteArray(),
        nx::utils::guarded(this, handleResponse),
        thread());

    return true;
}

bool ServerUpdateTool::requestStartUpdate(
    const nx::vms::common::update::Information& info,
    const QSet<QnUuid>& targets)
{
    NX_VERBOSE(this, "requestStartUpdate(%1) - sending /ec2/startUpdate command", info.version);

    QSet<QnUuid> servers;
    for (const auto& id: targets)
    {
        auto item = m_stateTracker->findItemById(id);
        if (item->component == UpdateItem::Component::server)
            servers.insert(id);
    }

    if (servers.empty())
    {
        NX_WARNING(this, "requestStartUpdate(%1) - target list contains no server to update",
            info.version);
    }

    const auto connection = connectedServerApi();
    if (!connection)
    {
        m_remoteUpdateStatus = {};
        NX_ERROR(this, "requestStartUpdate(%1) - no connection to the server", info.version);
        emit startUpdateComplete(false, toString(InternalError::noConnection));
        return false;
    }

    dropAllRequests("requestStartUpdate()");

    const auto handleResponse =
        [this](
            bool success,
            rest::Handle /*requestId*/,
            const rest::ServerConnection::EmptyResponseType& /*response*/)
        {
            const auto error = success ? InternalError::noError : InternalError::networkError;
            if (success)
            {
                m_initiatedUpdate = true;
                saveInternalState();
            }
            emit startUpdateComplete(success, toString(error));
        };

    connection->postEmptyResult("/ec2/startUpdate",
        network::rest::Params{},
        QJson::serialized(info),
        nx::utils::guarded(this, handleResponse));

    m_remoteUpdateManifest = info;

    return true;
}

bool ServerUpdateTool::requestInstallAction(
    const QSet<QnUuid>& targets)
{
    // Filtering out only 'server' components.
    QSet<QnUuid> servers;
    for (const auto& id: targets)
    {
        auto item = m_stateTracker->findItemById(id);
        if (item->component == UpdateItem::Component::server)
            servers.insert(id);
    }

    if (servers.empty())
    {
        NX_VERBOSE(this, "requestInstallAction() it is client-only update");
        // Seems like it is client-only update. There is no need to send
        // POST ec2/installUpdate request.
        emit startInstallComplete(true, toString(InternalError::noError));
        return true;
    }

    m_remoteUpdateStatus = {};

    m_timeStartedInstall = qnSyncTime->value();
    m_serversAreInstalling = servers;

    const auto connection = connectedServerApi();
    if (!connection)
    {
        NX_ERROR(this, "requestInstallAction(%1) - no connection to the server", targets);
        emit startInstallComplete(false, toString(InternalError::noConnection));
        return false;
    }

    NX_VERBOSE(this, "requestInstallAction() for %1", servers);
    dropAllRequests("requestInstallAction()");

    const auto handleResponse =
        [this](
            bool success,
            rest::Handle requestId,
            const nx::network::rest::JsonResult& result)
        {
            m_requestingInstall.remove(requestId);
            requestRemoteUpdateStateAsync();

            auto error = InternalError::noError;
            if (!success)
            {
                error = InternalError::networkError;
                m_serversAreInstalling = {};
            }
            else if (result.error != network::rest::Result::NoError)
            {
                success = false;
                error = InternalError::serverError;
                NX_ERROR(this, "requestInstallAction() - server responded with \"%1\"",
                    result.errorString);
            }

            emit startInstallComplete(success, toString(error));
        };

    const rest::Handle handle = connection->postJsonResult("/api/installUpdate",
        {{"peers", containerString(servers.begin(), servers.end(),
            /*delimiter*/ ",",
            /*prefix*/ "",
            /*suffix*/ "",
            /*empty*/ "")}},
        QByteArray(),
        nx::utils::guarded(this, handleResponse));

    if (handle)
        m_requestingInstall.insert(handle);

    return true;
}

void ServerUpdateTool::requestModuleInformation()
{
    //NX_VERBOSE(this, "requestModuleInformation()");
    auto callback =
        [tool = QPointer(this), connection = m_serverConnection](
            bool success, rest::Handle /*handle*/,
            const rest::RestResultWithData<QList<nx::vms::api::ModuleInformation>>& response)
        {
            if (success && tool)
                emit tool->moduleInformationReceived(response.data);
        };

    // We expect that m_serverConnection is created in requestStartUpdate
    if (!m_serverConnection)
    {
        if (auto ec2connection = systemContext()->messageBusConnection())
        {
            const nx::vms::api::ModuleInformation& moduleInformation =
                ec2connection->moduleInformation();

            // TODO: #sivanov Looks like we can take existing rest connection here.
            m_serverConnection.reset(new rest::ServerConnection(
                moduleInformation.id,
                /*auditId*/ systemContext()->sessionId(),
                systemContext()->certificateVerifier(),
                ec2connection->address(),
                ec2connection->credentials()));
        }
        else
        {
            NX_WARNING(this, "requestModuleInformation() - ec2Connection is not available. "
                "I will have problems tracking server version during install process.");
        }
    }
    if (m_serverConnection)
        m_serverConnection->getModuleInformationAll(callback);
}

void ServerUpdateTool::atUpdateStatusResponse(
    bool success,
    rest::Handle handle,
    const std::vector<nx::vms::common::update::Status>& response)
{
    NX_VERBOSE(this, "Got update status response. Success: %1, handle: %2", success, handle);

    if (m_expectedStatusHandle && m_expectedStatusHandle != handle)
    {
        NX_VERBOSE(this, "Update status response %1 was skipped.", handle);
        return;
    }

    m_expectedStatusHandle = 0;

    if (!success)
        return;

    RemoteStatus remoteStatus;
    for (const auto& status: response)
        remoteStatus[status.serverId] = status;

    m_remoteUpdateStatus = std::move(remoteStatus);
}

void ServerUpdateTool::requestRemoteUpdateStateAsync()
{
    NX_VERBOSE(this, "Requesting update status...");

    // Request another state only if there is no pending request.
    if (m_expectedStatusHandle > 0)
    {
        NX_VERBOSE(this, "One of update status requests is in progress. Skipping...");
        return;
    }

    const auto connection = connectedServerApi();
    if (!connection)
        return;

    // We need to track both /ec2/updateStatus and /ec2/updateInformation to get relevant
    // information. /ec2/updateInformation is necessary to keep track of the servers
    // participating in the update.

    // Requesting update status for mediaservers.
    m_expectedStatusHandle = system_update::requestUpdateStatus(
        connection,
        [this, tool = QPointer<ServerUpdateTool>(this)](
            bool success,
            rest::Handle handle,
            const network::rest::JsonResult& result)
        {
            if (!tool)
                return;

            if (result.error != network::rest::Result::NoError)
            {
                NX_VERBOSE(this,
                    "requestRemoteUpdateStateAsync: "
                        "Error in response to /ec2/updateStatus: code=%1, error=%2",
                    result.error, result.errorString);
            }

            tool->atUpdateStatusResponse(
                success, handle, result.deserialized<common::update::StatusList>());
        },
        thread());

    // Requesting remote update info.
    const rest::Handle handle = system_update::requestUpdateInformation(
        connection, common::update::TargetVersionParams{},
        [tool = QPointer<ServerUpdateTool>(this)](
            bool success, rest::Handle handle, const nx::network::rest::JsonResult& result)
        {
            if (!tool)
                return;

            tool->m_activeRequests.remove(handle);

            if (!success || result.error != network::rest::Result::NoError)
                return;

            if (tool->m_skippedRequests.contains(handle))
                return;

            tool->m_remoteUpdateManifest = result.deserialized<common::update::Information>();
            if (tool->m_remoteUpdateManifest.lastInstallationRequestTime > 0ms)
            {
                tool->m_serversAreInstalling =
                    nx::utils::toQSet(tool->m_remoteUpdateManifest.participants);
            }
            else
            {
                tool->m_serversAreInstalling = {};
            }
        },
        thread());
    m_activeRequests.insert(handle);
}

std::future<ServerUpdateTool::RemoteStatus> ServerUpdateTool::requestRemoteUpdateState()
{
    auto promise = std::make_shared<std::promise<RemoteStatus>>();
    if (auto connection = connectedServerApi())
    {
        system_update::requestUpdateStatus(connection,
            [this, promise, tool = QPointer<ServerUpdateTool>(this)](
                 bool success,
                 rest::Handle handle,
                 const network::rest::JsonResult& result)
            {
                if (result.error != network::rest::Result::NoError)
                {
                    NX_DEBUG(this,
                        "requestRemoteUpdateState: Error in response to /ec2/updateStatus: %1",
                        result.errorString);
                }

                const auto statusList = result.deserialized<common::update::StatusList>();

                if (tool)
                    tool->atUpdateStatusResponse(success, handle, statusList);

                RemoteStatus remoteStatus;
                for (const auto& status: statusList)
                    remoteStatus[status.serverId] = status;

                promise->set_value(remoteStatus);
            }, thread());
    }
    else
    {
        promise->set_value({});
    }

    return promise->get_future();
}

std::shared_ptr<PeerStateTracker> ServerUpdateTool::getStateTracker()
{
    return m_stateTracker;
}

QSet<QnUuid> ServerUpdateTool::getServersInstalling() const
{
    return m_serversAreInstalling;
}

QString ServerUpdateTool::toString(InternalError errorCode)
{
    switch(errorCode)
    {
        case InternalError::noError:
            return "";
        case InternalError::noConnection:
            return tr("No connection to the server.");
        case InternalError::networkError:
            return tr("Network error.");
        case InternalError::serverError:
            return tr("Server error.");
    }
    return "Unknown error.";
}

const nx::vms::update::Package* ServerUpdateTool::findPackageForFile(const QString& fileName) const
{
    for (const auto& package: m_manualPackages)
    {
        if (package.file == fileName)
            return &package;
    }
    return nullptr;
}

void ServerUpdateTool::atDownloaderStatusChanged(const FileInformation& fileInformation)
{
    auto it = m_activeDownloads.find(fileInformation.name);
    if (it == m_activeDownloads.end())
    {
        NX_ERROR(this) << "Did not found a file"<<fileInformation.name << "in m_activeDownloads";
        return;
    }

    auto package = findPackageForFile(fileInformation.name);
    if (!package)
    {
        NX_ERROR(this)
            << "Did not found a package for a file"
            << fileInformation.name << ". Why were we downloading it?";
        return;
    }

    switch (fileInformation.status)
    {
        case FileInformation::Status::notFound:
            NX_VERBOSE(this) << "atDownloaderStatusChanged("<< fileInformation.name
                << ") - status changed to " << fileInformation.status;
            emit packageDownloadFailed(*package, "Update file is not found");
            m_activeDownloads.erase(it);
            break;
        case FileInformation::Status::uploading:
            break;
        case FileInformation::Status::downloaded:
            atDownloadFinished(fileInformation.name);
            break;
        case FileInformation::Status::corrupted:
            NX_VERBOSE(this) << "atDownloaderStatusChanged("<< fileInformation.name
                << ") - status changed to " << fileInformation.status;
            m_activeDownloads.erase(it);
            emit packageDownloadFailed(*package, "Update file is corrupted");
            break;
        case FileInformation::Status::downloading:
            it->second = fileInformation.downloadProgress();
            break;
        default:
            // Nothing to do here
            break;
    }
}

void ServerUpdateTool::atChunkDownloadFailed(const QString& /*fileName*/)
{
    // It is a breakpoint catcher.
    //NX_VERBOSE(this) << "atChunkDownloadFailed() failed to download chunk for" << fileName;
}

void ServerUpdateTool::atDownloadFailed(const QString& fileName)
{
    auto package = findPackageForFile(fileName);
    if (package)
    {
        NX_ERROR(this, "atDownloadFailed(%1) - failed to download update package.", fileName);
        emit packageDownloadFailed(*package, "Update file is corrupted");
    }
    else
    {
        NX_ERROR(this, "atDownloadFailed(%1) - did not found a package for a file. "
            "Why were we downloading this file?", fileName);
    }
}

void ServerUpdateTool::atDownloadFinished(const QString& fileName)
{
    auto it = m_activeDownloads.find(fileName);
    if (it == m_activeDownloads.end())
    {
        NX_ERROR(this, "atDownloadFinished(%1) - Did not found a file in m_activeDownloads",
            fileName);
        return;
    }

    for (const update::Package& package: m_manualPackages)
    {
        if (package.file == fileName)
        {
            m_completeDownloads.insert(it->first);
            m_activeDownloads.erase(it);
            m_packageProperties[package.file].localFile = m_downloader->filePath(fileName);

            emit packageDownloaded(package);
            return;
        }
    }
}

void ServerUpdateTool::atDownloadStalled(const QString& fileName, bool stalled)
{
    NX_ERROR(this, "atDownloadStalled(%1) - stalled=%2", fileName, stalled);
}

nx::utils::Url getServerUrl(nx::network::SocketAddress address, QString path)
{
    if (address == nx::network::SocketAddress())
        return {};

    return nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(address)
        .setPath(path)
        .toUrl();
}

QString ServerUpdateTool::getUpdateStateUrl() const
{
    if (auto connection = this->connection(); NX_ASSERT(connection))
    {
        const auto url = getServerUrl(connection->address(), "/ec2/updateStatus");
        return url.toString(QUrl::RemoveUserInfo);
    }
    return {};
}

QString ServerUpdateTool::getUpdateInformationUrl() const
{
    if (auto connection = this->connection(); NX_ASSERT(connection))
    {
        const auto url = getServerUrl(connection->address(), "/ec2/updateInformation");
        return url.toString(QUrl::RemoveUserInfo);
    }
    return {};
}

QString ServerUpdateTool::getInstalledUpdateInfomationUrl() const
{
    if (auto connection = this->connection(); NX_ASSERT(connection))
    {
        auto url = getServerUrl(connection->address(), "/ec2/updateInfomation");
        QUrlQuery query;
        query.addQueryItem("version", "latest");
        url.setQuery(query);
        return url.toString(QUrl::RemoveUserInfo);
    }
    return {};
}

std::future<UpdateContents> ServerUpdateTool::checkForUpdate(
    const common::update::UpdateInfoParams& infoParams)
{
    const auto connection = this->connection();

    if (!connection || !connection->serverApi())
    {
        NX_WARNING(this, "checkForUpdate() - There's no server connection.");
        std::promise<UpdateContents> promise;
        auto future = promise.get_future();
        promise.set_value({});
        return future;
    }

    return std::async(
        [updateUrl = nx::vms::common::update::releaseListUrl(systemContext()),
            connection,
            infoParams]() -> UpdateContents
        {
            auto contents = system_update::getUpdateContents(
                connection->serverApi(), updateUrl, infoParams);
            if (std::holds_alternative<update::LatestVmsVersionParams>(infoParams))
                contents.sourceType = UpdateSourceType::internet;
            else
                contents.sourceType = UpdateSourceType::internetSpecific;
            return contents;
        });
}

} // namespace nx::vms::client::desktop
