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
const int readBufferSize = 1024 * 1024;

} // anonymous namespace

namespace nx {
namespace client {
namespace desktop {

ServerUpdateTool::ServerUpdateTool(QObject* parent):
    base_type(parent),
    m_outputDir(kTempDataDir)
{
    /*
    m_peerManagerFactory.reset(new nx::vms::common::p2p::downloader::PeerManagerFactory(commonModule()));
    m_downloader.reset(new Downloader(QDir("downloads"), commonModule(), m_peerManagerFactory.get()));
    connect(m_downloader.get(), &Downloader::fileStatusChanged,
            this, &MultiServerUpdatesWidget::at_downloaderStatusChanged);
            */

    // Keeping this initializations here because I do not want to track
    // declaration order from the header.
    m_uploadManager.reset(new UploadManager());
}

ServerUpdateTool::~ServerUpdateTool()
{
}

/*
void MultiServerUpdatesWidget::at_downloaderStatusChanged(const FileInformation& fileInformation)
{
    // TODO: Some sort of an error here
    qDebug() << "MultiServerUpdatesWidget::at_downloaderStatusChanged("<< fileInformation.name
             << ") - status changed to " << fileInformation.status;
}

ServerUpdateTool::OfflineUpdateState ServerUpdateTool::iterateOfflineUpdater()
{
    switch(m_offlineUpdaterState)
    {
        case OfflineUpdateState::Initial:
            break;
        case OfflineUpdateState::Unpack:
            break;
        case OfflineUpdateState::Push:
            break;
        case OfflineUpdateState::Done:
            break;
    }
    return m_offlineUpdaterState;
}*/

ServerUpdateTool::UpdateCheckResult ServerUpdateTool::checkUpdateFromInternet(QString updateUrl)
{
    UpdateCheckResult result;
    result.info = nx::update::updateInformation(updateUrl, nx::update::kLatestVersion, &result.error);
    return result;
}

std::future<ServerUpdateTool::UpdateCheckResult> ServerUpdateTool::checkUpdateFromFile(QString file)
{
    qDebug() << "ServerUpdateTool::checkUpdateFromFile(" << file << ")";

    m_offlineUpdaterState = OfflineUpdateState::Unpack;
    m_extractor = std::make_shared<QnZipExtractor>(file, m_outputDir.path());
    m_offlineUpdateCheckResult = std::promise<UpdateCheckResult>();
    m_extractor->start();
    connect(m_extractor.get(), &QnZipExtractor::finished, this, &ServerUpdateTool::at_extractFilesFinished);

    // TODO: exit if there is already some work issued
    return m_offlineUpdateCheckResult.get_future();
}

ServerUpdateTool::UpdateCheckResult ServerUpdateTool::readUpdateManifest(QString path)
{
    qDebug() << "ServerUpdateTool::readUpdateManifest(" << path << ")";
    // TODO: Should be able to find update manifest in the subdirectory.
    UpdateCheckResult result;
    result.error = nx::update::InformationError::jsonError;
    result.mode = CheckMode::File;

    QFile file(path);

    if (!file.open(QFile::ReadOnly))
        return result;

    QByteArray buf(readBufferSize, 0);

    qint64 read = file.read(buf.data(), readBufferSize);
    if (read == -1)
        return result;

    buf.truncate(read);
    if (!QJson::deserialize(buf, &result.info))
        return result;

    result.error = nx::update::InformationError::noError;

    return result;
}

// NOTE: We are probably not in the UI thread.
void ServerUpdateTool::at_extractFilesFinished(int code)
{
    // TODO: Add some thread safety here
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::Unpack);
    if (code != QnZipExtractor::Ok)
    {
        qDebug() << "ServerUpdateTool::at_extractFilesFinished() err=" << QnZipExtractor::errorToString((QnZipExtractor::Error)code);
        m_offlineUpdaterState = OfflineUpdateState::Initial;
        UpdateCheckResult result = {CheckMode::File, {}, nx::update::InformationError::missingPackageError};
        m_offlineUpdateCheckResult.set_value(result);
        return;
    }
    else
    {
        qDebug() << "ServerUpdateTool::at_extractFilesFinished() status = Ready";
    }

    QString manifestPath = m_outputDir.filePath(kPackageIndexFile);
    UpdateCheckResult result = readUpdateManifest(manifestPath);

    m_filesToUpload.clear();
    int missing = 0;

    // TODO: Check if files are stored in the subdirectory
    for (auto package: result.info.packages)
    {
        QFileInfo file(m_outputDir.filePath(package.file));
        if (file.exists())
            m_filesToUpload.push_back(package.file);
        else
            missing++;
    }

    if (!m_filesToUpload.empty())
    {
        // TODO:
        result.error = nx::update::InformationError::noError;
        m_offlineUpdaterState = OfflineUpdateState::Ready;
    }
    else
    {
        // Haven't found files for update
        m_offlineUpdaterState = OfflineUpdateState::Initial;
        result.error = nx::update::InformationError::missingPackageError;
    }
    m_offlineUpdateCheckResult.set_value(result);
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

void ServerUpdateTool::at_uploadStateChanged(QnUuid serverId, const UploadState& state)
{
#ifdef JUST_FOR_REFERENCE
    struct UploadState
    {
        enum Status
        {
            Initial,
            CalculatingMD5,
            CreatingUpload,
            Uploading,
            Checking,
            Done,
            Error,
            Canceled
        };
        /** Upload id, also serves as the filename on the server. */
        QString id;
        /** Upload size, in bytes. */
        qint64 size = 0;
        /** Total bytes uploaded. */
        qint64 uploaded = 0;
        /** Current status of the upload. */
        Status status = Initial;
        /** Error message, if any. */
        QString errorMessage;
    };
#endif
    if (!m_uploadState.count(serverId))
    {
        qDebug() << "ServerUpdateTool::at_uploadStateChanged() no upload state for server " << serverId.toString();
        return;
    }
    if (!m_activeServers.count(serverId))
    {
        qDebug() << "ServerUpdateTool::at_uploadStateChanged() there is no such active server " << serverId.toString();
        return;
    }

    auto& uploadSet = m_uploadState[serverId];
    auto server = m_activeServers[serverId];
    switch(state.status)
    {
        case UploadState::Done:
            qDebug() << "ServerUpdateTool::at_uploadStateChanged() uploaded file=" << state.id << "";
            uploadSet.active.erase(state.id);
            uploadNext(server, uploadSet);
            break;
        case UploadState::Uploading:
            qDebug() << "ServerUpdateTool::at_uploadStateChanged() uploading file=" << state.id << " bytes" << state.uploaded << " of" << state.size;
            break;
        case UploadState::Error:
            qDebug() << "ServerUpdateTool::at_uploadStateChanged() error with file=" << state.id << " error=" << state.errorMessage;
            break;
        default:
            break;
    }
}

void ServerUpdateTool::startUpload()
{
    qDebug() << "ServerUpdateTool::startUpload()";
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::Ready);
    QnMediaServerResourceList recipients = getServersForUpload();

    QStringList files = m_filesToUpload;

    m_uploadState.clear();
    for(auto server: recipients)
    {
        UploadSet state;
        state.files = m_filesToUpload;
        state.remaining = m_filesToUpload;
        m_uploadState[server->getId()] = state;
        /*
         * QString UploadManager::addUpload(
    const QnMediaServerResourcePtr& server,
    const QString& path,
    qint64 ttl,
    QString* errorMessage,
    QObject* context,
    Callback callback)
         */

    }

    for(auto server: recipients)
        uploadNext(server, m_uploadState[server->getId()]);
}

void ServerUpdateTool::stopUpload()
{
    NX_ASSERT(m_offlineUpdaterState == OfflineUpdateState::Push);
    // TODO: Implement it
    qDebug() << "ServerUpdateTool::stopUpload()";
}

void ServerUpdateTool::uploadNext(QnMediaServerResourcePtr server, UploadSet& state)
{
    if (!state.active.empty() || state.remaining.empty())
        return;

    QString path = state.remaining.front();

    qint64 ttl = -1;    //< This should mean 'infinite time'
    QString error;
    QnUuid serverId = server->getId();
    auto callback = [this, serverId](const UploadState& state)
    {
        at_uploadStateChanged(serverId, state);
    };
    auto id = m_uploadManager->addUpload(server, m_outputDir.filePath(path), ttl, &error, this, callback);

    if (!id.isEmpty())
    {
        state.active[id] = path;
        state.remaining.pop_front();
    }
    else
    {
        qDebug() << "ServerUpdateTool::uploadNext - failed to start uploading file="
                 << path
                 << "reason="
                 << error;
    }
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

    QString versionSuffix; //< TODO: #dklychkov what's that?

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
    QUrl url(path + versionSuffix);
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

using UpdateStatusAll = rest::RestResultWithData<std::vector<nx::update::Status>>;

bool ServerUpdateTool::getServersStatusChanges(RemoteStatus& status)
{
    if (m_remoteUpdateStatus.empty())
        return false;
    status = std::move(m_remoteUpdateStatus);
    return true;
}

void ServerUpdateTool::requestStopAction(QSet<QnUuid> targets)
{
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
        }
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
