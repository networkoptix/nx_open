
#include "system_description_aggregator.h"

#include <nx/utils/log/assert.h>

namespace {

QnBaseSystemDescription::ServersList subtractLists(
    const QnBaseSystemDescription::ServersList& first,
    const QnBaseSystemDescription::ServersList& second)
{
    QnBaseSystemDescription::ServersList result;
    std::copy_if(first.begin(), first.end(), std::back_inserter(result),
        [&second](const QnModuleInformation& first)
        {
            const auto secondIt = std::find_if(second.begin(), second.end(),
                                                [first](const QnModuleInformation& second)
            {
                return (first.id == second.id);
            });

            return (secondIt == second.end()); // add to result if not found in second
        });
    return result;
};

}   // namespace

QnSystemDescriptionAggregator::QnSystemDescriptionAggregator(int priority,
    const QnSystemDescriptionPtr& systemDescription)
    :
    m_systems()
{
    mergeSystem(priority, systemDescription);
}

bool QnSystemDescriptionAggregator::isAggregator() const
{
    return (m_systems.size() > 1);
}

bool QnSystemDescriptionAggregator::containsSystem(const QString& systemId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->id() == systemId)
            return true;
    }
    return false;
}

void QnSystemDescriptionAggregator::mergeSystem(int priority,
    const QnSystemDescriptionPtr& system)
{
    NX_ASSERT(system, "System is empty");
    if (!system)
        return;

    const bool exists = m_systems.contains(priority);
    NX_ASSERT(!exists, "System exists already");
    if (exists)
        return;

    const int lastPriority = (m_systems.isEmpty() ? 0 : m_systems.firstKey());
    const bool headSystemChanged = ((priority < lastPriority) || m_systems.empty());

    m_systems.insert(priority, system);

    connect(system.data(), &QnBaseSystemDescription::serverAdded,
        this, &QnSystemDescriptionAggregator::updateServers);
    connect(system.data(), &QnBaseSystemDescription::serverRemoved,
        this, &QnSystemDescriptionAggregator::updateServers);

    connect(system.data(), &QnBaseSystemDescription::serverChanged,
        this, &QnSystemDescriptionAggregator::handleServerChanged);
    connect(system.data(), &QnBaseSystemDescription::systemNameChanged, this,
        [this, system]() { onSystemNameChanged(system); });

    updateServers();
    if (headSystemChanged)
        emitHeadChanged();
}

void QnSystemDescriptionAggregator::emitHeadChanged()
{
    emit isCloudSystemChanged();
    emit ownerChanged();
    emit systemNameChanged();
}

void QnSystemDescriptionAggregator::handleServerChanged(const QnUuid& serverId,
    QnServerFields fields)
{
    const auto it = std::find_if(m_servers.begin(), m_servers.end(),
        [serverId](const QnModuleInformation& info) { return (serverId == info.id); });

    if (it != m_servers.end())
        emit serverChanged(serverId, fields);
}

void QnSystemDescriptionAggregator::onSystemNameChanged(const QnSystemDescriptionPtr& system)
{
    if (m_systems.empty() || !system)
        return;

    /*
     * We have 3 types of systems here:
     * 1. Cloud systems (online/offline, it does not matter) - change of system name
     *    is not processing now. (TODO: #add processing of cloud system name change)
     * 2. Local offline recent systems - they can't change their names
     * 3. Locally discovered systems - we process name change for systems with version
     *    less than 2.3. (TODO: #ynikitenkov add processing of local online systems name change)
     * Thus, it is enough to just test if system with changed name and head system is both cloud
     * or both non cloud - it means that it is same system.
     */

    const auto headSystem = m_systems.begin().value();
    const auto sameAsHeadSystem = (headSystem->isCloudSystem() != system->isCloudSystem());
    if (sameAsHeadSystem)
        emit systemNameChanged();
}

void QnSystemDescriptionAggregator::removeSystem(int priority)
{
    const bool exist = m_systems.contains(priority);
    NX_ASSERT(exist, "System does not exist");
    if (!exist)
        return;

    const auto oldServers = servers();
    const auto system = m_systems.value(priority);

    const bool headSystemChanged = (priority == m_systems.firstKey());
    disconnect(system.data(), nullptr, this, nullptr);
    m_systems.remove(priority);

    updateServers();
    if (headSystemChanged && !m_systems.isEmpty())
        emitHeadChanged();
}

QString QnSystemDescriptionAggregator::id() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return QString();

    return m_systems.first()->id();
}

QnUuid QnSystemDescriptionAggregator::localId() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return QnUuid();

    return m_systems.first()->localId();
}

QString QnSystemDescriptionAggregator::name() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return QString();

    return m_systems.first()->name();
}

bool QnSystemDescriptionAggregator::isCloudSystem() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return false;

    return m_systems.first()->isCloudSystem();
}

bool QnSystemDescriptionAggregator::isNewSystem() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return false;

    for (const auto system : m_systems)
    {
        if (system->isNewSystem())
            return true;
    }
    return false;
}


QString QnSystemDescriptionAggregator::ownerAccountEmail() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return QString();

    return m_systems.first()->ownerAccountEmail();
}

QString QnSystemDescriptionAggregator::ownerFullName() const
{
    const bool emptySystems = m_systems.empty();
    NX_ASSERT(!emptySystems, "Invalid aggregator");
    if (emptySystems)
        return QString();

    return m_systems.first()->ownerFullName();
}

QnBaseSystemDescription::ServersList QnSystemDescriptionAggregator::servers() const
{
    return m_servers;
}

void QnSystemDescriptionAggregator::updateServers()
{
    const auto newServers = gatherServers();
    const auto toRemove = subtractLists(m_servers, newServers);
    const auto toAdd = subtractLists(newServers, m_servers);

    m_servers = newServers;
    for (const auto& server : toRemove)
        emit serverRemoved(server.id);

    for (const auto& server : toAdd)
        emit serverAdded(server.id);
}

QnBaseSystemDescription::ServersList QnSystemDescriptionAggregator::gatherServers() const
{
    ServersList result;
    for (const auto systemDescription : m_systems)
    {
        const auto toAddList = subtractLists(systemDescription->servers(), result);
        std::copy(toAddList.begin(), toAddList.end(), std::back_inserter(result));
    }
    return result;
}

bool QnSystemDescriptionAggregator::containsServer(const QnUuid& serverId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return true;
    }
    return false;
}

QnModuleInformation QnSystemDescriptionAggregator::getServer(const QnUuid& serverId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServer(serverId);
    }
    return QnModuleInformation();
}

QUrl QnSystemDescriptionAggregator::getServerHost(const QnUuid& serverId) const
{
    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            return systemDescription->getServerHost(serverId);
    }

    return QString();
}

qint64 QnSystemDescriptionAggregator::getServerLastUpdatedMs(const QnUuid& serverId) const
{
    qint64 result = 0;

    for (const auto systemDescription : m_systems)
    {
        if (systemDescription->containsServer(serverId))
            result = std::max<qint64>(result, systemDescription->getServerLastUpdatedMs(serverId));
    }

    return result;
}
