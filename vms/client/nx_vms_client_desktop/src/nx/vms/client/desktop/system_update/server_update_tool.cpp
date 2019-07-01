#include <future>

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/incompatible_server_watcher.h>

#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <utils/update/update_utils.h>

#include <client/client_settings.h>
#include <client/client_module.h>
#include <watchers/cloud_status_watcher.h>

#include <core/resource/fake_media_server.h>
#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <network/system_helpers.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/utils/app_info.h>
#include <nx/utils/guarded_callback.h>
#include <nx/update/update_check.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include "server_update_tool.h"
#include "server_updates_model.h"

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

QJsonArray componentsListToJson(
    const nx::utils::OsInfo& clientOsInfo, const QSet<nx::utils::OsInfo>& serverInfoSet)
{
    const auto makeItem =
        [](const QString& component, const nx::utils::OsInfo& info)
        {
            QJsonObject obj = info.toJson();
            obj[QStringLiteral("component")] = component;
            return obj;
        };

    QJsonArray result;
    result.append(makeItem(QStringLiteral("client"), clientOsInfo));
    for (const auto& info: serverInfoSet)
        result.append(makeItem(QStringLiteral("server"), info));

    return result;
}

QString compactPackagesQuery(const QJsonArray& packagesQuery)
{
    const QByteArray& json = QJsonDocument(packagesQuery).toJson(QJsonDocument::Compact);
    QByteArray compressed = qCompress(json);
    compressed.remove(0, 4); //< We don't need 4 bytes which are added by Qt in the beginning.
    return QString::fromLatin1(compressed.toBase64());
}

const QString kPackageIndexFile = "packages.json";
const QString kLatestChangeset = "latest";
const int kReadBufferSizeBytes = 1024 * 1024;

} // namespace

namespace nx::vms::client::desktop {

ServerUpdateTool::ServerUpdateTool(QObject* parent):
    base_type(parent),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/offline"))
{
    // Expecting paths like "/temp/nx_updates/offline/rand_file".
    QString path = m_outputDir.path();
    NX_VERBOSE(this) << "ServerUpdateTool will output temp files to " << path;

    m_uploadManager.reset(new UploadManager());
    m_stateTracker.reset(new PeerStateTracker(this));

    connect(this, &ServerUpdateTool::moduleInformationReceived,
        m_stateTracker.get(), &PeerStateTracker::setVersionInformation);
    m_updatesModel.reset(new ServerUpdatesModel(m_stateTracker, this));

    m_downloader.reset(new Downloader(m_outputDir, commonModule()));

    // This object is managed by shared_ptr. So we must sure there is no parent, or this instance
    // can be deleted twice.
    setParent(nullptr);

    connect(m_downloader.get(), &Downloader::fileStatusChanged,
        this, &ServerUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::fileInformationChanged,
        this, &ServerUpdateTool::atDownloaderStatusChanged);

    connect(m_downloader.get(), &Downloader::chunkDownloadFailed,
        this, &ServerUpdateTool::atChunkDownloadFailed);

    connect(m_downloader.get(), &Downloader::downloadFailed,
        this, &ServerUpdateTool::atDownloadFailed);
}

ServerUpdateTool::~ServerUpdateTool()
{
    saveInternalState();
    m_downloader->disconnect(this);
    m_serverConnection.reset();
    NX_VERBOSE(this) << "~ServerUpdateTool() done";
}

void ServerUpdateTool::resumeTasks()
{
    m_downloader->startDownloads();
    loadInternalState();
}

// We serialize state of the uploader using this struct.
struct StoredState
{
    QString file;
    int state;
};

#define StoredState_Fields (file)(state)
QN_FUSION_DECLARE_FUNCTIONS(StoredState, (json))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StoredState, (json), StoredState_Fields)

void ServerUpdateTool::loadInternalState()
{
    if (m_offlineUpdaterState != OfflineUpdateState::initial)
        return;

    QString raw = qnSettings->systemUpdaterState();
    StoredState stored;
    if (QJson::deserialize(raw, &stored))
    {
        auto state = (OfflineUpdateState)stored.state;
        switch(state)
        {
            case OfflineUpdateState::done:
            case OfflineUpdateState::push:
                // We have no idea whether update files are still good on server.
                // The most simple solution - to restart upload process.
                // TODO: Check if we really need more robust state restoration.
                NX_DEBUG(this, "loadInternalState() - restoring offline update from %1", stored.file);
                m_checkFileUpdate = checkUpdateFromFile(stored.file);
                break;
            default:
                NX_DEBUG(this, "loadInternalState() - got state %1, going to 'initial'", state);
                m_offlineUpdaterState = OfflineUpdateState::initial;
        }
    }
    else
    {
        NX_DEBUG(this, "loadInternalState() - no uploader state to restore");
    }
}

void ServerUpdateTool::saveInternalState()
{
    NX_VERBOSE(this, "saveInternalState(%1)", m_offlineUpdaterState);
    StoredState stored;
    stored.file = m_localUpdateFile;
    stored.state = (int)m_offlineUpdaterState;

    QByteArray raw;
    QJson::serialize(stored, &raw);
    qnSettings->setSystemUpdaterState(raw);
}

std::future<nx::update::UpdateContents> ServerUpdateTool::checkUpdateFromFile(const QString& file)
{
    NX_VERBOSE(this, "checkUpdateFromFile(%1)", file);

    // Clean up existing folder for updates.
    m_outputDir.removeRecursively();
    if (!m_outputDir.exists())
        m_outputDir.mkpath(".");

    m_localUpdateFile = file;
    changeUploadState(OfflineUpdateState::unpack);
    m_extractor = std::make_shared<QnZipExtractor>(file, m_outputDir.path());
    m_offlineUpdateCheckResult = std::promise<UpdateContents>();
    m_extractor->start();
    connect(m_extractor.get(), &QnZipExtractor::finished, this, &ServerUpdateTool::atExtractFilesFinished);

    return m_offlineUpdateCheckResult.get_future();
}

std::future<nx::update::UpdateContents> ServerUpdateTool::checkMediaserverUpdateInfo()
{
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        auto promise = std::make_shared<std::promise<UpdateContents>>();
        auto result = promise->get_future();
        // Requesting remote update info.
        connection->getUpdateInfo(
            [this, promise = std::move(promise), tool=QPointer<ServerUpdateTool>(this)](bool success,
                rest::Handle /*handle*/, rest::UpdateInformationData response)
            {
                UpdateContents contents;
                if (success)
                {
                    if (response.error != QnRestResult::NoError)
                    {
                        NX_DEBUG(
                            this,
                            lm("checkMediaserverUpdateInfo: An error in response to the /ec2/updateInformation request: %1")
                                .args(response.errorString));
                    }
                    else
                    {
                        contents.info = response.data;
                        if (tool)
                        {
                            tool->m_timeStartedInstall = contents.info.lastInstallationRequestTime;
                            tool->m_serversAreInstalling = contents.info.participants.toSet();
                        }
                    }
                }
                contents.sourceType = nx::update::UpdateSourceType::mediaservers;
                promise->set_value(contents);
            });
        return result;
    }
    else
    {
        NX_WARNING(this) << "checkMediaserverUpdateInfo() - have no connection to the server";
        std::promise<UpdateContents> emptyPromise;
        auto result = emptyPromise.get_future();
        emptyPromise.set_value(UpdateContents());
        return result;
    }
}

std::future<nx::update::UpdateContents> ServerUpdateTool::takeUpdateCheckFromFile()
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
    result.error = nx::update::InformationError::brokenPackageError;
    result.sourceType = nx::update::UpdateSourceType::file;

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

    result.error = nx::update::InformationError::noError;
}

// NOTE: We are probably not in the UI thread.
void ServerUpdateTool::atExtractFilesFinished(int code)
{
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::unpack);

    UpdateContents contents;
    contents.sourceType = nx::update::UpdateSourceType::file;
    contents.source = QString("file://%1").arg(m_localUpdateFile);

    if (code != QnZipExtractor::Ok)
    {
        NX_VERBOSE(this, "atExtractFilesFinished() err=%1",
            QnZipExtractor::errorToString((QnZipExtractor::Error)code));
        changeUploadState(OfflineUpdateState::initial);
        contents.error = nx::update::InformationError::missingPackageError;
        m_offlineUpdateCheckResult.set_value(contents);
        return;
    }

    NX_VERBOSE(this, "atExtractFilesFinished() status = Ready");

    // Find a subfolter containing update manifest
    QDir packageDir = findFolderForFile(m_outputDir, kPackageIndexFile);
    contents.storageDir = packageDir;

    QString manifestPath = packageDir.filePath(kPackageIndexFile);
    readUpdateManifest(manifestPath, contents);

    if (contents.error == nx::update::InformationError::noError)
    {
        // TODO: Provide proper installed versions.
        if (verifyUpdateManifest(contents, {}) && !contents.filesToUpload.empty())
        {
            contents.error = nx::update::InformationError::noError;
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
            if (contents.error == nx::update::InformationError::noError)
                contents.error = nx::update::InformationError::missingPackageError;
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
    QnMediaServerResourceList result = {commonModule()->currentServer()};
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
        bool online = status == Qn::ResourceStatus::Online;

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

void ServerUpdateTool::startManualDownloads(const UpdateContents& contents)
{
    NX_ASSERT(m_downloader);

    // TODO: Stop previous manual downloads
    m_manualPackages = contents.manualPackages;

    for (auto& package: m_manualPackages)
    {
        FileInformation info;
        info.md5 = QByteArray::fromHex(package.md5.toLatin1());
        info.size = package.size;
        info.name = package.file;
        info.url = package.url;

        NX_ASSERT(info.isValid());
        auto code = m_downloader->addFile(info);
        m_downloader->startDownloads();
        using Code = vms::common::p2p::downloader::ResultCode;
        QString file =  m_downloader->filePath(package.file);
        m_issuedDownloads.insert(package.file);
        package.localFile = file;

        switch (code)
        {
            case Code::ok:
                NX_VERBOSE(this) << "startManualDownloads() - downloading package"
                    << info.name << " from url=" << package.url;
                m_activeDownloads.insert(std::make_pair(package.file, 0));
                break;
            case Code::fileAlreadyExists:
            case Code::fileAlreadyDownloaded:
            {
                auto fileInfo = m_downloader->fileInformation(package.file);
                m_activeDownloads.insert(std::make_pair(package.file, 0));
                atDownloaderStatusChanged(fileInfo);
                break;
            }
            default:
            // Some sort of an error here.
            {
                QString error = vms::common::p2p::downloader::toString(code);
                NX_VERBOSE(this) << "requestStartUpdate() - failed start downloading package"
                    << info.name << error;
                m_failedDownloads.insert(file);
                break;
            }
        }
    }
}

bool ServerUpdateTool::hasManualDownloads() const
{
    return !m_issuedDownloads.empty();
}

QSet<QnUuid> ServerUpdateTool::getTargetsForPackage(const nx::update::Package& package) const
{
    // TODO: We should provide additional server to store all update packages
    return package.targets;
}

int ServerUpdateTool::uploadPackage(
    const nx::update::Package& package,
    const QDir& storageDir)
{
    NX_ASSERT(package.isValid());

    QString localFile = package.localFile;
    NX_ASSERT(!localFile.isEmpty());

    auto targets = getTargetsForPackage(package);

    if (targets.empty())
    {
        NX_WARNING(this, "uploadPackage(%1) - no server wants this package", localFile);
        return 0;
    }

    int toUpload = 0;

    NX_INFO(this, "uploadPackage(%1) - going to upload package to servers", localFile);

    UploadState config;
    config.source = storageDir.absoluteFilePath(localFile);
    config.destination = package.file;
    // Updates should land to updates/publication_key/file_name.
    config.ttl = -1; //< This should mean 'infinite time'

    for (const auto& serverId: targets)
    {
        auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverId);
        if (!server)
        {
            NX_ERROR(this, "uploadPackage(%1) - lost server %2 for upload", localFile, serverId);
            continue;
        }

        auto callback = [tool = QPointer<ServerUpdateTool>(this), serverId](const UploadState& state)
        {
            if (tool)
                tool->atUploadWorkerState(serverId, state);
        };

        auto id = m_uploadManager->addUpload(server, config, this, callback);

        if (!id.isEmpty())
        {
            m_uploadStateById[id] = config;
            m_activeUploads.insert(id);
            toUpload++;
        }
        else
        {
            NX_VERBOSE(this, "uploadPackage(%1) - failed to start uploading file=%2 reason=%3",
                package.file, localFile, config.errorMessage);
            m_completedUploads.insert(id);
        }
    }

    return toUpload;
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
            NX_VERBOSE(this) << "atUploadWorkerState() uploading file="
                << state.destination << "bytes"
                << state.uploaded << "of" << state.size
                << "server:" << serverId;
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
    {
        changeUploadState(OfflineUpdateState::done);
    }
}

bool ServerUpdateTool::startUpload(const UpdateContents& contents)
{
    NX_VERBOSE(this) << "startUpload()";
    QnMediaServerResourceList recipients = getServersForUpload();

    if (recipients.empty())
    {
        NX_WARNING(this, "startUpload(%1) - no recipients for update", contents.info.version);
        return false;
    }

    for (const auto& id: m_activeUploads)
        m_uploadManager->cancelUpload(id);

    m_activeUploads.clear();
    m_completedUploads.clear();
    m_uploadStateById.clear();

    int toUpload = 0;
    if (contents.filesToUpload.isEmpty())
    {
        NX_WARNING(this, "startUpload(%1) - nothing to upload", contents.info.version);
    }
    else
    {
        for (const auto& package: contents.info.packages)
        {
            if (package.isServer())
                toUpload += uploadPackage(package, contents.storageDir);
        }
    }

    if (toUpload > 0)
        NX_VERBOSE(this, "startUpload(%1) - started %2 uploads", contents.info.version, toUpload);

    if (m_activeUploads.empty() && !m_completedUploads.empty())
        changeUploadState(OfflineUpdateState::done);
    else
        changeUploadState(OfflineUpdateState::push);
    return true;
}

void ServerUpdateTool::stopUpload()
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

bool ServerUpdateTool::verifyUpdateManifest(
    UpdateContents& contents,
    const std::set<nx::utils::SoftwareVersion>& clientVersions,
    bool checkClient) const
{
    NX_ASSERT(m_stateTracker);
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers = m_stateTracker->activeServers();

    ClientVerificationData clientData;
    clientData.fillDefault();
    clientData.clientId = checkClient ? m_stateTracker->getClientPeerId() : QnUuid();
    clientData.installedVersions = clientVersions;
    return verifyUpdateContents(commonModule(), contents, activeServers, clientData);
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

using UpdateStatusAll = rest::RestResultWithData<std::vector<nx::update::Status>>;

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
    auto delta = qnSyncTime->currentMSecsSinceEpoch() - m_timeStartedInstall;
    return std::chrono::milliseconds(delta);
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

    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        NX_INFO(this, "requestStopAction() - sending request");
        dropAllRequests("requestStopAction()");
        connection->updateActionStop(nx::utils::guarded(this,
            [this](bool success, rest::Handle /*handle*/)
            {
                // This will be called in outer
                NX_VERBOSE(this, "requestStopAction() - success=%1", success);
                m_remoteUpdateManifest = nx::update::Information();
                m_requestingStop = false;
                emit cancelUpdateComplete(success);
            }), thread());
    }

    for (const auto& file: m_activeDownloads)
        m_downloader->deleteFile(file.first);
    m_activeDownloads.clear();
    m_stateTracker->clearState();
    return true;
}

bool ServerUpdateTool::requestRetryAction()
{
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        NX_INFO(this, "requestRetryAction() - sending request");
        m_expectedStatusHandle = connection->retryUpdate(nx::utils::guarded(this,
            [this, tool = QPointer<ServerUpdateTool>(this)](
                bool success,
                rest::Handle handle,
                rest::UpdateStatusAllData response)
            {
                if (response.error != QnRestResult::NoError)
                {
                    NX_VERBOSE(this,
                        "requestRetryAction: An error in response to the /ec2/retryUpdate request: code=%1, err=%2",
                        response.error, response.errorString);
                }

                if (tool)
                    tool->atUpdateStatusResponse(success, handle, response.data);
            }), thread());
        return m_expectedStatusHandle != 0;
    }
    return false;
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
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        NX_VERBOSE(this, "requestFinishUpdate(%1) - sending request", skipActivePeers);
        connection->updateActionFinish(skipActivePeers, nx::utils::guarded(this,
            [this, skipActivePeers](bool success, rest::Handle /*handle*/)
            {
                NX_VERBOSE(this, "requestFinishUpdate(%1) - got response", skipActivePeers);
                m_requestingFinish = false;
                emit finishUpdateComplete(success);
            }), thread());
        m_stateTracker->clearState();
        return true;
    }

    NX_VERBOSE(this, "requestFinishUpdate(%1) - no connection to the server", skipActivePeers);
    return false;
}

void ServerUpdateTool::requestStartUpdate(
    const nx::update::Information& info,
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

    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        dropAllRequests("requestStartUpdate()");
        auto callback = [this](bool success, rest::Handle /*requestId*/)
            {
                // TODO: Just forcing it, until server side responds with
                // a proper nx::update::Information
                success = true;
                emit startUpdateComplete(success);
            };
        connection->updateActionStart(info, nx::utils::guarded(this, callback));
        m_remoteUpdateManifest = info;
    }

    m_remoteUpdateStatus = {};
}

void ServerUpdateTool::requestInstallAction(
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

    NX_VERBOSE(this) << "requestInstallAction() for" << servers;
    m_remoteUpdateStatus = {};

    m_timeStartedInstall = qnSyncTime->currentMSecsSinceEpoch();
    m_serversAreInstalling = servers;

    auto callback = [tool = QPointer<ServerUpdateTool>(this)](bool success, rest::Handle handle)
        {
            if (!success)
                NX_ERROR(typeid(ServerUpdateTool)) << "requestInstallAction() - response success=false";
            if (tool)
            {
                tool->m_requestingInstall.remove(handle);
                tool->requestRemoteUpdateStateAsync();
            }
        };

    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        dropAllRequests("requestInstallAction()");
        if (auto handle = connection->updateActionInstall(servers, callback, thread()))
            m_requestingInstall.insert(handle);
    }
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
        if (auto ec2connection = commonModule()->ec2Connection())
        {
            QnConnectionInfo connectionInfo = ec2connection->connectionInfo();
            QnUuid serverId = QnUuid(connectionInfo.ecsGuid);
            nx::utils::Url serverUrl = connectionInfo.ecUrl;
            m_serverConnection.reset(new rest::ServerConnection(commonModule(), serverId, serverUrl));
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

void ServerUpdateTool::atUpdateStatusResponse(bool success, rest::Handle handle,
    const std::vector<nx::update::Status>& response)
{
    m_checkingRemoteUpdateStatus = false;

    if (m_expectedStatusHandle != handle)
    {
        NX_VERBOSE(this) << "atUpdateStatusResponse handle" << handle << "was skipped";
        return;
    }

    if (!success)
        return;

    RemoteStatus remoteStatus;
    for (const auto& status : response)
        remoteStatus[status.serverId] = status;

    m_expectedStatusHandle = 0;
    m_remoteUpdateStatus = std::move(remoteStatus);
}

rest::QnConnectionPtr ServerUpdateTool::getServerConnection(const QnMediaServerResourcePtr& server) const
{
    return server ? server->restConnection() : rest::QnConnectionPtr();
}

void ServerUpdateTool::requestRemoteUpdateStateAsync()
{
    // Request another state only if there is no pending request.
    if (m_checkingRemoteUpdateStatus)
        return;

    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        // We need to track both /ec2/updateStatus and /ec2/updateInformation to get relevant
        // information. /ec2/updateInformation is necessary to keep track of the servers
        // participating in the update.
        auto callback =
            [this, tool = QPointer<ServerUpdateTool>(this)](
                bool success,
                rest::Handle handle,
                rest::UpdateStatusAllData response)
            {
                if (response.error != QnRestResult::NoError)
                {
                    NX_VERBOSE(this,
                        "requestRemoteUpdateStateAsync: An error in response to the /ec2/updateStatus request: code=%1, err=%2",
                        response.error, response.errorString);
                }

                if (tool)
                    tool->atUpdateStatusResponse(success, handle, response.data);
            };

        // Requesting update status for mediaservers.
        m_checkingRemoteUpdateStatus = true;
        m_expectedStatusHandle = connection->getUpdateStatus(callback, thread());

        // Requesting remote update info.
        rest::Handle handle = connection->getUpdateInfo(
            [tool = QPointer<ServerUpdateTool>(this)](
                bool success, rest::Handle handle, rest::UpdateInformationData response)
            {
                if (!tool)
                    return;

                tool->m_activeRequests.remove(handle);
                if (success && response.error == QnRestResult::NoError)
                {
                    if (tool->m_skippedRequests.contains((handle)))
                        return;
                    tool->m_remoteUpdateManifest = response.data;
                    tool->m_serversAreInstalling = QSet<QnUuid>::fromList(response.data.participants);
                }
            }, thread());
        m_activeRequests.insert(handle);
    }
}

std::future<ServerUpdateTool::RemoteStatus> ServerUpdateTool::requestRemoteUpdateState()
{
    auto promise = std::make_shared<std::promise<RemoteStatus>>();
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        connection->getUpdateStatus(
            [this, promise, tool = QPointer<ServerUpdateTool>(this)](
                 bool success,
                 rest::Handle handle,
                 rest::UpdateStatusAllData response)
            {
                if (response.error != QnRestResult::NoError)
                {
                    NX_DEBUG(
                        this,
                        lm("requestRemoteUpdateState: An error in response to the /ec2/updateStatus request: %1")
                            .args(response.errorString));
                }

                if (tool)
                    tool->atUpdateStatusResponse(success, handle, response.data);
                RemoteStatus remoteStatus;
                for (const auto& status: response.data)
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

QString ServerUpdateTool::getServerAuthString() const
{
    const auto& connectionInfo = commonModule()->ec2Connection()->connectionInfo();
    nx::utils::Url serverUrl = connectionInfo.ecUrl;
    if (serverUrl.scheme().isEmpty())
        serverUrl.setScheme(nx::network::http::urlSheme(connectionInfo.allowSslConnections));

    return QnStartupParameters::createAuthenticationString(serverUrl, connectionInfo.version);
}

std::shared_ptr<ServerUpdatesModel> ServerUpdateTool::getModel()
{
    return m_updatesModel;
}

std::shared_ptr<PeerStateTracker> ServerUpdateTool::getStateTracker()
{
    return m_stateTracker;
}

QSet<QnUuid> ServerUpdateTool::getServersInstalling() const
{
    return m_serversAreInstalling;
}

const nx::update::Package* ServerUpdateTool::findPackageForFile(const QString& fileName) const
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
            m_failedDownloads.insert(it->first);
            break;
        case FileInformation::Status::uploading:
            break;
        case FileInformation::Status::downloaded:
            NX_VERBOSE(this) << "atDownloaderStatusChanged("<< fileInformation.name
                << ") - finally downloaded it to" << m_downloader->filePath(fileInformation.name);
            m_activeDownloads.erase(it);
            m_completeDownloads.insert(it->first);
            emit packageDownloaded(*package);
            break;
        case FileInformation::Status::corrupted:
            NX_VERBOSE(this) << "atDownloaderStatusChanged("<< fileInformation.name
                << ") - status changed to " << fileInformation.status;
            emit packageDownloadFailed(*package, "Update file is corrupted");
            m_activeDownloads.erase(it);
            m_failedDownloads.insert(it->first);
            break;
        case FileInformation::Status::downloading:
            it->second = fileInformation.calculateDownloadProgress();
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
        NX_ERROR(this)
            << "atDownloadFailed(" << fileName << ") - failed to download update package.";
    }
    else
    {
        NX_ERROR(this)
            << "atDownloadFailed(" << fileName << ") - did not found a package for a file."
            << "Why were we downloading this file?";
    }
}

nx::utils::Url getServerUrl(QnCommonModule* commonModule, QString path)
{
    if (const auto connection = commonModule->ec2Connection())
    {
        QnConnectionInfo connectionInfo = connection->connectionInfo();
        nx::utils::Url url = connectionInfo.ecUrl;
        url.setPath(path);
        bool hasSsl = commonModule->moduleInformation().sslAllowed;
        url.setScheme(nx::network::http::urlSheme(hasSsl));
        return url;
    }
    return "";
}

QString ServerUpdateTool::getUpdateStateUrl() const
{
    const auto url = getServerUrl(commonModule(), "/ec2/updateStatus");
    return url.toString(QUrl::RemoveUserInfo);
}

QString ServerUpdateTool::getUpdateInformationUrl() const
{
    const auto url = getServerUrl(commonModule(), "/ec2/updateInformation");
    return url.toString(QUrl::RemoveUserInfo);
}

QString ServerUpdateTool::getInstalledUpdateInfomationUrl() const
{
    auto url = getServerUrl(commonModule(), "/ec2/updateInfomation");
    QUrlQuery query;
    query.addQueryItem("version", "latest");
    url.setQuery(query);
    return url.toString(QUrl::RemoveUserInfo);
}

std::future<ServerUpdateTool::UpdateContents> ServerUpdateTool::checkLatestUpdate(
    const QString& updateUrl)
{
    return checkSpecificChangeset(updateUrl, kLatestChangeset);
}

std::future<ServerUpdateTool::UpdateContents> ServerUpdateTool::checkSpecificChangeset(
    const QString& updateUrl,
    const QString& changeset)
{
    auto connection = getServerConnection(commonModule()->currentServer());
    auto engineVersion = commonModule()->engineVersion();

    return std::async(
        [this, updateUrl, connection, engineVersion, changeset]() -> UpdateContents
        {
            UpdateContents contents;

            contents.changeset = changeset;
            contents.info = nx::update::updateInformation(updateUrl, engineVersion,
                changeset, &contents.error);
            if (changeset == kLatestChangeset)
                contents.sourceType = nx::update::UpdateSourceType::internet;
            else
                contents.sourceType = nx::update::UpdateSourceType::internetSpecific;
            contents.source = lit("%1 by serverUpdateTool for build=%2").arg(updateUrl, changeset);

            if (contents.error == nx::update::InformationError::networkError && connection)
            {
                NX_WARNING(NX_SCOPE_TAG, "Checking for updates using mediaserver as proxy");
                auto promise = std::make_shared<std::promise<bool>>();
                contents.source = lit("%1 by serverUpdateTool for build=%2 proxied by mediaserver").arg(updateUrl, changeset);
                contents.info = {};
                auto proxyCheck = promise->get_future();
                connection->checkForUpdates(changeset,
                    [this, promise = std::move(promise), &contents](bool success,
                        rest::Handle /*handle*/, rest::UpdateInformationData response)
                    {
                        if (success)
                        {
                            if (response.error != QnRestResult::NoError)
                            {
                                NX_DEBUG(
                                    this,
                                    lm("checkSpecificChangeset: An error in response to the /ec2/updateInformation request: %1")
                                        .args(response.errorString));

                                QnLexical::deserialize(response.errorString, &contents.error);
                            }
                            else
                            {
                                contents.error = nx::update::InformationError::noError;
                                contents.info = response.data;
                            }
                        }
                        else
                        {
                            contents.error = nx::update::InformationError::networkError;
                        }

                        promise->set_value(success);
                    });

                proxyCheck.wait();
            }

            return contents;
        });
}


nx::utils::SoftwareVersion getCurrentVersion(
    const nx::vms::api::SoftwareVersion& engineVersion,
    QnResourcePool* resourcePool)
{
    nx::utils::SoftwareVersion minimalVersion = engineVersion;
    const auto allServers = resourcePool->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr& server: allServers)
    {
        if (server->getVersion() < minimalVersion)
            minimalVersion = server->getVersion();
    }
    return minimalVersion;
}

QUrl generateUpdatePackageUrl(
    const nx::vms::api::SoftwareVersion& engineVersion,
    const nx::update::UpdateContents& contents,
    const QSet<QnUuid>& targets,
    QnResourcePool* resourcePool)
{
    bool useLatest = contents.sourceType == nx::update::UpdateSourceType::internet;
    auto changeset = contents.info.version;
    nx::utils::SoftwareVersion targetVersion = useLatest
        ? nx::utils::SoftwareVersion()
        : contents.getVersion();
    bool noFullVersion = false;
    // Checking if version has only build number.
    contents.info.version.toInt(&noFullVersion);

    QUrlQuery query;

    if (targetVersion.isNull())
    {
        query.addQueryItem("version", "latest");
        query.addQueryItem("current", getCurrentVersion(engineVersion, resourcePool).toString());
    }
    else
    {
        QString key = noFullVersion ? changeset : QString::number(targetVersion.build());
        QString password = passwordForBuild(key);
        // Changeset will contain either full version like "4.0.0.28340",
        // or only build number like "28340".
        query.addQueryItem("version", key);
        query.addQueryItem("password", password);
    }

    query.addQueryItem("customization", QnAppInfo::customizationName());

    QSet<nx::utils::OsInfo> osInfoList;
    for (const auto& id: targets)
    {
        const auto& server = resourcePool->getResourceById<QnMediaServerResource>(id);
        if (!server)
            continue;

        if (!server->getOsInfo().isValid())
            continue;

        if (!targetVersion.isNull() && server->getVersion() == targetVersion)
            continue;

        osInfoList.insert(server->getOsInfo());
    }

    query.addQueryItem("components",
        compactPackagesQuery(componentsListToJson(nx::utils::OsInfo::current(), osInfoList)));

    QString path = qnSettings->updateCombineUrl();
    if (path.isEmpty())
        path = QnAppInfo::updateGeneratorUrl();
    QUrl url(path);
    url.setQuery(query);

    return url;
}

} // namespace nx::vms::client::desktop
