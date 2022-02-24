// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_endpoint.h"

#include <finders/systems_finder.h>
#include <helpers/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/ini.h>
#include <watchers/cloud_status_watcher.h>

namespace nx::vms::client::core {

std::optional<CloudSystemEndpoint> cloudSystemEndpoint(const QString& systemId)
{
    if (!NX_ASSERT(qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut))
        return std::nullopt;

    NX_DEBUG(typeid(cloudSystemEndpoint), "Connecting to the cloud system %1", systemId);

    auto system = qnSystemsFinder->getSystem(systemId);
    if (!system || !system->isReachable())
    {
        NX_WARNING(typeid(cloudSystemEndpoint), "System %1 is no longer accessible", systemId);
        return std::nullopt;
    }

    CloudSystemEndpoint result;
    nx::utils::Url url;

    if (!ini().ignoreSavedPreferredCloudServers)
    {
        result.serverId =
            nx::vms::client::core::helpers::preferredCloudServer(systemId).value_or(QnUuid());
        if (!result.serverId.isNull())
        {
            if (system->isReachableServer(result.serverId))
            {
                url = system->getServerHost(result.serverId);
                if (NX_ASSERT(!url.isEmpty()))
                {
                    NX_DEBUG(typeid(cloudSystemEndpoint),
                        "Choosing stored preferred cloud server %1 (%2)",
                        result.serverId.toString(),
                        url);
                }
            }
            else
            {
                NX_DEBUG(typeid(cloudSystemEndpoint),
                    "Stored preferred cloud server %1 is not reachable",
                    result.serverId.toString());
            }
        }
    }

    if (url.isEmpty())
    {
        const auto servers = system->servers();

        const auto debugServersInfo =
            [&servers, system]() -> QString
            {
                QStringList result;
                for (const auto& server: servers)
                {
                    result.push_back(QStringLiteral("   %1 (%2)").arg(
                        server.id.toString(),
                        system->getServerHost(server.id).toString()));
                }
                return result.join("\n");
            };

        NX_DEBUG(typeid(cloudSystemEndpoint), "Connection options:\n%1", debugServersInfo());

        const auto iter = std::find_if(servers.cbegin(), servers.cend(),
            [system](const nx::vms::api::ModuleInformation& server)
            {
                return system->isReachableServer(server.id);
            });

        if (iter != servers.cend())
        {
            if (NX_ASSERT(!iter->id.isNull()))
            {
                result.serverId = iter->id;
                url = system->getServerHost(iter->id);
                if (NX_ASSERT(!url.isEmpty()))
                {
                    const bool isCloudUrl =
                        nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());

                    NX_DEBUG(typeid(cloudSystemEndpoint),
                        "Choosing %1 connection to the server %2 (%3)",
                        isCloudUrl ? "cloud" : "local",
                        result.serverId,
                        url);
                }
            }
        }
    }

    if (url.isEmpty())
    {
        NX_WARNING(typeid(cloudSystemEndpoint),
            "No connection to the system %1 is found", systemId);
        return std::nullopt;
    }

    // Here we can have two possible scenarios
    result.address = nx::network::SocketAddress(url.host(), url.port());
    return result;
}

} // namespace nx::vms::client::desktop
