// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_system_endpoint.h"

#include <finders/systems_finder.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

std::optional<CloudSystemEndpoint> cloudSystemEndpoint(const QString& systemId)
{
    if (!NX_ASSERT(qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut))
        return std::nullopt;

    NX_DEBUG(NX_SCOPE_TAG, "Connecting to the cloud system %1", systemId);

    auto system = qnSystemsFinder->getSystem(systemId);
    if (!system || !system->isReachable())
    {
        NX_WARNING(NX_SCOPE_TAG, "System %1 is no longer accessible", systemId);
        return std::nullopt;
    }

    return cloudSystemEndpoint(system);
}

std::optional<CloudSystemEndpoint> cloudSystemEndpoint(const QnSystemDescriptionPtr& system)
{
    if (!NX_ASSERT(system) || !system->isReachable())
    {
        NX_WARNING(NX_SCOPE_TAG, "System %1 is no longer accessible", system->id());
        return std::nullopt;
    }

    CloudSystemEndpoint result;
    nx::utils::Url url;

    if (!ini().ignoreSavedPreferredCloudServers)
    {
        result.serverId = settings()->preferredCloudServer(system->id()).value_or(QnUuid());
        if (!result.serverId.isNull())
        {
            if (system->isReachableServer(result.serverId))
            {
                url = system->getServerHost(result.serverId);
                if (NX_ASSERT(!url.isEmpty()))
                {
                    NX_DEBUG(NX_SCOPE_TAG,
                        "Choosing stored preferred cloud server %1 (%2)",
                        result.serverId,
                        url);
                }
            }
            else
            {
                NX_DEBUG(NX_SCOPE_TAG,
                    "Stored preferred cloud server %1 is not reachable",
                    result.serverId);
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

        NX_DEBUG(NX_SCOPE_TAG, "Connection options:\n%1", debugServersInfo());

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
                if (!url.isEmpty())
                {
                    const bool isCloudUrl =
                        nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());

                    NX_DEBUG(NX_SCOPE_TAG,
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
        NX_WARNING(NX_SCOPE_TAG, "No connection to the system %1 is found", system->id());
        return std::nullopt;
    }

    // Here we can have two possible scenarios
    result.address = nx::network::SocketAddress(url.host(), url.port());
    return result;
}

} // namespace nx::vms::client::desktop
