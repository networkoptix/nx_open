#include "recent_local_systems_finder.h"

#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>
#include <nx/network/http/asynchttpclient.h>

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent) :
    base_type(parent),
    m_recentSystems()
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
    {
        if (valueId == QnClientCoreSettings::RecentLocalConnections)
            updateSystems();
    });

    updateSystems();
}

QnAbstractSystemsFinder::SystemDescriptionList QnRecentLocalSystemsFinder::systems() const
{
    return m_recentSystems.values();
}

QnSystemDescriptionPtr QnRecentLocalSystemsFinder::getSystem(const QString &id) const
{
    return m_recentSystems.value(id, QnSystemDescriptionPtr());
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
    const auto currentKeys = m_recentSystems.keys().toSet();
    const auto added = newSystemsKeys - currentKeys;
    const auto removed = currentKeys - newSystemsKeys;

    QList<QString> IdsList;

    for (const auto systemId : removed)
        m_recentSystems.remove(systemId);

    for (const auto systemId : added)
    {
        const auto system = newSystems.value(systemId);
        m_recentSystems.insert(systemId, system);
        emit systemDiscovered(system);
    }

    for (const auto id : removed)
        emit systemLost(id);
}

