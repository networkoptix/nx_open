
#include "recent_local_systems_finder.h"

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>
#include <nx/network/http/asynchttpclient.h>

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent):
    base_type(parent),

    m_recentSystems(),
    m_onlineSystems(),
    m_onlineSystemNames(),
    m_finalSystems()
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
        {
            if (valueId == QnClientCoreSettings::RecentLocalConnections)
                updateSystems();
        });

    updateSystems();
}


void QnRecentLocalSystemsFinder::processSystemAdded(const QnSystemDescriptionPtr& system)
{
    if (m_onlineSystems.contains(system->id()))
        return;

    qDebug() << "--- processSystemAdded " << system->name();
    const auto newFilter = system->name();
    ++m_onlineSystemNames[newFilter];
    m_onlineSystems.insert(system->id(), system->name());

    // Filters out final systems
    for (auto it = m_finalSystems.begin(); it != m_finalSystems.end();)
    {
        const auto targetSystem = it.value();
        if (newFilter == targetSystem->name())
        {
            it = m_finalSystems.erase(it);
            emit systemLost(targetSystem->id());
        }
        else
            ++it;
    }
}

void QnRecentLocalSystemsFinder::processSystemRemoved(const QString& systemId)
{
    if (!m_onlineSystems.contains(systemId))
        return;

    const auto systemName = m_onlineSystems.value(systemId);
    const auto it = m_onlineSystemNames.find(systemName);
    if (it == m_onlineSystemNames.end())
        return;

    auto& namesCount = it.value();
    if (--namesCount > 0)
        return; // We have other systems with same name

    m_onlineSystemNames.erase(it);

    SystemsHash newRecentSystems;
    for (const auto currentSystem : m_recentSystems)
    {
        if (!isFilteredOut(currentSystem))
            newRecentSystems.insert(currentSystem->id(), currentSystem);
    }

    updateRecentSystems(newRecentSystems);
}

QnAbstractSystemsFinder::SystemDescriptionList QnRecentLocalSystemsFinder::systems() const
{
    return m_finalSystems.values();
}

QnSystemDescriptionPtr QnRecentLocalSystemsFinder::getSystem(const QString &id) const
{
    return m_finalSystems.value(id, QnSystemDescriptionPtr());
}

void QnRecentLocalSystemsFinder::updateSystems()
{
    SystemsHash newSystems;
    for (const auto connection : qnClientCoreSettings->recentLocalConnections())
    {
        if (connection.systemId.isEmpty())
            continue;

        const auto system = QnSystemDescription::createLocalSystem(
            connection.systemId, connection.systemName);

        const auto itSameName = std::find_if(newSystems.begin(), newSystems.end(),
            [name = system->name()](const QnSystemDescriptionPtr& val)
            {
                return (name == val->name());
            });

        if (itSameName == newSystems.end())
            newSystems.insert(connection.systemId, system);
        else if (itSameName.value()->id() > system->id())   //< to have definitely one system
        {
            newSystems.erase(itSameName);
            newSystems.insert(connection.systemId, system);
        }
    }

    updateRecentSystems(newSystems);
}

void QnRecentLocalSystemsFinder::updateRecentSystems(const SystemsHash& newSystems)
{
    const auto newSystemsKeys = newSystems.keys().toSet();
    const auto currentKeys = m_recentSystems.keys().toSet();
    const auto added = newSystemsKeys - currentKeys;
    const auto removed = currentKeys - newSystemsKeys;

    for (const auto systemId : removed)
    {
        m_recentSystems.remove(systemId);
        if (m_finalSystems.remove(systemId))
            emit systemLost(systemId);
    }

    for (const auto systemId : added)
    {
        const auto system = newSystems.value(systemId);
        m_recentSystems.insert(systemId, system);
        if (!isFilteredOut(system))
        {
            m_finalSystems.insert(systemId, system);
            emit systemDiscovered(system);
        }
    }
}

bool QnRecentLocalSystemsFinder::isFilteredOut(const QnSystemDescriptionPtr& system) const
{
    return m_onlineSystemNames.contains(system->name());
}
