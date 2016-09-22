
#include "recent_local_systems_finder.h"

#include <network/module_finder.h>
#include <client_core/client_core_settings.h>
#include <client_core/local_connection_data.h>
#include <nx/network/http/asynchttpclient.h>

QnRecentLocalSystemsFinder::QnRecentLocalSystemsFinder(QObject* parent):
    base_type(parent),
    m_systems()
{
    connect(qnClientCoreSettings, &QnClientCoreSettings::valueChanged, this,
        [this](int valueId)
        {
            if (valueId == QnClientCoreSettings::RecentLocalConnections)
                updateSystems();
        });

    updateSystems();

    const auto checkModulesChanged =
        [this](const QnModuleInformation& /* module */)
        {
            const auto modules = qnModuleFinder->foundModules();

            const auto processSystems =
                [this, modules](const SystemsHash& systems)
                {
                    const auto keys = systems.keys();
                    for (const auto key : keys)
                        checkSystem(systems.value(key), modules);
                };

            processSystems(m_systems);
            processSystems(m_onlineSystems);
        };

    const auto moduleFinder = qnModuleFinder;
    connect(moduleFinder, &QnModuleFinder::moduleChanged, this, checkModulesChanged);
    connect(qnModuleFinder, &QnModuleFinder::moduleLost, this, checkModulesChanged);
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
        m_onlineSystems.remove(systemId);
        removeVisibleSystem(systemId);
    }

    const auto modules = qnModuleFinder->foundModules();
    for (const auto systemId : added)
    {
        const auto system = newSystems.value(systemId);
        checkSystem(system, modules);
    }
}

void QnRecentLocalSystemsFinder::removeVisibleSystem(const QString& systemId)
{
    if (m_systems.remove(systemId))
        emit systemLost(systemId);
}

void QnRecentLocalSystemsFinder::checkSystem(const QnSystemDescriptionPtr& system,
    const ModulesList& modules)
{
    if (!system)
        return;

    const auto systemId = system->id();
    if (shouldRemoveSystem(system, modules))
    {
        // adds to online list
        if (!m_onlineSystems.contains(systemId))
            m_onlineSystems.insert(systemId, system);

        removeVisibleSystem(systemId);
    }
    else
    {
        m_onlineSystems.remove(systemId);
        if (m_systems.contains(systemId))
            return;

        m_systems.insert(systemId, system);
        emit systemDiscovered(system);
    }
}

bool QnRecentLocalSystemsFinder::shouldRemoveSystem(const QnSystemDescriptionPtr& system,
    const ModulesList& modules)
{
    if (!system)
        return true;

    /*
     * Local servers can be connected to cloud. Thus, if we have online server with
     * same local system id we should filter it out.
     */

    for (const auto module : modules)
    {
        const auto localSystemId = helpers::getLocalSystemId(module);
        if (system->id() == localSystemId)
            return true;
    }
    return false;
}