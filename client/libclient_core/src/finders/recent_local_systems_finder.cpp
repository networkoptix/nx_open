
#include "recent_local_systems_finder.h"

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>
#include <nx/network/http/asynchttpclient.h>

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent):
    base_type(parent),
    m_systems(),
    m_reservedSystems(),
    m_onlineSystems()
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

    m_onlineSystems.insert(system->id(), system->name());
    checkAllSystems();
}

void QnRecentLocalSystemsFinder::processSystemRemoved(const QString& systemId)
{
    if (m_onlineSystems.remove(systemId))
        checkAllSystems();
}

void QnRecentLocalSystemsFinder::checkAllSystems()
{
    const auto processSystems =
        [this](const SystemsHash& systems)
        {
            for (const auto id : systems.keys())
                checkSystem(systems.value(id));
        };

    processSystems(m_systems);
    processSystems(m_reservedSystems);
}

QnAbstractSystemsFinder::SystemDescriptionList QnRecentLocalSystemsFinder::systems() const
{
    return m_systems.values();
}

QnSystemDescriptionPtr QnRecentLocalSystemsFinder::getSystem(const QString &id) const
{
    return m_systems.value(id, QnSystemDescriptionPtr());
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
        newSystems.insert(connection.systemId, system);
    }

    const auto newSystemsKeys = newSystems.keys().toSet();
    const auto currentKeys = m_systems.keys().toSet();
    const auto added = newSystemsKeys - currentKeys;
    const auto removed = currentKeys - newSystemsKeys;

    for (const auto systemId : removed)
    {
        m_reservedSystems.remove(systemId);
        removeVisibleSystem(systemId);
    }

    for (const auto systemId : added)
    {
        const auto system = newSystems.value(systemId);
        checkSystem(system);
    }
}

void QnRecentLocalSystemsFinder::removeVisibleSystem(const QString& systemId)
{
    if (m_systems.remove(systemId))
        emit systemLost(systemId);
}

void QnRecentLocalSystemsFinder::checkSystem(const QnSystemDescriptionPtr& system)
{
    if (!system)
        return;

    const auto systemId = system->id();
    if (shouldRemoveSystem(system))
    {
        // adds to online list
        if (!m_reservedSystems.contains(systemId))
            m_reservedSystems.insert(systemId, system);

        removeVisibleSystem(systemId);
    }
    else
    {
        m_reservedSystems.remove(systemId);
        if (m_systems.contains(systemId))
            return;

        m_systems.insert(systemId, system);
        emit systemDiscovered(system);
    }
}

bool QnRecentLocalSystemsFinder::shouldRemoveSystem(const QnSystemDescriptionPtr& system)
{
    if (!system)
        return true;

    for (const auto systemName : m_onlineSystems)
    {
        if (system->name() == systemName)
            return true;
    }
    return false;
}