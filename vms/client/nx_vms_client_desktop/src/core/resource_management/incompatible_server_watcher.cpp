// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incompatible_server_watcher.h"

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

namespace {

bool isSuitable(const nx::vms::api::ModuleInformation& moduleInformation)
{
    return moduleInformation.version >= nx::utils::SoftwareVersion(2, 3, 0, 0);
}

} // namespace

class QnIncompatibleServerWatcherPrivate : public QObject, public nx::vms::client::core::CommonModuleAware
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

    void createInitialServers(const nx::vms::api::DiscoveredServerDataList& discoveredServers);

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

    mutable nx::Mutex mutex;
    QHash<QnUuid, QnUuid> fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> serverUuidByFakeUuid;
    QHash<QnUuid, DiscoveredServerItem> discoveredServerItemById;
    nx::utils::ScopedConnections connections;
};

QnIncompatibleServerWatcher::QnIncompatibleServerWatcher(QObject *parent) :
    QObject(parent),
    d_ptr(new QnIncompatibleServerWatcherPrivate(this))
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionOpened, this,
        &QnIncompatibleServerWatcher::start);

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::connectionClosed, this,
        &QnIncompatibleServerWatcher::stop);
}

QnIncompatibleServerWatcher::~QnIncompatibleServerWatcher()
{
    stop();
}

void QnIncompatibleServerWatcher::start()
{
    Q_D(QnIncompatibleServerWatcher);

    d->connections << connect(
        qnClientMessageProcessor,
        &QnClientMessageProcessor::gotInitialDiscoveredServers,
        d,
        &QnIncompatibleServerWatcherPrivate::createInitialServers);

    d->connections << connect(
        qnClientMessageProcessor,
        &QnCommonMessageProcessor::discoveredServerChanged,
        d,
        &QnIncompatibleServerWatcherPrivate::at_discoveredServerChanged);

    d->connections << connect(
        resourcePool(),
        &QnResourcePool::statusChanged,
        d,
        &QnIncompatibleServerWatcherPrivate::at_resourcePool_statusChanged);
}

void QnIncompatibleServerWatcher::stop()
{
    Q_D(QnIncompatibleServerWatcher);
    d->connections.reset();

    if (!commonModule())
        return;

    QList<QnUuid> ids;

    {
        NX_MUTEX_LOCKER lock(&d->mutex);
        ids = d->fakeUuidByServerUuid.values();
        d->fakeUuidByServerUuid.clear();
        d->serverUuidByFakeUuid.clear();
        d->discoveredServerItemById.clear();
    }

    QnResourceList servers;
    for (const QnUuid& id: ids)
    {
        if (auto server = resourcePool()->getIncompatibleServerById(id))
            servers.push_back(server);
    }
    NX_DEBUG(this, "Stop watching");
    resourcePool()->removeResources(servers);
}

void QnIncompatibleServerWatcher::keepServer(const QnUuid &id, bool keep)
{
    Q_D(QnIncompatibleServerWatcher);

    NX_MUTEX_LOCKER lock(&d->mutex);

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
        NX_MUTEX_LOCKER lock(&mutex);
        if (serverUuidByFakeUuid.contains(id))
            return;
    }

    nx::vms::api::ResourceStatus status = server->getStatus();
    auto moduleInformation = server->getModuleInformation();

    if (status != nx::vms::api::ResourceStatus::offline
        && helpers::serverBelongsToCurrentSystem(moduleInformation, commonModule())
        && nx::vms::common::ServerCompatibilityValidator::isCompatible(moduleInformation))
    {
        removeResource(getFakeId(id));
    }
    else if (status == nx::vms::api::ResourceStatus::offline)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        auto it = discoveredServerItemById.find(id);
        if (it != discoveredServerItemById.end())
        {
            lock.unlock();

            // TODO: should we add Server with mismatchedCertificate status here?
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
    NX_MUTEX_LOCKER lock(&mutex);
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

            bool createResource = false;
            if (serverData.status == nx::vms::api::ResourceStatus::incompatible)
            {
                createResource = true;
            }
            else if (serverData.status == nx::vms::api::ResourceStatus::unauthorized)
            {
                const auto serverResource =
                    resourcePool()->getResourceById<QnMediaServerResource>(serverData.id);
                createResource = !serverResource || !serverResource->isOnline();
            }

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
    // Setting '!compatible' flag for original server.
    if (auto server = resourcePool()->getResourceById<QnMediaServerResource>(serverData.id))
        server->setCompatible(false);

    QnUuid id = getFakeId(serverData.id);

    if (id.isNull())
    {
        // add a resource
        if (!isSuitable(serverData))
            return;

        NX_ASSERT(serverData.status != nx::vms::api::ResourceStatus::offline,
            "Offline status is a mark to remove fake server.");
        QnMediaServerResourcePtr server = makeResource(serverData);
        {
            NX_MUTEX_LOCKER lock(&mutex);
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

        NX_ASSERT(server, "There must be a resource in the resource pool.");

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

    // Removing incompatible flag from original resource.
    if (auto server = resourcePool()->getResourceById<QnMediaServerResource>(id))
        server->setCompatible(true);

    QnUuid serverId;
    {
        NX_MUTEX_LOCKER lock(&mutex);
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
    NX_MUTEX_LOCKER lock(&mutex);
    return fakeUuidByServerUuid.value(realId);
}

QnMediaServerResourcePtr QnIncompatibleServerWatcherPrivate::makeResource(
    const nx::vms::api::DiscoveredServerData& serverData) const
{
    QnFakeMediaServerResourcePtr server(new QnFakeMediaServerResource(commonModule()));
    server->setFakeServerModuleInformation(serverData);
    return server;
}

void QnIncompatibleServerWatcherPrivate::createInitialServers(
    const nx::vms::api::DiscoveredServerDataList& discoveredServers)
{
    for (const auto& discoveredServer: discoveredServers)
        at_discoveredServerChanged(discoveredServer);
}
