// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logon_data_helpers.h"

#include <network/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_finder/system_finder.h>

namespace nx::vms::client::core {

namespace {

bool isCloudUrl(const nx::Url& url)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
}

} // namespace

std::optional<LogonData> cloudLogonData(const QString& systemId, bool useCache)
{
    if (!NX_ASSERT(qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut))
        return std::nullopt;

    const auto system = appContext()->systemFinder()->getSystem(systemId);
    if (!system)
    {
        NX_WARNING(NX_SCOPE_TAG, "System %1 does not longer exist", systemId);
        return std::nullopt;
    }

    return cloudLogonData(system, useCache);
}

std::optional<LogonData> cloudLogonData(const SystemDescriptionPtr& system, bool useCache)
{
    if (!NX_ASSERT(system))
        return std::nullopt;

    if (!system->isCloudSystem() || !system->isReachable())
    {
        NX_WARNING(NX_SCOPE_TAG, "System %1 is no longer accessible", system->id());
        return std::nullopt;
    }

    const auto servers = system->servers();

    // Cloud systems are initialized with a dummy server with null id.
    const bool systemHasInitialServer = std::any_of(servers.cbegin(), servers.cend(),
        [system](const nx::vms::api::ModuleInformationWithAddresses& server)
        {
            return server.id.isNull() && isCloudUrl(system->getServerHost(nx::Uuid()));
        });
    const bool systemHasInitialServerOnly = systemHasInitialServer && servers.size() == 1;

    LogonData result;
    nx::Url url;

    if (systemHasInitialServerOnly)
    {
        url = system->getServerHost(nx::Uuid());
        NX_DEBUG(NX_SCOPE_TAG, "Connecting to the cloud system which was not pinged yet: %1", url);
    }

    result.expectedServerId = appContext()->coreSettings()->preferredCloudServer(system->id());
    if (result.expectedServerId)
    {
        if (system->isReachableServer(result.expectedServerId.value()))
        {
            url = system->getServerHost(result.expectedServerId.value());
            if (NX_ASSERT(!url.isEmpty()))
            {
                NX_DEBUG(NX_SCOPE_TAG, "Choosing stored preferred cloud server %1 (%2)",
                    result.expectedServerId, url);
            }
        }
        else if (systemHasInitialServerOnly)
        {
            url.setHost(helpers::serverCloudHost(system->id(), result.expectedServerId.value()));
            NX_DEBUG(NX_SCOPE_TAG, "Trying to connect to stored preferred cloud server %1 (%2)",
                result.expectedServerId, url);
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Stored preferred cloud server %1 is not reachable",
                result.expectedServerId);
        }
    }
    else
    {
        NX_DEBUG(NX_SCOPE_TAG, "Preferred cloud server was not found");
    }

    static const auto kStartVersionStoring = nx::utils::SoftwareVersion(6, 0);
    if (auto systemVersion = system->version(); !systemVersion.isNull()
        && !(systemHasInitialServerOnly && systemVersion < kStartVersionStoring))
    {
        result.expectedServerVersion = systemVersion;
        NX_DEBUG(NX_SCOPE_TAG, "Expect system version %1.", systemVersion);
    }

    // First system in the aggregator is the cloud one (if the system is cloud).
    result.expectedCloudSystemId = system->id();
    NX_DEBUG(NX_SCOPE_TAG, "Expect Cloud System ID %1.", system->id());

    if (url.isEmpty())
    {
        const auto debugServersInfo =
            [&servers, system]()
            {
                QStringList result;
                for (const auto& server: servers)
                {
                    result.push_back(
                        NX_FMT("   %1 (%2)", server.id, system->getServerHost(server.id)));
                }
                return result.join("\n");
            };

        NX_DEBUG(NX_SCOPE_TAG, "Connection options:\n%1", debugServersInfo());

        const auto iter = std::find_if(servers.cbegin(), servers.cend(),
            [system](const nx::vms::api::ModuleInformation& server)
            {
                return !server.id.isNull() && system->isReachableServer(server.id);
            });

        if (iter != servers.cend())
        {
            result.expectedServerId = iter->id;
            result.expectedServerVersion = iter->version;
            url = system->getServerHost(iter->id);
            if (NX_ASSERT(!url.isEmpty()))
            {
                NX_DEBUG(NX_SCOPE_TAG,
                    "Choosing %1 connection to the server %2 [%3] (%4)",
                    isCloudUrl(url) ? "cloud" : "local",
                    result.expectedServerId,
                    result.expectedServerVersion,
                    url);
            }
            else if (systemHasInitialServer)
            {
                url = system->getServerHost(nx::Uuid());
                NX_DEBUG(NX_SCOPE_TAG,
                    "Choosing initial server for not pinged site: %1",
                    url);
            }
        }
    }

    if (url.isEmpty())
    {
        NX_WARNING(NX_SCOPE_TAG, "No connection to the system %1 is found", system->id());
        return std::nullopt;
    }

    if (!NX_ASSERT(!url.host().isEmpty(), "Invalid system url %1", url))
        return std::nullopt;

    if (result.expectedCloudSystemId && useCache)
    {
        result.authCacheData =
            appContext()->coreSettings()->systemAuthenticationCache(*result.expectedCloudSystemId);
    }

    result.address = nx::network::SocketAddress(url.host(), url.port());
    result.userType = nx::vms::api::UserType::cloud;
    return result;
}

LogonData localLogonData(
    const nx::Url& url,
    const nx::network::http::Credentials& credentials)
{
    LogonData result;
    result.address = nx::network::SocketAddress(
        url.host(),
        url.port(::helpers::kDefaultConnectionPort));
    result.credentials = credentials;
    return result;
}

} // namespace nx::vms::client::core
