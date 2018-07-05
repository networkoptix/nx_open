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
    base_type(parent)
{
}

ServerUpdateTool::~ServerUpdateTool()
{
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
