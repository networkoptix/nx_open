// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "other_servers_manager.h"

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/resource/server.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx_ec/data/api_conversion_functions.h>

namespace {

bool isSuitable(const nx::vms::api::ModuleInformation& moduleInformation)
{
    return moduleInformation.version >= nx::utils::SoftwareVersion(2, 3, 0, 0);
}

} // namespace

namespace nx::vms::client::desktop {

struct OtherServersManager::Private
{
    struct DiscoveredServerItem
    {
        nx::vms::api::DiscoveredServerData serverData;
        bool removed = false;

        DiscoveredServerItem() = default;
        DiscoveredServerItem(const nx::vms::api::DiscoveredServerData& serverData):
            serverData(serverData)
        {
        }
    };

    OtherServersManager* const q;
    QHash<QnUuid, DiscoveredServerItem> discoveredServerItemById;
    nx::utils::ScopedConnections connections;

    QSet<QnUuid> otherServers;

    void at_resourcePool_statusChanged(const QnResourcePtr& resource)
    {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            return;

        {
            if (otherServers.contains(server->getId()))
                return;
        }

        nx::vms::api::ResourceStatus status = server->getStatus();
        auto moduleInformation = server->getModuleInformation();

        if (status != nx::vms::api::ResourceStatus::offline
            && helpers::serverBelongsToCurrentSystem(moduleInformation, q->systemContext())
            && nx::vms::common::ServerCompatibilityValidator::isCompatible(moduleInformation))
        {
            removeOtherServer(server->getId());
        }
        else if (status == nx::vms::api::ResourceStatus::offline)
        {
            auto it = discoveredServerItemById.find(server->getId());
            if (it != discoveredServerItemById.end())
            {
                // TODO: should we add Server with mismatchedCertificate status here?
                if (it->serverData.status == nx::vms::api::ResourceStatus::incompatible
                    || it->serverData.status == nx::vms::api::ResourceStatus::unauthorized)
                {
                    addOtherServer(it->serverData);
                }
            }
        }
    }

    void at_discoveredServerChanged(const nx::vms::api::DiscoveredServerData& serverData)
    {
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
                    addOtherServer(serverData);
                else
                    removeOtherServer(serverData.id);

                break;
            }

            default:
            {
                if (it == discoveredServerItemById.end())
                    break;

                it->removed = true;

                discoveredServerItemById.remove(serverData.id);

                removeOtherServer(serverData.id);

                break;
            }
        }
    }

    void addOtherServer(const nx::vms::api::DiscoveredServerData& serverData)
    {
        auto server = q->resourcePool()->getResourceById<ServerResource>(serverData.id);

        // Updating compatibility and merge statuses for the original server.
        if (server)
        {
            server->setCompatible(false);
            server->setDetached(serverData.status == nx::vms::api::ResourceStatus::unauthorized);
        }

        if (auto iter = otherServers.find(serverData.id); iter == otherServers.end())
        {
            if (!isSuitable(serverData))
                return;

            NX_ASSERT(serverData.status != nx::vms::api::ResourceStatus::offline,
                "Offline status is a mark to remove fake server.");

            otherServers.insert(serverData.id);

            NX_DEBUG(this, lit("Add incompatible server %1 at %2 [%3]")
                .arg(serverData.id.toString())
                .arg(serverData.systemName)
                .arg(QStringList(serverData.remoteAddresses.values()).join(", ")));
            emit q->serverAdded(serverData.id);
        }
        else
        {
            emit q->serverUpdated(serverData.id);
        }
    }

    void removeOtherServer(const QnUuid& serverId)
    {
        if (!NX_ASSERT(!serverId.isNull()))
            return;

        // Updating compatibility and merge statuses for the original server.
        if (auto server = q->resourcePool()->getResourceById<ServerResource>(serverId))
        {
            server->setCompatible(true);
            server->setDetached(false);
        }

        if (auto iter = otherServers.find(serverId); iter != otherServers.end())
        {
            otherServers.erase(iter);
            emit q->serverRemoved(serverId);
        }
    }

    void createInitialServers(
        const nx::vms::api::DiscoveredServerDataList& discoveredServers)
    {
        for (const auto& discoveredServer: discoveredServers)
            at_discoveredServerChanged(discoveredServer);
    }

    nx::network::SocketAddress getPrimaryAddress(const QnUuid& serverId) const
    {
        if (!NX_ASSERT(otherServers.contains(serverId)))
            return {};

        const auto iter = discoveredServerItemById.find(serverId);
        if (!NX_ASSERT(iter != discoveredServerItemById.end()))
            return {};

        QList<nx::network::SocketAddress> addressList =
            ec2::moduleInformationEndpoints(iter->serverData);

        if (addressList.isEmpty())
            return nx::network::SocketAddress();

        return addressList.first();
    }
};

OtherServersManager::OtherServersManager(
    nx::vms::common::SystemContext* systemContext,
    QObject *parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q = this})
{
}

OtherServersManager::~OtherServersManager()
{
    stop();
}

void OtherServersManager::setMessageProcessor(QnClientMessageProcessor* messageProcessor)
{
    if (messageProcessor)
    {
        connect(messageProcessor, &QnClientMessageProcessor::connectionOpened, this,
            &OtherServersManager::start);

        connect(messageProcessor, &QnClientMessageProcessor::connectionClosed, this,
            &OtherServersManager::stop);
    }
    else
    {
        stop();
    }
}

void OtherServersManager::start()
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

void OtherServersManager::stop()
{
    d->connections.reset();

    d->otherServers.clear();
}

QnUuidList OtherServersManager::getServers() const
{
    return d->otherServers.values();
}

bool OtherServersManager::containsServer(const QnUuid& serverId) const
{
    return d->otherServers.contains(serverId);
}

nx::vms::api::ModuleInformationWithAddresses OtherServersManager::getModuleInformationWithAddresses(
    const QnUuid& serverId) const
{
    if (!NX_ASSERT(d->otherServers.contains(serverId)))
        return {};

    const auto iter = d->discoveredServerItemById.find(serverId);
    if (NX_ASSERT(iter != d->discoveredServerItemById.end()))
        return iter->serverData;

    return {};
}

nx::utils::Url OtherServersManager::getUrl(const QnUuid& serverId) const
{
    return nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setEndpoint(d->getPrimaryAddress(serverId)).toUrl();
}

} // namespace nx::vms::client::desktop
