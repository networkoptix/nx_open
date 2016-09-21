
#include "recent_local_systems_finder.h"

#include <client_core/client_core_settings.h>

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent):
    base_type(parent)
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
        {
            if (valueId == QnClientCoreSettings::RecentLocalConnections)
                updateSystems();
        });
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
        newSystems.insert(connection.systemId,
            QnSystemDescription::createLocalSystem(connection.systemId, connection.systemName));
    }

    const auto newKeys = newSystems.keys().toSet();
    const auto currentKeys = m_systems.keys().toSet();
    const auto added = newKeys - currentKeys;
    const auto removed = currentKeys - newKeys;

    for (const auto systemId : removed)
    {
        m_systems.remove(systemId);
        emit systemLost(systemId);
    }

    for (const auto systemId : added)
    {
        const auto newSystem = newSystems.value(systemId);
        m_systems.insert(systemId, newSystem);
        emit systemDiscovered(newSystem);
    }
}
