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
#include <utils/update/update_utils.h>
#include <update/task/check_update_peer_task.h>
#include <update/low_free_space_warning.h>

#include <client/client_settings.h>
#include <client/desktop_client_message_processor.h>

#include <utils/common/app_info.h>
#include <core/resource/fake_media_server.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

#include <api/server_rest_connection.h>

// There is a lot of obsolete code here. We use new update manager now, it automates all update routines.
namespace
{
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

} // anonymous namespace

namespace nx {
namespace client {
namespace desktop {

ServerUpdateTool::ServerUpdateTool(QObject* parent):
    base_type(parent),
    m_stage(QnFullUpdateStage::Init)
{
}

ServerUpdateTool::~ServerUpdateTool()
{
    if (m_updateProcess)
    {
        m_updateProcess->stop();
        delete m_updateProcess;
    }
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

void ServerUpdateTool::at_resourceAdded(const QnResourcePtr &resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        m_activeServers[server->getId()] = server;
}

void ServerUpdateTool::at_resourceRemoved(const QnResourcePtr &resource)
{
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        m_activeServers.erase(server->getId());
}

void ServerUpdateTool::at_resourceChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;
}

QnFullUpdateStage ServerUpdateTool::stage() const
{
    return m_stage;
}

bool ServerUpdateTool::isUpdating() const
{
    return m_updateProcess;
}

bool ServerUpdateTool::idle() const
{
    return m_stage == QnFullUpdateStage::Init;
}

void ServerUpdateTool::finishUpdate(const QnUpdateResult &result)
{
    m_updateProcess->deleteLater();
    m_updateProcess = nullptr;

    /* We must emit signal after m_updateProcess clean, so ::isUpdating() will return false. */
    emit updateFinished(result);
}

/**
 * Generates URL for specified version
 * TODO: It seems to be obsolete since we use new update manager
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

    QUrl url(QnAppInfo::updateGeneratorUrl() + versionSuffix);
    url.setQuery(query);

    return url;
}

#ifdef FUCK_THIS
void ServerUpdateTool::startUpdate(const QnSoftwareVersion &version /* = QnSoftwareVersion()*/) {
    QnUpdateTarget target(actualTargetIds(), version, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    startUpdate(target);
}

void ServerUpdateTool::startUpdate(const QString &fileName) {
    QnUpdateTarget target(actualTargetIds(), fileName, !m_enableClientUpdates || qnSettings->isClientUpdateDisabled());
    startUpdate(target);
}
#endif

void ServerUpdateTool::startOnlineClientUpdate(const QSet<QnUuid>& targets, const nx::utils::SoftwareVersion& version, bool enableClientUpdates)
{
    // !enableClientUpdates || qnSettings->isClientUpdateDisabled()
    QnUpdateTarget target(targets, version, enableClientUpdates);
    startUpdate(target);
}

bool ServerUpdateTool::canCancelUpdate() const
{
    if (!m_updateProcess)
        return true;

    if (m_stage == QnFullUpdateStage::Servers)
        return false;

    return true;
}

bool ServerUpdateTool::cancelUpdate()
{
    if (!m_updateProcess)
        return true;

    if (m_stage == QnFullUpdateStage::Servers)
        return false;

    m_updateProcess->stop();

    return true;
}

void ServerUpdateTool::startUpdate(const QnUpdateTarget& target)
{
    if (m_updateProcess)
        return;

    QSet<QnUuid> incompatibleTargets;
    bool clearTargetsWhenFinished = false;

    /*
    if (m_targets.isEmpty())
    {
        clearTargetsWhenFinished = true;
        setTargets(target.targets, defaultEnableClientUpdates);

        for (const auto& id: target.targets)
        {
            const auto server = resourcePool()->getIncompatibleServerById(id)
                .dynamicCast<QnFakeMediaServerResource>();
            if (!server)
                continue;

            const auto realId = server->getOriginalGuid();
            if (realId.isNull())
                continue;

            incompatibleTargets.insert(realId);
            qnDesktopClientMessageProcessor->incompatibleServerWatcher()->keepServer(realId, true);
        }
    }*/

    m_updateProcess = new QnUpdateProcess(target);

    connect(m_updateProcess, &QnUpdateProcess::stageChanged, this,
        [this](QnFullUpdateStage stage)
        {
            if (stage == m_legacyStatus.stage)
                return;
            m_legacyStatus.stage = stage;
            m_legacyStatusChanged = true;
        });

    connect(m_updateProcess, &QnUpdateProcess::progressChanged,
        this, [this](int progress)
        {
            if (progress != m_legacyStatus.progress)
            {
                m_legacyStatus.progress = progress;
                m_legacyStatusChanged = true;
            }
        });

    connect(m_updateProcess, &QnUpdateProcess::peerStageChanged,
        this, [this](const QnUuid &peerId, QnPeerUpdateStage stage)
        {
            // TODO: We could do a mapping from old update stage to a new one
            if (m_legacyStatus.peerStage[peerId] != stage)
            {
                m_legacyStatus.peerStage[peerId] = stage;
                m_legacyStatus.peerProgress[peerId] = 0;
                m_legacyStatusChanged = true;
            }
        });

    // This event is quite redundant
    connect(m_updateProcess, &QnUpdateProcess::peerStageProgressChanged,
        this, [this](const QnUuid &peerId, QnPeerUpdateStage stage, int progress)
        {
            if (m_legacyStatus.peerStage[peerId] != stage
                || m_legacyStatus.peerProgress[peerId] != progress)
            {
                m_legacyStatus.peerStage[peerId] = stage;
                m_legacyStatus.peerProgress[peerId] = progress;
                m_legacyStatusChanged = true;
            }
        });

    connect(m_updateProcess, &QnUpdateProcess::targetsChanged,
        this, [this](const QSet<QnUuid> &targets)
        {
            // When can it happen?
        });

    // This seems to be very serious
    connect(m_updateProcess, &QnUpdateProcess::lowFreeSpaceWarning,
        this, &ServerUpdateTool::lowFreeSpaceWarning, Qt::BlockingQueuedConnection);

    connect(m_updateProcess, &QnUpdateProcess::updateFinished, this,
        [this, incompatibleTargets, clearTargetsWhenFinished](const QnUpdateResult& result)
        {
            finishUpdate(result);

            const auto watcher = qnDesktopClientMessageProcessor->incompatibleServerWatcher();
            for (const auto& id : incompatibleTargets)
                watcher->keepServer(id, false);

            //if (clearTargetsWhenFinished)
            //    setTargets(QSet<QnUuid>(), defaultEnableClientUpdates);
        });

    m_updateProcess->start();
}

bool ServerUpdateTool::hasRemoteChanges() const
{
    bool result = false;
    if (!m_remoteUpdateStatus.empty())
        result = true;
    // ...
    return result;
}

bool ServerUpdateTool::getServersStatusChanges(UpdateStatus& status)
{
    if (m_remoteUpdateStatus.empty())
        return false;
    status = std::move(m_remoteUpdateStatus);
    return true;
}

bool ServerUpdateTool::getLegacyUpdateStatusChanges(LegacyUpdateStatus& status)
{
    if (!m_legacyStatusChanged)
        return false;
    status = m_legacyStatus;
    m_legacyStatusChanged = false;
    return true;
}

void ServerUpdateTool::requestUpdateActionAll(nx::api::Updates2ActionData::ActionCode action)
{
    auto callback = [this](bool success, rest::Handle handle, rest::ServerConnection::UpdateStatus response)
        {
            at_updateStatusResponse(success, handle, response.data);
        };
    nx::api::Updates2ActionData request;
    request.action = action;

    for (auto it : m_activeServers)
    {
        if (auto connection = getServerConnection(it.second))
        {

            connection->sendUpdateCommand(request, callback);
        }
    }
}

void ServerUpdateTool::requestUpdateAction(nx::api::Updates2ActionData::ActionCode action,
    QSet<QnUuid> targets,
    nx::vms::api::SoftwareVersion version)
{
    auto callback = [this](bool success, rest::Handle handle, rest::ServerConnection::UpdateStatus response)
        {
            at_updateStatusResponse(success, handle, response.data);
        };
    for (auto target : targets)
    {
        auto it = m_activeServers.find(target);
        if (it == m_activeServers.end())
            continue;

        auto connection = getServerConnection(it->second);
        if (!connection)
            continue;

        nx::api::Updates2ActionData request;
        request.targetVersion = version;
        request.action = action;

        connection->sendUpdateCommand(request, callback);
    }
}

void ServerUpdateTool::at_updateStatusResponse(bool success, rest::Handle handle,
    const std::vector<nx::api::Updates2StatusData>& response)
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
    const nx::api::Updates2StatusData& response)
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
            auto callback = [this](bool success, rest::Handle handle, rest::ServerConnection::UpdateStatusAll response)
                {
                    at_updateStatusResponse(success, handle, response.data);
                };

            m_checkingRemoteUpdateStatus = true;
            connection->getUpdateStatusAll(callback);
        }
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
