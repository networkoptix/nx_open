#include "server_update_tool.h"

#include <QtCore/QThread>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <QtConcurrent/QtConcurrent>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/incompatible_server_watcher.h>

#include <utils/common/app_info.h>
#include <utils/update/update_utils.h>
#include <update/task/check_update_peer_task.h>
#include <update/low_free_space_warning.h>

#include <client/client_settings.h>
#include <client/desktop_client_message_processor.h>

#include <core/resource/fake_media_server.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>
#include <nx/client/desktop/utils/upload_manager.h>
#include <api/server_rest_connection.h>

#include <nx/vms/common/p2p/downloader/private/abstract_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/resource_pool_peer_manager.h>
#include <nx/vms/common/p2p/downloader/private/peer_selection/peer_selector_factory.h>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

namespace {

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

const QString kPackageIndexFile = "packages.json";
const QString kTempDataDir = "update_temp";
const QString kFilePrefix = "file://";
const int kReadBufferSizeBytes = 1024 * 1024;

} // namespace

namespace nx::vms::common::p2p::downloader {

// This is a hacky class to make P2P downloader work for client as well.
// It should be removed when P2P downloader is properly fixed.
class PeerManagerFactory:
    public AbstractPeerManagerFactory,
    public QnCommonModuleAware
{
    public:
        PeerManagerFactory(QnCommonModule* commonModule):
            QnCommonModuleAware(commonModule)
        {

        }

        virtual AbstractPeerManager* createPeerManager(
            FileInformation::PeerSelectionPolicy peerPolicy,
            const QList<QnUuid>& additionalPeers) override
        {
            auto selector = peer_selection::PeerSelectorFactory::create(peerPolicy, additionalPeers, commonModule());
            return new ResourcePoolPeerManager(commonModule(), std::move(selector), true);
        }
};
} // namespace nx::vms::common::p2p::downloader

namespace nx {
namespace client {
namespace desktop {

using UpdateCheckResult = ServerUpdateTool::UpdateContents;

ServerUpdateTool::ServerUpdateTool(QObject* parent):
    base_type(parent),
    m_outputDir(QDir::temp().absoluteFilePath("nx_updates/"))
{
    QString path = m_outputDir.path();
    qDebug() << "Will output temp files to " << path;

    /*
    /temp/nx_updates/rand_file
    m_peerManagerFactory.reset(new nx::vms::common::p2p::downloader::PeerManagerFactory(commonModule()));
    m_downloader.reset(new Downloader(QDir("downloads"), commonModule(), m_peerManagerFactory.get()));
    connect(m_downloader.get(), &Downloader::fileStatusChanged,
            this, &MultiServerUpdatesWidget::at_downloaderStatusChanged);
            */

    // Keeping this initializations here because I do not want to track
    // declaration order from the header.
    m_uploadManager.reset(new UploadManager());

    QString stateRaw = qnSettings->systemUpdaterState();
    // TODO: State consists of: multimap of files and servers for the upload + current state
}

ServerUpdateTool::~ServerUpdateTool()
{
}

/*
void ServerUpdateTool::at_downloaderStatusChanged(const FileInformation& fileInformation)
{
    // TODO: Some sort of an error here
    qDebug() << "ServerUpdateTool::at_downloaderStatusChanged("<< fileInformation.name
             << ") - status changed to " << fileInformation.status;
}
*/

std::future<UpdateCheckResult> ServerUpdateTool::checkLatestUpdate()
{
    QString updateUrl = qnSettings->updateFeedUrl();
    return std::async(std::launch::async,
        [this, updateUrl]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, nx::update::kLatestVersion, &result.error);
            result.mode = UpdateCheckMode::internet;
            result.source = lit("%1?ver=%2").arg(updateUrl, nx::update::kLatestVersion);
            verifyUpdateManifest(result);
            return result;
        });
}

std::future<UpdateCheckResult> ServerUpdateTool::checkSpecificChangeset(QString build, QString password)
{
    QString updateUrl = qnSettings->updateFeedUrl();

    return std::async(std::launch::async,
        [this, updateUrl, build]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, build, &result.error);
            result.mode = UpdateCheckMode::internetSpecific;
            result.source = lit("%1?ver=%2").arg(updateUrl, build);
            verifyUpdateManifest(result);
            return result;
        });
}

std::future<ServerUpdateTool::UpdateContents> ServerUpdateTool::checkUpdateFromFile(QString file)
{
    qDebug() << "ServerUpdateTool::checkUpdateFromFile(" << file << ")";

    m_outputDir.removeRecursively();
    if (!m_outputDir.exists())
        m_outputDir.mkpath(".");

    m_localUpdateFile = file;
    m_offlineUpdaterState = OfflineUpdateState::unpack;
    m_extractor = std::make_shared<QnZipExtractor>(file, m_outputDir.path());
    m_offlineUpdateCheckResult = std::promise<UpdateContents>();
    m_extractor->start();
    connect(m_extractor.get(), &QnZipExtractor::finished, this, &ServerUpdateTool::at_extractFilesFinished);

    // TODO: exit if there is already some work issued
    return m_offlineUpdateCheckResult.get_future();
}

void ServerUpdateTool::readUpdateManifest(QString path, UpdateContents& result)
{
    qDebug() << "ServerUpdateTool::readUpdateManifest(" << path << ")";
    // TODO: Should be able to find update manifest in the subdirectory.
    result.error = nx::update::InformationError::jsonError;
    result.mode = UpdateCheckMode::file;

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

// NOTE: We are probably not in the UI thread.
void ServerUpdateTool::at_extractFilesFinished(int code)
{
    // TODO: Add some thread safety here
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::unpack);

    UpdateContents contents;
    contents.mode = UpdateCheckMode::file;
    contents.source = lit("file://%1").arg(m_localUpdateFile);

    if (code != QnZipExtractor::Ok)
    {
        qDebug() << "ServerUpdateTool::at_extractFilesFinished() err=" << QnZipExtractor::errorToString((QnZipExtractor::Error)code);
        m_offlineUpdaterState = OfflineUpdateState::initial;
        contents.error = nx::update::InformationError::missingPackageError;
        m_offlineUpdateCheckResult.set_value(contents);
        return;
    }

    qDebug() << "ServerUpdateTool::at_extractFilesFinished() status = Ready";

    // Find a subfolter containing update manifest
    QDir packageDir = findFolderForFile(m_outputDir, kPackageIndexFile);

    QString manifestPath = packageDir.filePath(kPackageIndexFile);
    readUpdateManifest(manifestPath, contents);

    if (contents.error == nx::update::InformationError::noError)
    {
        m_uploadDestination = QString("updates/%1/").arg(contents.info.version);

        if (verifyUpdateManifest(contents, packageDir) && !contents.filesToUpload.empty())
        {
            contents.error = nx::update::InformationError::noError;
            m_offlineUpdaterState = OfflineUpdateState::ready;
        }
        else
        {
            m_offlineUpdaterState = OfflineUpdateState::initial;
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
        m_activeServers[server->getId()] = server;

    m_onAddedResource = connect(pool, &QnResourcePool::resourceAdded,
        this, &ServerUpdateTool::at_resourceAdded);
    m_onRemovedResource = connect(pool, &QnResourcePool::resourceRemoved,
        this, &ServerUpdateTool::at_resourceRemoved);
    // TODO: Should replace it by connecting to each resource
    m_onUpdatedResource = connect(pool, &QnResourcePool::resourceChanged,
        this, &ServerUpdateTool::at_resourceChanged);
}

QnMediaServerResourceList ServerUpdateTool::getServersForUpload()
{
    QnMediaServerResourceList result;
    for (auto server: m_activeServers)
        result.push_back(server.second);
    return result;
}

ServerUpdateTool::OfflineUpdateState ServerUpdateTool::getUploaderState() const
{
    return m_offlineUpdaterState;
}

void ServerUpdateTool::at_uploadWorkerState(QnUuid serverId, const UploadState& state)
{
    if (!m_uploadState.count(state.id))
    {
        qDebug() << "ServerUpdateTool::at_uploadWorkerState() no upload state with id=" << state.id;
        return;
    }

    auto server = m_activeServers[serverId];
    switch (state.status)
    {
        case UploadState::Done:
            qDebug() << "ServerUpdateTool::at_uploadWorkerState() uploaded file="
                     << state.destination << "server:" << serverId;
            markUploadCompleted(state.id);
            break;
        case UploadState::Uploading:
            qDebug() << "ServerUpdateTool::at_uploadWorkerState() uploading file="
                     << state.destination << "bytes"
                     << state.uploaded << "of" << state.size
                     << "server:" << serverId;
            break;
        case UploadState::Error:
            qDebug() << "ServerUpdateTool::at_uploadWorkerState() error with file="
                     << state.destination
                     << "error:" << state.errorMessage
                     << "server:" << serverId;
            markUploadCompleted(state.id);
            break;
        default:
            break;
    }

    m_uploadState[state.id] = state;
    m_offlineUpdateStateChanged = true;
}

void ServerUpdateTool::markUploadCompleted(QString uploadId)
{
    m_activeUploads.erase(uploadId);
    m_completedUploads.insert(uploadId);

    if (m_activeUploads.empty() && !m_completedUploads.empty())
    {
        m_offlineUpdaterState = OfflineUpdateState::done;
        m_offlineUpdateStateChanged = true;
    }
}

void ServerUpdateTool::startUpload(const UpdateContents& contents)
{
    qDebug() << "ServerUpdateTool::startUpload()";
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::ready);
    QnMediaServerResourceList recipients = getServersForUpload();

    for (auto id: m_activeUploads)
        m_uploadManager->cancelUpload(id);

    m_activeUploads.clear();
    m_completedUploads.clear();
    m_uploadState.clear();

    for(auto server: recipients)
    {
        QnUuid serverId = server->getId();
        for (auto path: contents.filesToUpload)
        {
            auto callback = [this, serverId](const UploadState& state)
            {
                at_uploadWorkerState(serverId, state);
            };

            UploadState config;
            config.source = path;
            config.destination = m_uploadDestination + path;
            // Updates should land to updates/publication_key/file_name.
            config.ttl = -1; //< This should mean 'infinite time'
            auto id = m_uploadManager->addUpload(server, config, this, callback);

            if (!id.isEmpty())
            {
                m_uploadState[id] = config;
                m_activeUploads.insert(id);
            }
            else
            {
                qDebug() << "ServerUpdateTool::uploadNext - failed to start uploading file="
                         << path
                         << "reason="
                         << config.errorMessage;
                m_completedUploads.insert(id);
            }
        }
    }

    if (m_activeUploads.empty() && !m_completedUploads.empty())
        m_offlineUpdaterState = OfflineUpdateState::done;
    else
        m_offlineUpdaterState = OfflineUpdateState::push;

    m_offlineUpdateStateChanged = true;
}

void ServerUpdateTool::stopUpload()
{
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::push);
    // TODO: Implement it
    for (auto record: m_uploadState)
    {
        m_uploadManager->cancelUpload(record.first);
    }

    m_activeUploads.clear();
    m_completedUploads.clear();
    m_uploadState.clear();
    qDebug() << "ServerUpdateTool::stopUpload()";
    m_offlineUpdaterState = OfflineUpdateState::ready;
}

const nx::update::Package* findPackageForServer(
    QnMediaServerResourcePtr server, const nx::update::Information& info)
{
    auto serverInfo = server->getSystemInfo();

    for(const auto& pkg: info.packages)
    {
        if (pkg.component == nx::update::kComponentServer)
        {
            // Check arch and OS
            if (pkg.arch == serverInfo.arch
                && pkg.platform == serverInfo.platform
                && pkg.variant == serverInfo.modification)
                return &pkg;
        }
    }
    return nullptr;
}

bool ServerUpdateTool::UpdateContents::isValid() const
{
    return missingUpdate.empty()
        && invalidVersion.empty()
        && error == nx::update::InformationError::noError;
}

bool ServerUpdateTool::verifyUpdateManifest(UpdateContents& contents, QDir root) const
{
    const nx::update::Information& info = contents.info;

    nx::utils::SoftwareVersion targetVersion(info.version);

    contents.invalidVersion.clear();
    contents.missingUpdate.clear();
    contents.filesToUpload.clear();

    // Check if some packages from manifest do not exist.
    if (contents.mode == UpdateCheckMode::file)
    {
        QList<nx::update::Package> checked;
        for(auto& pkg: contents.info.packages)
        {
            if (contents.mode == UpdateCheckMode::file)
            {
                QFileInfo file(root.filePath(pkg.file));
                if (file.exists())
                {
                    contents.filesToUpload.push_back(pkg.file);
                    pkg.url = "";
                    pkg.file = m_uploadDestination + file.fileName();
                    checked.push_back(pkg);
                }
                else
                {
                    NX_ERROR(this) << " missing update file" << pkg.file;
                }
            }
        }
        contents.info.packages = checked;
    }

    if (contents.info.eulaLink.startsWith(kFilePrefix))
    {
        // Need to adjust eula link to absolute path to this files
        QString eulaLocalPath = contents.info.eulaLink.mid(kFilePrefix.size());
        contents.eulaPath = kFilePrefix + root.filePath(eulaLocalPath);
    }
    else
    {
        contents.eulaPath = contents.info.eulaLink;
    }

    // Find client package
    contents.clientPackage = nx::update::Package();
    const auto modification = QnAppInfo::applicationPlatformModification();
    auto arch = QnAppInfo::applicationArch();
    auto runtime = nx::vms::api::SystemInformation::currentSystemRuntime().replace(L' ', L'_');

    for(auto& pkg: contents.info.packages)
    {
        if (pkg.component == nx::update::kComponentClient)
        {
            // Check arch and OS
            if (pkg.arch == arch && pkg.variant == modification)
            {
                contents.clientPackage = pkg;
                break;
            }
        }
    }

    // Checking if all servers have update packages.
    for(auto record: m_activeServers)
    {
        auto server = record.second;
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);
        if (!isOurServer)
            continue;
        auto serverInfo = server->getSystemInfo();

        auto package = findPackageForServer(server, info);
        if (!package)
        {
            NX_ERROR(this) << "verifyUpdateManifest server "
                 << server->getId()
                 << "arch" << serverInfo.arch
                 << "platform" << serverInfo.platform
                 << "is missing its update package";
            contents.missingUpdate.insert(server);
        }

        nx::utils::SoftwareVersion serverVersion = server->getVersion();
        // Prohibiting updates to previous version
        if (serverVersion > targetVersion)
        {
            NX_ERROR(this) << "verifyUpdateManifest server "
                     << server->getId()
                     << "ver" << serverVersion.toString()
                     << "is incompatible with this update"
                     << "ver" << targetVersion.toString();
            contents.invalidVersion.insert(server);
        }
    }

    if (!contents.missingUpdate.empty())
        contents.error = nx::update::InformationError::missingPackageError;

    return contents.isValid();
}

ServerUpdateTool::ProgressInfo ServerUpdateTool::calculateUploadProgress()
{
    ProgressInfo result;

    for(const auto record: m_uploadState)
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

ServerUpdateTool::ProgressInfo ServerUpdateTool::calculateRemoteDownloadProgress()
{
    ProgressInfo result;
    return result;
}

ServerUpdateTool::ProgressInfo ServerUpdateTool::calculateInstallProgress()
{
    ProgressInfo result;
    return result;
}

void ServerUpdateTool::at_resourceAdded(const QnResourcePtr& resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        m_activeServers[server->getId()] = server;
}

void ServerUpdateTool::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        m_activeServers.erase(server->getId());
}

void ServerUpdateTool::at_resourceChanged(const QnResourcePtr& resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;
}

/**
 * Generates URL for upcombiner.
 * Upcombiner is special server utility, that combines several update packages
 * to a single zip archive.
 */
QUrl ServerUpdateTool::generateUpdatePackageUrl(const nx::utils::SoftwareVersion &targetVersion,
    const QString& targetChangeset, const QSet<QnUuid>& targets, QnResourcePool* resourcePool)
{
    QUrlQuery query;

    if (targetVersion.isNull())
    {
        query.addQueryItem(lit("version"), lit("latest"));
        query.addQueryItem(lit("current"), getCurrentVersion(resourcePool).toString());
    }
    else
    {
        const auto key = targetChangeset.isEmpty()
            ? QString::number(targetVersion.build())
            : targetChangeset;

        query.addQueryItem(lit("version"), targetVersion.toString());
        query.addQueryItem(lit("password"), passwordForBuild(key));
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

    query.addQueryItem(lit("client"), nx::vms::api::SystemInformation::currentSystemRuntime().replace(L' ', L'_'));
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

nx::update::Information ServerUpdateTool::getActiveUpdateInformation() const
{
    return m_updateManifest;
}

void ServerUpdateTool::requestStopAction()
{
    m_updateManifest = nx::update::Information();
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        connection->updateActionStop({});
    }
}

void ServerUpdateTool::requestStartUpdate(const nx::update::Information& info)
{
    if (auto connection = getServerConnection(commonModule()->currentServer()))
    {
        connection->updateActionStart(info);
        m_updateManifest = info;

#ifdef TO_BE_REMOVED
        // TODO: We already have a URL to client update. We should download and install it directly.
        FileInformation info;
        info.md5 = QByteArray::fromHex(m_clientUpdatePackage.md5.toLatin1());
        info.size = m_clientUpdatePackage.size;
        info.name = m_clientUpdatePackage.file;
        info.url = m_clientUpdatePackage.url;
        auto code = m_downloader->addFile(info);
        using Code = vms::common::p2p::downloader::ResultCode;

        switch(code)
        {
            case Code::ok:
                qDebug() << "MultiServerUpdatesWidget::at_startUpdateAction() - downloading client package"
                         << info.name;
                break;
            case Code::fileAlreadyExists:
                qDebug() << "MultiServerUpdatesWidget::at_startUpdateAction() - file is already here"
                         << info.name;
                break;
            default:
                // TODO: Some sort of an error here
                qDebug() << "MultiServerUpdatesWidget::at_startUpdateAction() - failed to add client package "
                         << info.name
                         << vms::common::p2p::downloader::toString(code);
                break;
        }
#endif
    }
}

void ServerUpdateTool::requestInstallAction(QSet<QnUuid> targets)
{
    for (auto it : m_activeServers)
        if (auto connection = getServerConnection(it.second))
            connection->updateActionInstall({});
}

void ServerUpdateTool::at_updateStatusResponse(bool success, rest::Handle handle,
    const std::vector<nx::update::Status>& response)
{
    m_checkingRemoteUpdateStatus = false;

    if (!success)
        return;

    for (const auto& status : response)
    {
        m_remoteUpdateStatus[status.serverId] = status;
    }
}

void ServerUpdateTool::at_updateStatusResponse(bool success, rest::Handle handle,
    const nx::update::Status& response)
{
    if (!success)
        return;

    m_remoteUpdateStatus[response.serverId] = response;
}

rest::QnConnectionPtr ServerUpdateTool::getServerConnection(const QnMediaServerResourcePtr& server)
{
    return server ? server->restConnection() : rest::QnConnectionPtr();
}

void ServerUpdateTool::requestRemoteUpdateState()
{
    // Request another state only if there is no pending request
    if (!m_checkingRemoteUpdateStatus)
    {
        if (auto connection = getServerConnection(commonModule()->currentServer()))
        {
            using UpdateStatusAll = std::vector<nx::update::Status>;
            auto callback = [this](bool success, rest::Handle handle, const UpdateStatusAll& response)
                {
                    at_updateStatusResponse(success, handle, response);
                };

            m_checkingRemoteUpdateStatus = true;
            connection->getUpdateStatus(callback, thread());

            connection->getUpdateInfo(
                [this](bool success, rest::Handle handle, const nx::update::Information& response)
                {
                    m_updateManifest = response;
                }, thread());
        }
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
