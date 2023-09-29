// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "incompatible_server_watcher.h"

#include <client/client_message_processor.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

using namespace nx::vms::client::desktop;

namespace {

bool isSuitable(const nx::vms::api::ModuleInformation& moduleInformation)
{
    return moduleInformation.version >= nx::utils::SoftwareVersion(2, 3, 0, 0);
}

} // namespace

struct QnIncompatibleServerWatcher::Private
{
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

    QnIncompatibleServerWatcher* const q;
    mutable nx::Mutex mutex;
    QHash<QnUuid, QnUuid> fakeUuidByServerUuid;
    QHash<QnUuid, QnUuid> serverUuidByFakeUuid;
    QHash<QnUuid, DiscoveredServerItem> discoveredServerItemById;
    nx::utils::ScopedConnections connections;

    void at_resourcePool_statusChanged(const QnResourcePtr& resource)
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
            && helpers::serverBelongsToCurrentSystem(moduleInformation, q->systemContext())
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

    void at_discoveredServerChanged(const nx::vms::api::DiscoveredServerData& serverData)
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
                        q->resourcePool()->getResourceById<QnMediaServerResource>(serverData.id);
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

    void addResource(
        const nx::vms::api::DiscoveredServerData& serverData)
    {
        auto server = q->resourcePool()->getResourceById<ServerResource>(serverData.id);

        // Updating compatibility and merge statuses for the original server.
        if (server)
        {
            server->setCompatible(false);
            server->setDetached(serverData.status == nx::vms::api::ResourceStatus::unauthorized);
        }

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
            q->resourcePool()->addIncompatibleServer(server);

            NX_DEBUG(this, lit("Add incompatible server %1 at %2 [%3]")
                .arg(serverData.id.toString())
                .arg(serverData.systemName)
                .arg(QStringList(serverData.remoteAddresses.values()).join(", ")));
        }
        else
        {
            // update the resource
            auto fakeServer = q->resourcePool()->getIncompatibleServerById(id)
                .dynamicCast<QnFakeMediaServerResource>();

            NX_ASSERT(fakeServer, "There must be a resource in the resource pool.");

            if (!fakeServer)
                return;

            fakeServer->setFakeServerModuleInformation(serverData);
            if (server)
                fakeServer->setOsInfo(server->getOsInfo());

            NX_DEBUG(this, lit("Update incompatible server %1 at %2 [%3]")
                .arg(serverData.id.toString())
                .arg(serverData.systemName)
                .arg(QStringList(serverData.remoteAddresses.values()).join(", ")));
        }
    }

    void removeResource(const QnUuid &id)
    {
        if (id.isNull())
            return;

        // Updating compatibility and merge statuses for the original server.
        if (auto server = q->resourcePool()->getResourceById<ServerResource>(id))
        {
            server->setCompatible(true);
            server->setDetached(false);
        }

        QnUuid serverId;
        {
            NX_MUTEX_LOCKER lock(&mutex);
            serverId = serverUuidByFakeUuid.take(id);
            if (serverId.isNull())
                return;

            fakeUuidByServerUuid.remove(serverId);
        }

        if (auto server = q->resourcePool()->getIncompatibleServerById(id))
        {
            NX_DEBUG(this, lit("Remove incompatible server %1 at %2")
                .arg(serverId.toString())
                .arg(server->getModuleInformation().systemName));

            q->resourcePool()->removeResource(server);
        }
    }

    QnUuid getFakeId(const QnUuid &realId) const
    {
        NX_MUTEX_LOCKER lock(&mutex);
        return fakeUuidByServerUuid.value(realId);
    }

    QnMediaServerResourcePtr makeResource(
        const nx::vms::api::DiscoveredServerData& serverData) const
    {
        QnFakeMediaServerResourcePtr fakeServer(new QnFakeMediaServerResource());
        fakeServer->setFakeServerModuleInformation(serverData);
        if (auto server = q->resourcePool()->getResourceById<QnMediaServerResource>(serverData.id))
            fakeServer->setOsInfo(server->getOsInfo());
        return fakeServer;
    }

    void createInitialServers(
        const nx::vms::api::DiscoveredServerDataList& discoveredServers)
    {
        for (const auto& discoveredServer: discoveredServers)
            at_discoveredServerChanged(discoveredServer);
    }
};

QnIncompatibleServerWatcher::QnIncompatibleServerWatcher(
    nx::vms::common::SystemContext* systemContext,
    QObject *parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q = this})
{
}

QnIncompatibleServerWatcher::~QnIncompatibleServerWatcher()
{
    stop();
}

void QnIncompatibleServerWatcher::setMessageProcessor(QnClientMessageProcessor* messageProcessor)
{
    if (messageProcessor)
    {
        connect(messageProcessor, &QnClientMessageProcessor::connectionOpened, this,
            &QnIncompatibleServerWatcher::start);

        connect(messageProcessor, &QnClientMessageProcessor::connectionClosed, this,
            &QnIncompatibleServerWatcher::stop);
    }
    else
    {
        stop();
    }
}

void QnIncompatibleServerWatcher::start()
{
    d->connections << connect(
        clientMessageProcessor(),
        &QnClientMessageProcessor::gotInitialDiscoveredServers,
        this,
        [this](const nx::vms::api::DiscoveredServerDataList& discoveredServers)
        {
            d->createInitialServers(discoveredServers);
        });

    d->connections << connect(
        clientMessageProcessor(),
        &QnCommonMessageProcessor::discoveredServerChanged,
        this,
        [this](const nx::vms::api::DiscoveredServerData& serverData)
        {
            d->at_discoveredServerChanged(serverData);
        });

    d->connections << connect(
        resourcePool(),
        &QnResourcePool::statusChanged,
        this,
        [this](const QnResourcePtr& resource)
        {
            d->at_resourcePool_statusChanged(resource);
        });
}

void QnIncompatibleServerWatcher::stop()
{
    d->connections.reset();

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

    if (!servers.empty())
        resourcePool()->removeResources(servers);
}

void QnIncompatibleServerWatcher::keepServer(const QnUuid &id, bool keep)
{
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
