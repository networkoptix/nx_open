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
}*/

ServerUpdateTool::OfflineUpdateState ServerUpdateTool::iterateOfflineUpdater()
{
    switch(m_offlineUpdaterState)
    {
        case OfflineUpdateState::Initial:
            break;
        case OfflineUpdateState::Check:
            break;
        case OfflineUpdateState::Unpacking:
            break;
        case OfflineUpdateState::Push:
            break;
        case OfflineUpdateState::Done:
            break;
    }
    return m_offlineUpdaterState;
}

ServerUpdateTool::UpdateCheckResult ServerUpdateTool::checkUpdateFromInternet(QString updateUrl)
{
    UpdateCheckResult result;
    result.info = nx::update::updateInformation(updateUrl, nx::update::kLatestVersion, &result.error);
    return result;
}

std::future<ServerUpdateTool::UpdateCheckResult> ServerUpdateTool::checkUpdateFromFile(QString file)
{
    qDebug() << "ServerUpdateTool::checkUpdateFromFile(" << file << ")";

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
    ServerUpdateTool::UpdateCheckResult result;
    result.error = nx::update::InformationError::jsonError;
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

// NOTE: We are probably not in the UI thread
void ServerUpdateTool::at_extractFilesFinished(int code)
{
    if (code != QnZipExtractor::Ok)
        qDebug() << "ServerUpdateTool::at_extractFilesFinished() err=" << QnZipExtractor::errorToString((QnZipExtractor::Error)code);

    QString manifestPath = m_outputDir.filePath(kPackageIndexFile);
    UpdateCheckResult result = readUpdateManifest(manifestPath);
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
    m_onUpdatedResource = connect(pool, &QnResourcePool::resourceChanged,
        this, &ServerUpdateTool::at_resourceChanged);
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
