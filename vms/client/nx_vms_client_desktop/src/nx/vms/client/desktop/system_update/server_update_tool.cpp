#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/incompatible_server_watcher.h>

#include <utils/common/app_info.h>
#include <utils/update/update_utils.h>

#include <client/client_settings.h>
#include <client/client_module.h>
#include <watchers/cloud_status_watcher.h>

#include <core/resource/fake_media_server.h>
#include <api/global_settings.h>
#include <api/server_rest_connection.h>
#include <network/system_helpers.h>
#include <nx/utils/app_info.h>
#include <nx/vms/client/desktop/utils/upload_manager.h>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include "server_update_tool.h"
#include "server_updates_model.h"

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

const QString kPackageIndexFile = "packages.json";
const QString kTempDataDir = "update_temp";
const QString kFilePrefix = "file://";
const int kReadBufferSizeBytes = 1024 * 1024;

} // namespace

namespace nx::vms::client::desktop {

ServerUpdateTool::ServerUpdateTool(QObject* parent):
    base_type(parent),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/offline"))
{
    // Expecting paths like /temp/nx_updates/offline/rand_file
    QString path = m_outputDir.path();
    NX_VERBOSE(this) << "ServerUpdateTool will output temp files to " << path;

    m_uploadManager.reset(new UploadManager());
    m_updatesModel.reset(new ServerUpdatesModel(this));

    loadInternalState();
}

ServerUpdateTool::~ServerUpdateTool()
{
    saveInternalState();
    NX_VERBOSE(this) << "~ServerUpdateTool() done";
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
    QString raw = qnSettings->systemUpdaterState();
    StoredState stored;
    if (QJson::deserialize(raw, &stored))
    {
        switch((OfflineUpdateState)stored.state)
        {
            case OfflineUpdateState::unpack:
            case OfflineUpdateState::ready:
            case OfflineUpdateState::push:
                // We have no idea whether update files are still good on server.
                // The most simple solution - to restart upload process.
                // TODO: Check if we really need more robust state restoration.
                m_updateCheck = checkUpdateFromFile(stored.file);
                break;
            //case OfflineUpdateState::done:
            //case OfflineUpdateState::error:
            default:
                m_offlineUpdaterState = OfflineUpdateState::initial;
        }
    }
}

void ServerUpdateTool::saveInternalState()
{
    StoredState stored;
    stored.file = m_localUpdateFile;
    stored.state = (int)m_offlineUpdaterState;

    QByteArray raw;
    QJson::serialize(stored, &raw);
    qnSettings->setSystemUpdaterState(raw);
}

std::future<UpdateContents> ServerUpdateTool::getUpdateCheck()
{
    return std::move(m_updateCheck);
}

std::future<UpdateContents> ServerUpdateTool::checkLatestUpdate()
{
    QString updateUrl = qnSettings->updateFeedUrl();
    return std::async(std::launch::async,
        [updateUrl]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, nx::update::kLatestVersion, &result.error);
            result.sourceType = UpdateSourceType::internet;
            result.source = lit("%1 for build=%2").arg(updateUrl, nx::update::kLatestVersion);
            return result;
        });
}

std::future<UpdateContents> ServerUpdateTool::checkSpecificChangeset(QString build)
{
    QString updateUrl = qnSettings->updateFeedUrl();

    return std::async(std::launch::async,
        [updateUrl, build]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, build, &result.error);
            result.sourceType = UpdateSourceType::internetSpecific;
            result.source = lit("%1 for build=%2").arg(updateUrl, build);
            return result;
        });
}

std::future<UpdateContents> ServerUpdateTool::checkUpdateFromFile(QString file)
{
    NX_VERBOSE(this) << "checkUpdateFromFile(" << file << ")";

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

    return m_offlineUpdateCheckResult.get_future();;
}

std::future<UpdateContents> ServerUpdateTool::checkRemoteUpdateInfo()
{
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        auto promise = std::make_shared<std::promise<UpdateContents>>();
        auto result = promise->get_future();
        // Requesting remote update info.
        connection->getUpdateInfo(
            [promise = std::move(promise)](bool success, rest::Handle handle, const nx::update::Information& response)
            {
                UpdateContents contents;
                if (success)
                {
                    contents.info = response;
                }
                contents.sourceType = UpdateSourceType::mediaservers;
                promise->set_value(contents);
            });
        return result;
    }
    else
    {
        NX_WARNING(this) << "requestRemoteUpdateInfo() - have no connection to the server";
        std::promise<UpdateContents> emptyPromise;
        auto result = emptyPromise.get_future();
        emptyPromise.set_value(UpdateContents());
        return result;
    }
}

void ServerUpdateTool::changeUploadState(OfflineUpdateState newState)
{
    if (m_offlineUpdaterState == newState)
        return;
    m_offlineUpdaterState = newState;
    m_offlineUpdateStateChanged = true;
    saveInternalState();
}

void ServerUpdateTool::readUpdateManifest(QString path, UpdateContents& result)
{
    result.error = nx::update::InformationError::jsonError;
    result.sourceType = UpdateSourceType::file;

    QFile file(path);

    if (!file.open(QFile::ReadOnly))
        return;

    QByteArray buf(kReadBufferSizeBytes, 0);

    qint64 read = file.read(buf.data(), kReadBufferSizeBytes);
    if (read == -1)
        return;

    buf.truncate(read);
    if (!QJson::deserialize(buf, &result.info))
        return;

    result.error = nx::update::InformationError::noError;
}

// NOTE: We are probably not in the UI thread.
void ServerUpdateTool::atExtractFilesFinished(int code)
{
    // TODO: Add some thread safety here
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::unpack);

    UpdateContents contents;
    contents.sourceType = UpdateSourceType::file;
    contents.source = lit("file://%1").arg(m_localUpdateFile);

    if (code != QnZipExtractor::Ok)
    {
        NX_VERBOSE(this) << "atExtractFilesFinished() err=" << QnZipExtractor::errorToString((QnZipExtractor::Error)code);
        changeUploadState(OfflineUpdateState::initial);
        contents.error = nx::update::InformationError::missingPackageError;
        m_offlineUpdateCheckResult.set_value(contents);
        return;
    }

    NX_VERBOSE(this) << "atExtractFilesFinished() status = Ready";

    // Find a subfolter containing update manifest
    QDir packageDir = findFolderForFile(m_outputDir, kPackageIndexFile);
    contents.storageDir = packageDir;

    QString manifestPath = packageDir.filePath(kPackageIndexFile);
    readUpdateManifest(manifestPath, contents);

    if (contents.error == nx::update::InformationError::noError)
    {
        m_uploadDestination = QString("updates/%1/").arg(contents.info.version);

        // TODO: Move this verification to multi_server_updates_widget.cpp
        if (verifyUpdateManifest(contents) && !contents.filesToUpload.empty())
        {
            contents.error = nx::update::InformationError::noError;
            changeUploadState(OfflineUpdateState::ready);
        }
        else
        {
            changeUploadState(OfflineUpdateState::initial);
            if (contents.error == nx::update::InformationError::noError)
                contents.error = nx::update::InformationError::missingPackageError;
        }
    }

    m_offlineUpdateCheckResult.set_value(contents);
}

void ServerUpdateTool::setResourceFeed(QnResourcePool* pool)
{
    QObject::disconnect(m_onAddedResource);
    QObject::disconnect(m_onRemovedResource);
    QObject::disconnect(m_onUpdatedResource);

    m_activeServers.clear();
    const auto allServers = pool->getAllServers(Qn::AnyStatus);
    for (const QnMediaServerResourcePtr &server : allServers)
    {
        const auto status = server->getStatus();
        if (status == Qn::Offline || status == Qn::Unauthorized)
            continue;
        m_activeServers[server->getId()] = server;
    }

    m_onAddedResource = connect(pool, &QnResourcePool::resourceAdded,
        this, &ServerUpdateTool::atResourceAdded);
    m_onRemovedResource = connect(pool, &QnResourcePool::resourceRemoved,
        this, &ServerUpdateTool::atResourceRemoved);
    // TODO: Should replace it by connecting to each resource
    m_onUpdatedResource = connect(pool, &QnResourcePool::resourceChanged,
        this, &ServerUpdateTool::atResourceChanged);
}

QnMediaServerResourceList ServerUpdateTool::getServersForUpload()
{
    QnMediaServerResourceList result;
    auto items = m_updatesModel->getServerData();
    for(auto record: items)
    {
        auto server = record->server;
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);

        auto status = server->getStatus();
        bool online = status == Qn::ResourceStatus::Online;

        if (isOurServer && online && record->storeUpdates)
            result.push_back(server);
    }
    return result;
}

ServerUpdateTool::OfflineUpdateState ServerUpdateTool::getUploaderState() const
{
    return m_offlineUpdaterState;
}

void ServerUpdateTool::atUploadWorkerState(QnUuid serverId, const UploadState& state)
{
    if (!m_uploadStateById.count(state.id))
    {
        NX_VERBOSE(this) << "atUploadWorkerState() no upload state with id=" << state.id;
        return;
    }

    auto server = m_activeServers[serverId];
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

void ServerUpdateTool::markUploadCompleted(QString uploadId)
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
        // TODO: Do something meaningfull here
        return false;
    }

    for (auto id: m_activeUploads)
        m_uploadManager->cancelUpload(id);

    m_activeUploads.clear();
    m_completedUploads.clear();
    m_uploadStateById.clear();

    for(auto server: recipients)
    {
        QnUuid serverId = server->getId();
        for (auto path: contents.filesToUpload)
        {
            auto callback = [tool=QPointer<ServerUpdateTool>(this), serverId](const UploadState& state)
            {
                if (tool)
                    tool->atUploadWorkerState(serverId, state);
            };

            UploadState config;
            config.source = contents.storageDir.absoluteFilePath(path);
            config.destination = m_uploadDestination + path;
            // Updates should land to updates/publication_key/file_name.
            config.ttl = -1; //< This should mean 'infinite time'
            auto id = m_uploadManager->addUpload(server, config, this, callback);

            if (!id.isEmpty())
            {
                m_uploadStateById[id] = config;
                m_activeUploads.insert(id);
            }
            else
            {
                NX_VERBOSE(this) << "uploadNext - failed to start uploading file="
                    << path
                    << "reason="
                    << config.errorMessage;
                m_completedUploads.insert(id);
            }
        }
    }

    if (m_activeUploads.empty() && !m_completedUploads.empty())
        changeUploadState(OfflineUpdateState::done);
    else
        changeUploadState(OfflineUpdateState::push);
    return true;
}

void ServerUpdateTool::stopUpload()
{
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::push);
    for (auto record: m_uploadStateById)
    {
        m_uploadManager->cancelUpload(record.first);
    }

    m_activeUploads.clear();
    m_completedUploads.clear();
    m_uploadStateById.clear();
    NX_VERBOSE(this) << "stopUpload()";
    changeUploadState(OfflineUpdateState::ready);
}

bool ServerUpdateTool::verifyUpdateManifest(UpdateContents& contents) const
{
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers;
    {
        std::scoped_lock<std::recursive_mutex> lock(m_statusLock);
        activeServers = m_activeServers;
    }

    return verifyUpdateContents(commonModule(), contents, activeServers);
}

ServerUpdateTool::ProgressInfo ServerUpdateTool::calculateUploadProgress()
{
    ProgressInfo result;

    for(const auto record: m_uploadStateById)
    {
        result.max += 100;
        qint64 progress = 0;
        switch (record.second.status)
        {
            case UploadState::Uploading:
                progress = 100*record.second.uploaded / record.second.size;
                break;
            case UploadState::Done:
                progress = 100;
                break;
            default:
                progress = 0;
                break;
        }
        result.current += progress;
    }
    return result;
}

void ServerUpdateTool::atResourceAdded(const QnResourcePtr& resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        m_activeServers[server->getId()] = server;
    // TODO: We should check new server for uploading operations
}

void ServerUpdateTool::atResourceRemoved(const QnResourcePtr& resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        m_activeServers.erase(server->getId());
    // TODO: We should remove this server from uploading operations
}

void ServerUpdateTool::atResourceChanged(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;
    // Relevant state transitions:
    //  - Offline->Online. Should start upload for this server
    //  - Online->Offline|Unauthorized|Incompatible. Should stop upload operation.
    //  - Stopped being part of this system: WTF?
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
    return m_updateManifest.isValid();
}

UpdateContents ServerUpdateTool::getRemoteUpdateContents() const
{
    UpdateContents contents;
    contents.sourceType = UpdateSourceType::mediaservers;
    contents.source = "mediaservers";
    contents.info = m_updateManifest;
    contents.clientPackage = findClientPackage(m_updateManifest);
    verifyUpdateManifest(contents);
    return contents;
}

void ServerUpdateTool::requestStopAction()
{
    m_updateManifest = nx::update::Information();
    if (auto connection = getServerConnection(commonModule()->currentServer()))
        connection->updateActionStop({});

    if (!m_activeRequests.empty())
    {
        m_skippedRequests.unite(m_activeRequests);
        NX_VERBOSE(this) << "requestStopAction() will skip requests" << m_skippedRequests;
    }

    m_updatesModel->clearState();
}

void ServerUpdateTool::requestStartUpdate(const nx::update::Information& info)
{
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        connection->updateActionStart(info);
        m_updateManifest = info;
    }

    if (!m_activeRequests.empty())
        m_skippedRequests.unite(m_activeRequests);
    m_remoteUpdateStatus = {};
}

void ServerUpdateTool::requestInstallAction(QSet<QnUuid> targets)
{
    if (!m_activeRequests.empty())
        m_skippedRequests.unite(m_activeRequests);
    m_remoteUpdateStatus = {};

    NX_VERBOSE(this) << "requestInstallAction() for" << targets;
    m_updatesModel->setServersInstalling(targets);

    for (auto it : m_activeServers)
        if (auto connection = getServerConnection(it.second))
            connection->updateActionInstall({});
}

void ServerUpdateTool::atUpdateStatusResponse(bool success, rest::Handle handle,
    const std::vector<nx::update::Status>& response)
{
    m_checkingRemoteUpdateStatus = false;
    m_activeRequests.remove(handle);
    if (m_skippedRequests.contains(handle))
    {
        m_skippedRequests.remove(handle);
        NX_VERBOSE(this) << "requestRemoteUpdateState handle" << handle << "was skipped";
        return;
    }

    if (!success)
        return;

    RemoteStatus remoteStatus;
    for (const auto& status : response)
    {
        remoteStatus[status.serverId] = status;
    }

    m_remoteUpdateStatus = std::move(remoteStatus);
}

rest::QnConnectionPtr ServerUpdateTool::getServerConnection(const QnMediaServerResourcePtr& server) const
{
    return server ? server->restConnection() : rest::QnConnectionPtr();
}

void ServerUpdateTool::requestRemoteUpdateState()
{
    // Request another state only if there is no pending request.
    if (m_checkingRemoteUpdateStatus)
        return;

    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        using UpdateStatusAll = std::vector<nx::update::Status>;

        auto callback = [tool=QPointer<ServerUpdateTool>(this)](bool success, rest::Handle handle, const UpdateStatusAll& response)
            {
                if (tool)
                    tool->atUpdateStatusResponse(success, handle, response);
            };

        m_timeStartedInstall = Clock::now();
        // Requesting update status for mediaservers.
        rest::Handle handle = 0;
        m_checkingRemoteUpdateStatus = true;
        handle = connection->getUpdateStatus(callback, thread());
        m_activeRequests.insert(handle);

        // Requesting remote update info.
        handle = connection->getUpdateInfo(
            [tool=QPointer<ServerUpdateTool>(this)](bool success, rest::Handle handle, const nx::update::Information& response)
            {
                if (!tool)
                    return;
                tool->m_activeRequests.remove(handle);
                if (success)
                    tool->m_updateManifest = response;
            }, thread());
        m_activeRequests.insert(handle);
    }
}

QSet<QnUuid> ServerUpdateTool::getAllServers() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdateTool::getServersInState(StatusCode state) const
{
    QSet<QnUuid> result;
    for (const auto& item: m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        if (item->state == state)
            result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdateTool::getOfflineServers() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        if (item->offline)
            result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdateTool::getLegacyServers() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        if (item->onlyLegacyUpdate)
            result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdateTool::getServersInstalling() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        if (item->installing)
            result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdateTool::getServersCompleteInstall() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        if (item->installed)
            result.insert(item->server->getId());
    }
    return result;
}

QSet<QnUuid> ServerUpdateTool::getServersWithChangedProtocol() const
{
    QSet<QnUuid> result;
    for (const auto& item : m_updatesModel->getServerData())
    {
        if (!item->server)
            continue;
        if (item->changedProtocol)
            result.insert(item->server->getId());
    }
    return result;
}

std::shared_ptr<ServerUpdatesModel> ServerUpdateTool::getModel()
{
    return m_updatesModel;
}

QString getServerUrl(QnCommonModule* commonModule, QString path)
{
    if (const auto connection = commonModule->ec2Connection())
    {
        QnConnectionInfo connectionInfo = connection->connectionInfo();
        nx::utils::Url url = connectionInfo.ecUrl;
        url.setPath(path);
        bool hasSsl = commonModule->moduleInformation().sslAllowed;
        url.setScheme(nx::network::http::urlSheme(hasSsl));
        return url.toString(QUrl::RemoveUserInfo);
    }
    return "";
}

QString ServerUpdateTool::getUpdateStateUrl() const
{
    return getServerUrl(commonModule(), "/ec2/updateStatus");
}

QString ServerUpdateTool::getUpdateInformationUrl() const
{
    return getServerUrl(commonModule(), "/ec2/updateInformation");
}

QString ServerUpdateTool::getInstalledUpdateInfomationUrl() const
{
    return getServerUrl(commonModule(), "/ec2/installedUpdateInfomation");
}

nx::utils::SoftwareVersion getCurrentVersion(QnResourcePool* resourcePool)
{
    nx::utils::SoftwareVersion minimalVersion = qnStaticCommon->engineVersion();
    const auto allServers = resourcePool->getAllServers(Qn::AnyStatus);
    for(const QnMediaServerResourcePtr &server: allServers)
    {
        if (server->getVersion() < minimalVersion)
            minimalVersion = server->getVersion();
    }
    return minimalVersion;
}

QUrl generateUpdatePackageUrl(const UpdateContents& contents, const QSet<QnUuid>& targets, QnResourcePool* resourcePool)
{
    bool useLatest = contents.sourceType == UpdateSourceType::internet;
    auto changeset = contents.info.version;
    nx::utils::SoftwareVersion targetVersion = useLatest ? nx::utils::SoftwareVersion() : contents.getVersion();

    QUrlQuery query;

    if (targetVersion.isNull())
    {
        query.addQueryItem(lit("version"), lit("latest"));
        query.addQueryItem(lit("current"), getCurrentVersion(resourcePool).toString());
    }
    else
    {
        QString key = QString::number(targetVersion.build());
        QString password = passwordForBuild(key);
        qDebug() << "Generated password" << password << "for key" << key;
        query.addQueryItem(lit("version"), targetVersion.toString());
        query.addQueryItem(lit("password"), password);
    }

    QSet<nx::vms::api::SystemInformation> systemInformationList;
    for (const auto& id: targets)
    {
        const auto& server = resourcePool->getResourceById<QnMediaServerResource>(id);
        if (!server)
            continue;

        bool incompatible = (server->getStatus() == Qn::Incompatible);

        if (server->getStatus() != Qn::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        if (!targetVersion.isNull() && server->getVersion() == targetVersion)
            continue;

        systemInformationList.insert(server->getSystemInfo());
    }

    auto clientPlatformModification = QnAppInfo::applicationPlatformModification();
    auto clientArch = QnAppInfo::applicationArch();
    auto clientPlatform = nx::utils::AppInfo::applicationPlatform();
    QString clientRuntime = QString("%1_%2_%3").arg(clientPlatform, clientArch, clientPlatformModification);

    query.addQueryItem(lit("client"), clientRuntime);
    for(const auto &systemInformation: systemInformationList)
        query.addQueryItem(lit("server"), systemInformation.toString().replace(L' ', L'_'));

    query.addQueryItem(lit("customization"), QnAppInfo::customizationName());

    QString path = qnSettings->updateCombineUrl();
    if (path.isEmpty())
        path = QnAppInfo::updateGeneratorUrl();
    QUrl url(path);
    url.setQuery(query);

    return url;
}

} // namespace nx::vms::client::desktop
