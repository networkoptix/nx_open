#include "system_servers_watcher.h"

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/log.h>
#include <api/global_settings.h>

namespace {

bool sameServer(const QnMediaServerResourcePtr& first, const QnMediaServerResourcePtr& second)
{
    return first && second && first->getId() == second->getId();
}

using SameServerPredicate = std::function<bool (const QnMediaServerResourcePtr&)>;
SameServerPredicate makeSameServerPredicate(const QnMediaServerResourcePtr& server)
{
    return
        [server](const QnMediaServerResourcePtr& other)
        {
            return sameServer(server, other);
        };
}

bool contains(const QnMediaServerResourceList& servers, const QnMediaServerResourcePtr& server)
{
    return std::any_of(servers.begin(), servers.end(), makeSameServerPredicate(server));
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

SystemServersWatcher::SystemServersWatcher(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent)
{
    const auto pool = commonModule()->resourcePool();
    connect(pool, &QnResourcePool::resourceAdded, this, &SystemServersWatcher::tryAddServer);
    connect(pool, &QnResourcePool::resourceRemoved, this, &SystemServersWatcher::tryRemoveServer);

    connect(commonModule()->globalSettings(), &QnGlobalSettings::localSystemIdChanged,
        this, &SystemServersWatcher::updateLocalSystemId);

    updateLocalSystemId();
}

QnMediaServerResourceList SystemServersWatcher::servers() const
{
    return m_servers;
}

int SystemServersWatcher::serversCount() const
{
    return m_servers.size();
}

void SystemServersWatcher::tryAddServer(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    qDebug() << "--------------------";

    if (server->getModuleInformation().localSystemId != m_localSystemId)
        return;

    if (contains(m_servers, server))
    {
        NX_EXPECT(false, "Server already in the list");
        return;
    }

    m_servers.append(server);

    emit serverAdded(server);
    emit serversCountChanged();
}

void SystemServersWatcher::tryRemoveServer(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    const auto it = std::find_if(m_servers.begin(), m_servers.end(),
        makeSameServerPredicate(server));

    if (it == m_servers.end())
        return;

    m_servers.erase(it);

    emit serverRemoved(server->getId());
    emit serversCountChanged();
}

void SystemServersWatcher::cleanServers()
{
    if (m_servers.isEmpty())
        return;

    while(!m_servers.isEmpty())
    {
        const auto server = m_servers.last();
        m_servers.pop_back();
        emit serverRemoved(server->getId());
    }

    emit serversCountChanged();
}

void SystemServersWatcher::updateLocalSystemId()
{
    const auto currentSystemLocalId = commonModule()->remoteGUID().isNull()
        ? QnUuid()
        : commonModule()->globalSettings()->localSystemId();

    if (m_localSystemId == currentSystemLocalId)
        return;

    cleanServers();
    m_localSystemId = currentSystemLocalId;

    for (const auto server: resourcePool()->getAllServers(Qn::AnyStatus))
        tryAddServer(server);
}

} // namespace desktop
} // namespace client
} // namespace nx
