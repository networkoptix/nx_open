// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_description_aggregator.h"

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/range_adapters.h>

#include "../system_finder.h"

namespace nx::vms::client::core {

namespace {

SystemDescription::ServersList subtractLists(
    SystemDescription::ServersList minuend,
    const SystemDescription::ServersList& subtrahend)
{
    const auto newEnd = std::remove_if(minuend.begin(), minuend.end(),
        [subtrahend](const nx::vms::api::ModuleInformationWithAddresses& current)
        {
            return std::any_of(subtrahend.begin(), subtrahend.end(),
                [currentId = current.id](const nx::vms::api::ModuleInformationWithAddresses& other)
                {
                    return other.id == currentId;
                });
        });

    minuend.erase(newEnd, minuend.end());
    return minuend;
};

bool isSameSystem(
    const SystemDescription& first,
    const SystemDescription& second)
{
    typedef QSet<nx::Uuid> ServerIdsSet;
    static const auto extractServerIds =
        [](const SystemDescription::ServersList& servers) -> ServerIdsSet
        {
            ServerIdsSet result;
            for (const auto& server: servers)
                result.insert(server.id);
            return result;
        };

    // Systems are the same if they have same set of servers
    return extractServerIds(first.servers()) == extractServerIds(second.servers());
}

bool isCloudHost(const QString& host)
{
    return nx::network::SocketGlobals::addressResolver().isCloudHostname(host);
}

}   // namespace

SystemDescriptionAggregator::SystemDescriptionAggregator(int priority,
    const SystemDescriptionPtr& systemDescription)
    :
    m_systems()
{
    mergeSystem(priority, systemDescription);
}

bool SystemDescriptionAggregator::isEmptyAggregator() const
{
    const bool emptySystemsList = m_systems.empty();
    NX_ASSERT(!emptySystemsList, "Empty systems aggregator detected");
    return emptySystemsList;
}

bool SystemDescriptionAggregator::isAggregator() const
{
    return (m_systems.size() > 1);
}

bool SystemDescriptionAggregator::containsSystem(const QString& systemId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->id() == systemId)
            return true;
    }
    return false;
}

int SystemDescriptionAggregator::mostReliableSource() const
{
    if (!NX_ASSERT(!isEmptyAggregator()))
        return SystemFinder::Source::saved; //< Lowest priority just in case.

    return m_systems.firstKey();
}

bool SystemDescriptionAggregator::containsSystem(int priority) const
{
    return m_systems.contains(priority);
}

void SystemDescriptionAggregator::mergeSystem(int priority, const SystemDescriptionPtr& system)
{
    if (!NX_ASSERT(system))
        return;

    if (const auto it = m_systems.find(priority); it != m_systems.end())
    {
        const auto oldSystem = *it;

        removeSystem(m_systems[priority]->id(), priority, /*isMerge*/ true);
    }

    m_systems.insert(priority, system);

    /**
     * We gather all servers for aggregated systems - for example, we may have
     * offline cloud system and same discovered local one
     */
    connect(system.get(), &SystemDescription::serverAdded,
        this, &SystemDescriptionAggregator::updateServers);
    connect(system.get(), &SystemDescription::serverRemoved,
        this, &SystemDescriptionAggregator::updateServers);

    connect(system.get(), &SystemDescription::newSystemStateChanged,
        this, &SystemDescriptionAggregator::newSystemStateChanged);

    connect(system.get(), &SystemDescription::serverChanged,
        this, &SystemDescriptionAggregator::handleServerChanged);
    connect(system.get(), &SystemDescription::systemNameChanged, this,
        [this, system]() { onSystemNameChanged(system); });
    connect(system.get(), &SystemDescription::versionChanged, this,
        &SystemDescriptionAggregator::versionChanged);

    connect(system.get(), &SystemDescription::onlineStateChanged,
        this, &SystemDescriptionAggregator::onOnlineStateChanged);
    connect(system.get(), &SystemDescription::reachableStateChanged,
        this, &SystemDescription::reachableStateChanged);
    connect(system.get(), &SystemDescription::oauthSupportedChanged,
        this, &SystemDescription::oauthSupportedChanged);
    connect(system.get(), &SystemDescription::system2faEnabledChanged,
        this, &SystemDescription::system2faEnabledChanged);
    connect(system.get(), &SystemDescription::saasStateChanged, this,
        [this]
        {
            if (m_systems.first() == sender())
                emit saasStateChanged();
        });

    updateServers();
    emitSystemChanged();
}

void SystemDescriptionAggregator::mergeSystem(const SystemDescriptionAggregatorPtr& system)
{
    for (auto [source, subsystem]: nx::utils::keyValueRange(system->m_systems))
    {
        if (NX_ASSERT(!m_systems.contains(source),
            "Worflow failure, see SystemFinder::mergeSystemIntoExisting() method"))
        {
            mergeSystem(source, subsystem);
        }
    }
}

void SystemDescriptionAggregator::emitSystemChanged()
{
    emit isCloudSystemChanged();
    emit ownerChanged();
    emit systemNameChanged();
    emit onlineStateChanged();
    emit reachableStateChanged();
    emit newSystemStateChanged();
    emit system2faEnabledChanged();
    emit versionChanged();
    emit saasStateChanged();
}

void SystemDescriptionAggregator::handleServerChanged(const nx::Uuid& serverId,
    QnServerFields fields)
{
    const auto it = std::find_if(m_servers.begin(), m_servers.end(),
        [serverId](const nx::vms::api::ModuleInformationWithAddresses& info) { return (serverId == info.id); });

    if (it != m_servers.end())
        emit serverChanged(serverId, fields);
}

void SystemDescriptionAggregator::onSystemNameChanged(const SystemDescriptionPtr& system)
{
    if (isEmptyAggregator() || !system)
        return;

    if (isSameSystem(*system, *m_systems.first()))
        emit systemNameChanged();
}

void SystemDescriptionAggregator::onOnlineStateChanged()
{
    if (isEmptyAggregator())
        return;

    // Offline cloud system may be listed as a local one. Send signal to refresh it's state.
    if (m_systems.first()->isCloudSystem())
    {
        emit isCloudSystemChanged();
        for (const auto& server: m_systems.first()->servers())
            emit serverChanged(server.id, QnServerField::Host);
    }

    emit onlineStateChanged();
}

void SystemDescriptionAggregator::removeSystem(
    const QString& systemId, int priority, bool isMerge)
{
    if (!m_systems.contains(priority))
        return;

    const auto& system = m_systems[priority];

    if (system->id() != systemId && !isMerge)
        return;

    system->disconnect(this);
    m_systems.remove(priority);

    // There is no need to update servers during a systems merge. They will be updated at the end
    // of the mergeSystem() (see VMS-47244).
    if (!isMerge)
        updateServers();

    if (!m_systems.isEmpty())
        emitSystemChanged();
}

QString SystemDescriptionAggregator::id() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->id());
}

nx::Uuid SystemDescriptionAggregator::localId() const
{
    return (isEmptyAggregator() ? nx::Uuid() : m_systems.first()->localId());
}

QString SystemDescriptionAggregator::cloudId() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->cloudId());
}

QString SystemDescriptionAggregator::name() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->name());
}

bool SystemDescriptionAggregator::isCloudSystem() const
{
    if (isEmptyAggregator() || !m_systems.first()->isCloudSystem())
        return false;

    // Show offline and reachable cloud system as local.
    return m_systems.first()->isOnline() || !isReachable();
}

bool SystemDescriptionAggregator::is2FaEnabled() const
{
    if (isEmptyAggregator())
        return false;

    // If the aggregator has cloud system, it would be the first in the m_systems map.
    return m_systems.first()->is2FaEnabled();
}

bool SystemDescriptionAggregator::isNewSystem() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const SystemDescriptionPtr& system) { return system->isNewSystem(); });
}


QString SystemDescriptionAggregator::ownerAccountEmail() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->ownerAccountEmail());
}

QString SystemDescriptionAggregator::ownerFullName() const
{
    return (isEmptyAggregator() ? QString() : m_systems.first()->ownerFullName());
}

SystemDescription::ServersList SystemDescriptionAggregator::servers() const
{
    return m_servers;
}

nx::vms::api::SaasState SystemDescriptionAggregator::saasState() const
{
    return isEmptyAggregator()
        ? nx::vms::api::SaasState::uninitialized
        : m_systems.first()->saasState();
}

bool SystemDescriptionAggregator::isReachableServer(const nx::Uuid& serverId) const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [serverId](const SystemDescriptionPtr& system)
        {
            return system->isReachableServer(serverId);
        });
}

bool SystemDescriptionAggregator::isOnline() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const SystemDescriptionPtr& system)
        {
            return system->isOnline();
        });
}

bool SystemDescriptionAggregator::isReachable() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const SystemDescriptionPtr& system)
        {
            return system->isReachable();
        });
}

void SystemDescriptionAggregator::updateServers()
{
    const auto newServers = gatherServers();
    const auto toRemove = subtractLists(m_servers, newServers);
    const auto toAdd = subtractLists(newServers, m_servers);

    m_servers = newServers;
    for (const auto& server: toRemove)
        emit serverRemoved(server.id);

    for (const auto& server: toAdd)
        emit serverAdded(server.id);

    /**
     * Updates server host in case we remove cloud system but have accesible local one.
     * See VMS-5884.
     */
    for (const auto& server: subtractLists(newServers, toAdd))
        emit serverChanged(server.id, QnServerField::Host);
}

SystemDescription::ServersList SystemDescriptionAggregator::gatherServers() const
{
    if (m_systems.empty())
        return {};

    // We are only interested in maximum priority system servers.
    return m_systems.first()->servers();
}

bool SystemDescriptionAggregator::containsServer(const nx::Uuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return true;
    }
    return false;
}

nx::vms::api::ModuleInformationWithAddresses SystemDescriptionAggregator::getServer(
    const nx::Uuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServer(serverId);
    }
    return {};
}

nx::utils::Url SystemDescriptionAggregator::getServerHost(const nx::Uuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (!systemDescription->containsServer(serverId))
            continue;

        // Do not return cloud host for offline cloud systems.
        const auto url = systemDescription->getServerHost(serverId);
        if (isCloudHost(url.host()) && !isCloudSystem())
            continue;

        return url;
    }

    return {};
}

QSet<nx::utils::Url> SystemDescriptionAggregator::getServerRemoteAddresses(
    const nx::Uuid& serverId) const
{
    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServerRemoteAddresses(serverId);
    }

    return {};
}

qint64 SystemDescriptionAggregator::getServerLastUpdatedMs(const nx::Uuid& serverId) const
{
    qint64 result = 0;

    for (const auto& systemDescription: m_systems)
    {
        if (systemDescription->containsServer(serverId))
            result = std::max<qint64>(result, systemDescription->getServerLastUpdatedMs(serverId));
    }

    return result;
}

bool SystemDescriptionAggregator::hasLocalServer() const
{
    if (isEmptyAggregator())
        return false;

    return std::any_of(m_systems.begin(), m_systems.end(),
        [](const SystemDescriptionPtr& system)
        {
            return system->hasLocalServer();
        });
}

QnSystemCompatibility SystemDescriptionAggregator::systemCompatibility() const
{
    for (const auto& system: m_systems)
    {
        const auto systemCompatibility = system->systemCompatibility();
        if (systemCompatibility == QnSystemCompatibility::compatible
            || systemCompatibility == QnSystemCompatibility::incompatible)
        {
            return systemCompatibility;
        }
    }

    return QnSystemCompatibility::requireCompatibilityMode;
}

bool SystemDescriptionAggregator::isOauthSupported() const
{
    if (isEmptyAggregator())
        return true;

    return std::all_of(m_systems.begin(), m_systems.end(),
        [](const SystemDescriptionPtr& system)
        {
            return system->isOauthSupported();
        });
}

nx::utils::SoftwareVersion SystemDescriptionAggregator::version() const
{
    if (isEmptyAggregator())
        return {};

    return (*std::max_element(m_systems.cbegin(), m_systems.cend(),
        [](const auto& l, const auto& r)
        {
            return l->version() < r->version();
        }))->version();
}

QString SystemDescriptionAggregator::idForToStringFromPtr() const
{
    QString result = nx::format("System Aggregator %1 [id: %2, local id: %3]:\n",
        name(), id(), localId());
    for (const auto& system: m_systems)
        result += nx::toString(system) + '\n';
    return result;
}

} // namespace nx::vms::client::core
