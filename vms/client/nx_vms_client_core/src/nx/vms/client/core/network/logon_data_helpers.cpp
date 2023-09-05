// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logon_data_helpers.h"

#include <finders/systems_finder.h>
#include <network/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/settings/client_core_settings.h>

namespace nx::vms::client::core {

namespace {

bool isCloudUrl(const nx::utils::Url& url)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostname(url.host());
}

} // namespace

std::optional<LogonData> cloudLogonData(const QString& systemId)
{
    if (!NX_ASSERT(qnCloudStatusWatcher->status() != CloudStatusWatcher::LoggedOut))
        return std::nullopt;

    const auto system = qnSystemsFinder->getSystem(systemId);
    if (!system)
    {
        NX_WARNING(NX_SCOPE_TAG, "System %1 does not longer exist", systemId);
        return std::nullopt;
    }

    return cloudLogonData(system);
}

std::optional<LogonData> cloudLogonData(const QnSystemDescriptionPtr& system)
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
            return server.id.isNull() && isCloudUrl(system->getServerHost(QnUuid()));
        });
    const bool systemHasInitialServerOnly = systemHasInitialServer && servers.size() == 1;

    LogonData result;
    nx::utils::Url url;

    if (systemHasInitialServerOnly)
    {
        url = system->getServerHost(QnUuid());
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
                    result.expectedServerId->toString(), url);
            }
        }
        else if (systemHasInitialServerOnly)
        {
            url.setHost(helpers::serverCloudHost(system->id(), result.expectedServerId.value()));
            NX_DEBUG(NX_SCOPE_TAG, "Trying to connect to stored preferred cloud server %1 (%2)",
                result.expectedServerId->toString(), url);
        }
        else
        {
            NX_DEBUG(NX_SCOPE_TAG, "Stored preferred cloud server %1 is not reachable",
                result.expectedServerId->toString());
        }
    }

    if (auto systemVersion = system->version(); !systemVersion.isNull())
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
                url = system->getServerHost(QnUuid());
                NX_DEBUG(NX_SCOPE_TAG,
                    "Connecting to the cloud system which was not pinged yet: %1",
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

    result.address = nx::network::SocketAddress(url.host(), url.port());
    result.userType = nx::vms::api::UserType::cloud;
    return result;
}

LogonData localLogonData(
    const nx::utils::Url& url,
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
