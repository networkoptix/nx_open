#include "incompatible_server_watcher.h"

#include <client_core/connection_context_aware.h>
#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <common/common_module.h>
#include <core/resource/fake_media_server.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/connection_validator.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/module_information.h>

namespace {

bool isSuitable(const nx::vms::api::ModuleInformation& moduleInformation)
{
    return moduleInformation.version >= nx::utils::SoftwareVersion(2, 3, 0, 0);
}

} // namespace

class QnIncompatibleServerWatcherPrivate : public QObject, public QnConnectionContextAware
{
    Q_DECLARE_PUBLIC(QnIncompatibleServerWatcher)
    QnIncompatibleServerWatcher *q_ptr;

public:
    QnIncompatibleServerWatcherPrivate(QnIncompatibleServerWatcher* parent);

    void at_resourcePool_statusChanged(const QnResourcePtr& resource);
    void at_discoveredServerChanged(const nx::vms::api::DiscoveredServerData& serverData);

    void addResource(const nx::vms::api::DiscoveredServerData& serverData);
    void removeResource(const QnUuid &id);
    QnUuid getFakeId(const QnUuid &realId) const;

    QnMediaServerResourcePtr makeResource(
        const nx::vms::api::DiscoveredServerData& serverData) const;

public:
    struct DiscoveredServerItem
    {
        nx::vms::api::DiscoveredServerData serverData;
        bool keep = false;
        bool removed = false;

        DiscoveredServerItem() = default;
        DiscoveredServerItem(const nx::vms::api::DiscoveredServerData& serverData):
            serverData(serverData)
        {
        }
    };

    mutable QnMutex mutex;
    QHash<QnUuid, QnUuid> fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> serverUuidByFakeUuid;
    QHash<QnUuid, DiscoveredServerItem> discoveredServerItemById;
};

QnIncompatibleServerWatcher::QnIncompatibleServerWatcher(QObject *parent) :
    QObject(parent),
    d_ptr(new QnIncompatibleServerWatcherPrivate(this))
{
}

QnIncompatibleServerWatcher::~QnIncompatibleServerWatcher()
{
    stop();
}

void QnIncompatibleServerWatcher::start()
{
    Q_D(QnIncompatibleServerWatcher);

    connect(qnClientMessageProcessor, &QnCommonMessageProcessor::discoveredServerChanged,
            d, &QnIncompatibleServerWatcherPrivate::at_discoveredServerChanged);

    connect(resourcePool(), &QnResourcePool::statusChanged,
            d, &QnIncompatibleServerWatcherPrivate::at_resourcePool_statusChanged);
}

void QnIncompatibleServerWatcher::stop()
{
    Q_D(QnIncompatibleServerWatcher);

    if (!commonModule())
        return;

    if (auto messageProcessor = commonModule()->messageProcessor())
        messageProcessor->disconnect(this);

    resourcePool()->disconnect(this);

    QList<QnUuid> ids;

    {
        QnMutexLocker lock(&d->mutex);
        ids = d->fakeUuidByServerUuid.values();
        d->fakeUuidByServerUuid.clear();
        d->serverUuidByFakeUuid.clear();
        d->discoveredServerItemById.clear();
    }

    for (const QnUuid& id: ids)
    {
        if (auto server = resourcePool()->getIncompatibleServerById(id))
            resourcePool()->removeResource(server);
    }
}

void QnIncompatibleServerWatcher::keepServer(const QnUuid &id, bool keep)
{
    Q_D(QnIncompatibleServerWatcher);

    QnMutexLocker lock(&d->mutex);

    auto it = d->discoveredServerItemById.find(id);
    if (it == d->discoveredServerItemById.end())
        return;

    it->keep = keep;

    if (!keep && it->removed)
    {
        d->discoveredServerItemById.erase(it);

        lock.unlock();

        d->removeResource(d->getFakeId(id));
    }
}

void QnIncompatibleServerWatcher::createInitialServers(
    const nx::vms::api::DiscoveredServerDataList& discoveredServers)
{
    Q_D(QnIncompatibleServerWatcher);

    for (const auto& discoveredServer: discoveredServers)
        d->at_discoveredServerChanged(discoveredServer);
}


QnIncompatibleServerWatcherPrivate::QnIncompatibleServerWatcherPrivate(QnIncompatibleServerWatcher *parent) :
    QObject(parent),
    q_ptr(parent)
{
}

void QnIncompatibleServerWatcherPrivate::at_resourcePool_statusChanged(const QnResourcePtr &resource)
{
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    QnUuid id = server->getId();

    {
        QnMutexLocker lock(&mutex);
        if (serverUuidByFakeUuid.contains(id))
            return;
    }

    Qn::ResourceStatus status = server->getStatus();
    if (status != Qn::Offline
        && QnConnectionValidator::isCompatibleToCurrentSystem(server->getModuleInformation(),
            commonModule()))
    {
        removeResource(getFakeId(id));
    }
    else if (status == Qn::Offline)
    {
        QnMutexLocker lock(&mutex);
        auto it = discoveredServerItemById.find(id);
        if (it != discoveredServerItemById.end())
        {
            lock.unlock();

            if (it->serverData.status == nx::vms::api::ResourceStatus::incompatible
                || it->serverData.status == nx::vms::api::ResourceStatus::unauthorized)
            {
                addResource(it->serverData);
            }
        }
    }
}

void QnIncompatibleServerWatcherPrivate::at_discoveredServerChanged(
    const nx::vms::api::DiscoveredServerData& serverData)
{
    QnMutexLocker lock(&mutex);
    auto it = discoveredServerItemById.find(serverData.id);

    switch (serverData.status)
    {
        case nx::vms::api::ResourceStatus::online:
        case nx::vms::api::ResourceStatus::incompatible:
        case nx::vms::api::ResourceStatus::unauthorized:
        {
            if (it != discoveredServerItemById.end())
            {
                it->removed = false;
                it->serverData = serverData;
            }
            else
            {
                discoveredServerItemById.insert(serverData.id, serverData);
            }

            lock.unlock();

            const bool createResource =
                serverData.status == nx::vms::api::ResourceStatus::incompatible
                    || (serverData.status == nx::vms::api::ResourceStatus::unauthorized
                        && !resourcePool()->getResourceById<QnMediaServerResource>(serverData.id));

            if (createResource)
                addResource(serverData);
            else
                removeResource(getFakeId(serverData.id));

            break;
        }

        default:
        {
            if (it == discoveredServerItemById.end())
                break;

            it->removed = true;

            if (it->keep)
                break;

            discoveredServerItemById.remove(serverData.id);

            lock.unlock();

            removeResource(getFakeId(serverData.id));

            break;
        }
    }
}

void QnIncompatibleServerWatcherPrivate::addResource(
    const nx::vms::api::DiscoveredServerData& serverData)
{
    QnUuid id = getFakeId(serverData.id);

    if (id.isNull())
    {
        // add a resource
        if (!isSuitable(serverData))
            return;

        NX_ASSERT(serverData.status != nx::vms::api::ResourceStatus::offline,
            Q_FUNC_INFO, "Offline status is a mark to remove fake server.");
        QnMediaServerResourcePtr server = makeResource(serverData);
        {
            QnMutexLocker lock(&mutex);
            fakeUuidByServerUuid[serverData.id] = server->getId();
            serverUuidByFakeUuid[server->getId()] = serverData.id;
        }
        resourcePool()->addIncompatibleServer(server);

        NX_DEBUG(this, lit("Add incompatible server %1 at %2 [%3]")
            .arg(serverData.id.toString())
            .arg(serverData.systemName)
            .arg(QStringList(serverData.remoteAddresses.toList()).join(lit(", "))));
    }
    else
    {
        // update the resource
        auto server = resourcePool()->getIncompatibleServerById(id)
            .dynamicCast<QnFakeMediaServerResource>();

        NX_ASSERT(server, "There must be a resource in the resource pool.", Q_FUNC_INFO);

        if (!server)
            return;

        server->setFakeServerModuleInformation(serverData);

        NX_DEBUG(this, lit("Update incompatible server %1 at %2 [%3]")
            .arg(serverData.id.toString())
            .arg(serverData.systemName)
            .arg(QStringList(serverData.remoteAddresses.toList()).join(lit(", "))));
    }
}

void QnIncompatibleServerWatcherPrivate::removeResource(const QnUuid &id)
{
    if (id.isNull())
        return;

    QnUuid serverId;
    {
        QnMutexLocker lock(&mutex);
        serverId = serverUuidByFakeUuid.take(id);
        if (serverId.isNull())
            return;

        fakeUuidByServerUuid.remove(serverId);
    }


    if (auto server = resourcePool()->getIncompatibleServerById(id))
    {
        NX_DEBUG(this, lit("Remove incompatible server %1 at %2")
            .arg(serverId.toString())
            .arg(server->getModuleInformation().systemName));

        resourcePool()->removeResource(server);
    }
}

QnUuid QnIncompatibleServerWatcherPrivate::getFakeId(const QnUuid &realId) const
{
    QnMutexLocker lock(&mutex);
    return fakeUuidByServerUuid.value(realId);
}

QnMediaServerResourcePtr QnIncompatibleServerWatcherPrivate::makeResource(
    const nx::vms::api::DiscoveredServerData& serverData) const
{
    QnFakeMediaServerResourcePtr server(new QnFakeMediaServerResource(commonModule()));
    server->setFakeServerModuleInformation(serverData);
    return server;
}
