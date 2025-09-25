// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "single_system_description.h"

#include <network/system_helpers.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scoped_change_notifier.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/local_network_interfaces_manager.h>
#include <nx/vms/common/network/server_compatibility_validator.h>

namespace nx::vms::client::core {

namespace {

// TODO: #ynikitenkov Add fusion functions
#define EXTRACT_CHANGE_FLAG(fieldName, flag) static_cast<QnServerFields>(                \
    before.fieldName != after.fieldName ? flag : QnServerField::NoField)

QnServerFields getChanges(const nx::vms::api::ModuleInformationWithAddresses& before,
    const nx::vms::api::ModuleInformationWithAddresses& after)
{
    const auto fieldsResult =
        (EXTRACT_CHANGE_FLAG(systemName, QnServerField::SystemName)
        | EXTRACT_CHANGE_FLAG(name, QnServerField::Name)
        | EXTRACT_CHANGE_FLAG(cloudSystemId, QnServerField::CloudId))
        | EXTRACT_CHANGE_FLAG(version, QnServerField::Version);

    return (fieldsResult);
}
#undef EXTRACT_CHANGE_FLAG

} // namespace

using ServerCompatibilityValidator = nx::vms::common::ServerCompatibilityValidator;

SingleSystemDescription::SingleSystemDescription(
    const QString& systemId,
    const nx::Uuid& localSystemId,
    const QString& cloudSystemId,
    const QString& systemName)
    :
    m_id(systemId),
    m_localId(localSystemId),
    m_cloudId(cloudSystemId),
    m_systemName(extractSystemName(systemName))
{
}

QString SingleSystemDescription::extractSystemName(const QString& systemName)
{
    return systemName.isEmpty()
        ? '<' + tr("Unnamed Site") + '>'
        : systemName;
}

QString SingleSystemDescription::id() const
{
    return m_id;
}

nx::Uuid SingleSystemDescription::localId() const
{
    return m_localId;
}

QString SingleSystemDescription::cloudId() const
{
    return m_cloudId;
}

QString SingleSystemDescription::name() const
{
    return m_systemName;
}

QString SingleSystemDescription::ownerAccountEmail() const
{
    return m_ownerAccountEmail;
}

QString SingleSystemDescription::ownerFullName() const
{
    return m_ownerFullName;
}

bool SingleSystemDescription::isReachableServer(const nx::Uuid& serverId) const
{
    return m_reachableServers.contains(serverId);
}

SingleSystemDescription::ServersList SingleSystemDescription::servers() const
{
    ServersList result;
    for (const auto& id: m_prioritized)
    {
        const auto it = m_servers.find(id);
        if (it != m_servers.end())
            result.append(it.value());
    }
    return result;
}

nx::vms::api::SaasState SingleSystemDescription::saasState() const
{
    const auto systemServers = servers();
    if (systemServers.empty())
        return nx::vms::api::SaasState::uninitialized;

    return systemServers.first().saasState;
}

void SingleSystemDescription::addServer(
    const nx::vms::api::ModuleInformationWithAddresses& serverInfo,
    bool online)
{
    constexpr int kOnlinePriority = 0;
    constexpr int kOfflinePriority = 1;

    NX_DEBUG(this, "Cloud system <%1>, server <%2>(%3): adding server information",
        id(), serverInfo.id, online ? "online" : "offline");

    const bool containsServer = m_servers.contains(serverInfo.id);
    NX_ASSERT(!containsServer, "System contains specified server");

    if (containsServer)
    {
        NX_DEBUG(this,
            "Cloud system <%1>, server <%2>: server information exists already, updating it",
            id(), serverInfo.id);
        updateServer(serverInfo);
        return;
    }

    nx::utils::ScopedChangeNotifier<nx::vms::api::SaasState> saasStateChangeNotifier(
        [this]{ return saasState(); },
        [this]{ emit saasStateChanged(); });

    if (online && !isReachableServer(serverInfo.id))
        handleReachableServerAdded(serverInfo.id);

    const auto systemVersion = version();

    const int priority = online ? kOnlinePriority : kOfflinePriority;
    m_prioritized.insert(priority, serverInfo.id);
    m_servers.insert(serverInfo.id, serverInfo);
    m_serverTimestamps[serverInfo.id].restart();
    setName(serverInfo.systemName);
    emit serverAdded(serverInfo.id);

    if (serverInfo.version > systemVersion)
        emit versionChanged();
}

bool SingleSystemDescription::containsServer(const nx::Uuid& serverId) const
{
    return m_servers.contains(serverId);
}

nx::vms::api::ModuleInformationWithAddresses SingleSystemDescription::getServer(const nx::Uuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId),
        "System does not contain specified server");
    return m_servers.value(serverId);
}

QnServerFields SingleSystemDescription::updateServer(
    const nx::vms::api::ModuleInformationWithAddresses& serverInfo,
    bool online)
{
    NX_DEBUG(this, "Cloud system <%1>, server <%2>(%3): updating server information",
        id(), serverInfo.id, online ? "online" : "offline");
    NX_ASSERT(!serverInfo.version.isNull(), "Server %1 has null version", serverInfo.id);

    const auto systemVersion = version();
    const auto it = m_servers.find(serverInfo.id);
    const bool containsServer = (it != m_servers.end());
    NX_ASSERT(containsServer,
        "System does not contain specified server");

    if (!containsServer)
    {
        NX_DEBUG(this,
            "Cloud system <%1>, server <%2>: can't find server info, adding a new one",
            id(), serverInfo.id);
        addServer(serverInfo, online);
        return QnServerField::NoField;
    }

    nx::utils::ScopedChangeNotifier<nx::vms::api::SaasState> saasStateChangeNotifier(
        [this]{ return saasState(); },
        [this]{ emit saasStateChanged(); });

    if (online && !isReachableServer(serverInfo.id))
        handleReachableServerAdded(serverInfo.id);

    auto& current = it.value();
    const auto changes = getChanges(current, serverInfo);
    m_serverTimestamps[serverInfo.id].restart();
    current = serverInfo;
    if (!changes)
        return QnServerField::NoField;

    m_remoteAddressesCache.remove(serverInfo.id);
    setName(serverInfo.systemName);
    emit serverChanged(serverInfo.id, changes);

    // System version cannot be lowered.
    if (changes.testFlag(QnServerField::Version) && serverInfo.version > systemVersion)
        emit versionChanged();

    return changes;
}

bool SingleSystemDescription::isReachable() const
{
    return !m_reachableServers.isEmpty();
}

bool SingleSystemDescription::hasLocalServer() const
{
    const auto systemServers = servers();
    return std::ranges::any_of(
        systemServers,
        [this](const auto& moduleInformation)
        {
            const auto host = getServerHost(moduleInformation.id).host();
            return !nx::network::SocketGlobals::addressResolver().isCloudHostname(host)
                && (nx::network::HostAddress(host).isLoopback()
                    || (NX_ASSERT(appContext()->localNetworkInterfaceManager())
                        && appContext()->localNetworkInterfaceManager()->containsHost(host)));
        });
}

QnSystemCompatibility SingleSystemDescription::systemCompatibility() const
{
    const auto systemServers = servers();
    for (const auto& serverInfo: systemServers)
    {
        auto reason = ServerCompatibilityValidator::check(serverInfo);
        // If at least one server is compatible, all the system is compatible
        if (!reason)
            return QnSystemCompatibility::compatible;
        if (*reason == ServerCompatibilityValidator::Reason::customizationDiffers
            || *reason == ServerCompatibilityValidator::Reason::versionIsTooLow)
        {
            return QnSystemCompatibility::incompatible;
        }
    }

    return QnSystemCompatibility::requireCompatibilityMode;
}

bool SingleSystemDescription::isOauthSupported() const
{
    return true; //< We suppose that OAuth is supported by default.
}

nx::utils::SoftwareVersion SingleSystemDescription::version() const
{
    const auto systemServers = servers();
    if (systemServers.empty())
        return {};

    return std::max_element(systemServers.cbegin(), systemServers.cend(),
        [](const auto& l, const auto& r)
        {
            return l.version < r.version;
        })->version;
}

QString SingleSystemDescription::idForToStringFromPtr() const
{
    return nx::format("System: %1 [id: %2, local id: %3]",
        name(), id(), localId().toSimpleString());
}

void SingleSystemDescription::handleReachableServerAdded(const nx::Uuid& serverId)
{
    const bool containsAlready = m_reachableServers.contains(serverId);
    NX_ASSERT(!containsAlready, "Server is supposed as reachable already");
    if (containsAlready)
    {
        NX_DEBUG(this, "Cloud system <%1>, server <%2>: server is reachable already", id(),
            serverId);
        return;
    }

    const bool wasReachable = isReachable();
    m_reachableServers.insert(serverId);
    if (wasReachable == isReachable())
    {
        NX_DEBUG(this, "Cloud system <%1>, server <%2>: system is reachable already", id(),
            serverId);
        return;
    }

    NX_DEBUG(this, "Cloud system <%1>, server <%2>: system is reachable now", id(), serverId);
    emit reachableStateChanged();
}

void SingleSystemDescription::handleServerRemoved(const nx::Uuid& serverId)
{
    const bool wasReachable = isReachable();
    NX_DEBUG(this, "Cloud system <%1>, server <%2>: removing from reachable list", id(),
        serverId);

    if (m_reachableServers.remove(serverId) && (wasReachable != isReachable()))
    {
        NX_DEBUG(this, "Cloud system <%1>, server <%2>: system is unreachable now",
            id(), serverId);
        emit reachableStateChanged();
    }
    else
    {
        NX_DEBUG(this, "Cloud system <%1>, server <%2>: system reachable state is not changed",
            id(), serverId);
    }
}

void SingleSystemDescription::removeServer(const nx::Uuid& serverId)
{
    const bool containsServer = m_servers.contains(serverId);
    NX_ASSERT(containsServer,
        "System does not contain specified server");
    if (!containsServer)
        return;

    nx::utils::ScopedChangeNotifier<nx::vms::api::SaasState> saasStateChangeNotifier(
        [this]{ return saasState(); },
        [this]{ emit saasStateChanged(); });

    handleServerRemoved(serverId);
    const auto priorityPred = [serverId](const nx::Uuid& id) { return (serverId == id); };
    const auto it = std::find_if(m_prioritized.begin(), m_prioritized.end(), priorityPred);
    if (it != m_prioritized.end())
        m_prioritized.erase(it);

    m_hosts.remove(serverId);
    m_serverTimestamps.remove(serverId);
    m_remoteAddressesCache.remove(serverId);
    const bool someoneRemoved = m_servers.remove(serverId);
    if (someoneRemoved)
        emit serverRemoved(serverId);

    // Do not emit version changes here as system version cannot be lowered.
}

void SingleSystemDescription::setName(const QString& value)
{
    const auto newValue = extractSystemName(value);
    if (m_systemName == newValue)
        return;

    m_systemName = newValue;
    emit systemNameChanged();
}

void SingleSystemDescription::setServerHost(const nx::Uuid& serverId, const nx::Url& host)
{
    const bool containsServer = m_servers.contains(serverId);

    NX_ASSERT(containsServer,
        "System does not contain specified server");

    if (!containsServer)
        return;

    const auto it = m_hosts.find(serverId);
    const bool changed = ((it == m_hosts.end())
        || (it.value() != host));

    m_serverTimestamps[serverId].restart();
    if (!changed)
        return;
    m_hosts[serverId] = host;
    emit serverChanged(serverId, QnServerField::Host);
}

nx::Url SingleSystemDescription::getServerHost(const nx::Uuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId),
        "System does not contain specified server");

    return m_hosts.value(serverId);
}

QSet<nx::Url> SingleSystemDescription::getServerRemoteAddresses(const nx::Uuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId),
        "System does not contain specified server");

    if (m_remoteAddressesCache.contains(serverId))
        return m_remoteAddressesCache[serverId];

    const auto& server = m_servers.value(serverId);

    QSet<nx::Url> addresses;
    for (const auto& addressString: server.remoteAddresses)
    {
        nx::network::SocketAddress sockAddr(addressString.toStdString());

        nx::Url url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(sockAddr)
            .toUrl();
        if (url.port() == 0)
            url.setPort(server.port);

        addresses.insert(url);
    }

    m_remoteAddressesCache.insert(serverId, addresses);
    return addresses;
}

qint64 SingleSystemDescription::getServerLastUpdatedMs(const nx::Uuid& serverId) const
{
    NX_ASSERT(m_servers.contains(serverId),
        "System does not contain specified server");

    return m_serverTimestamps.value(serverId).elapsed();
}

void SingleSystemDescription::setPending(bool value)
{
    m_isPending = value;
}

bool SingleSystemDescription::isPending() const
{
    return m_isPending;
}

nx::Uuid SingleSystemDescription::organizationId() const
{
    return {};
}

} // namespace nx::vms::client::core
