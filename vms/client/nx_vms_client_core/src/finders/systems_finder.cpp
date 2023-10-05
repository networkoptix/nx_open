// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_finder.h"

#include <finders/cloud_systems_finder.h>
#include <finders/direct_systems_finder.h>
#include <finders/recent_local_systems_finder.h>
#include <finders/scope_local_systems_finder.h>
#include <network/system_description_aggregator.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <utils/common/instance_storage.h>

#include "search_address_manager.h"

using namespace nx::vms::client::core;

enum class SystemHideFlag
{
    none = 0,
    incompatible = 1 << 0,
    notConnectableCloud = 1 << 1,
    requireCompatibilityMode = 1 << 2,
    nonLocalHost = 1 << 3,
};
Q_DECLARE_FLAGS(SystemHideFlags, SystemHideFlag)

QnSystemsFinder::QnSystemsFinder(QObject* parent):
    base_type(parent)
{
    NX_ASSERT(nx::vms::common::ServerCompatibilityValidator::isInitialized(),
        "Internal finders use it on start when processing existing system descriptions");

    enum
    {
        kCloudPriority,
        kDirectFinder,
        kRecentFinder,
        kScopeFinder,
    };

    auto cloudSystemsFinder = new CloudSystemsFinder(this);
    addSystemsFinder(cloudSystemsFinder, kCloudPriority);

    SearchAddressManager* searchUrlManager = new SearchAddressManager(this);

    auto directSystemsFinder = new QnDirectSystemsFinder(searchUrlManager, this);
    addSystemsFinder(directSystemsFinder, kDirectFinder);

    auto recentLocalSystemsFinder = new QnRecentLocalSystemsFinder(this);
    addSystemsFinder(recentLocalSystemsFinder, kRecentFinder);

    auto scopeLocalSystemsFinder = new ScopeLocalSystemsFinder(this);
    addSystemsFinder(scopeLocalSystemsFinder, kScopeFinder);
}

QnSystemsFinder::~QnSystemsFinder()
{}

QnSystemsFinder* QnSystemsFinder::instance()
{
    return appContext()->systemsFinder();
}

void QnSystemsFinder::addSystemsFinder(QnAbstractSystemsFinder* finder, int priority)
{
    nx::utils::ScopedConnectionsPtr connections(new nx::utils::ScopedConnections());

    *connections << connect(finder, &QnAbstractSystemsFinder::systemDiscovered, this,
        [this, priority](const QnSystemDescriptionPtr& system)
        {
            onBaseSystemDiscovered(system, priority);
        });

    *connections << connect(finder, &QnAbstractSystemsFinder::systemLost, this,
        [this, priority](const QString& id) { onSystemLost(id, priority); });

    *connections << connect(finder, &QObject::destroyed, this,
        [this, finder]() { m_finders.remove(finder); });

    m_finders.insert(finder, connections);
    for (const auto& system: finder->systems())
        onBaseSystemDiscovered(system, priority);
}

void QnSystemsFinder::onBaseSystemDiscovered(const QnSystemDescriptionPtr& system, int priority)
{
    const auto it = m_systems.find(system->localId());
    if (it != m_systems.end())
    {
        m_systemToLocalId[system->id()] = system->localId();

        const auto existingSystem = *it;
        existingSystem->mergeSystem(priority, system);
        return;
    }

    const SystemHideFlags systemHideOptions(nx::vms::client::core::ini().systemsHideOptions);
    const auto systemCompatibility = system->systemCompatibility();

    if (systemHideOptions.testFlag(SystemHideFlag::incompatible)
        && systemCompatibility == QnSystemCompatibility::incompatible)
    {
        return;
    }

    const auto isCloudAndNotOnline = [system] { return system->isCloudSystem() && !system->isOnline(); };
    if (systemHideOptions.testFlag(SystemHideFlag::notConnectableCloud) && isCloudAndNotOnline())
        return;

    if (systemHideOptions.testFlag(SystemHideFlag::requireCompatibilityMode)
        && systemCompatibility == QnSystemCompatibility::requireCompatibilityMode)
    {
        return;
    }

    if (systemHideOptions.testFlag(SystemHideFlag::nonLocalHost) && !system->hasLocalServer())
        return;

    m_systemToLocalId[system->id()] = system->localId();

    const AggregatorPtr target(new QnSystemDescriptionAggregator(priority, system));

    auto updateRecordInRecentConnections =
        [this, target]
        {
            auto connections = appContext()->coreSettings()->recentLocalConnections();
            auto it = connections.find(target->localId());
            if (it == connections.end())
                return;

            if (it->systemName == target->name() && it->version == target->version())
                return;

            it->systemName = target->name();
            it->version = target->version();

            appContext()->coreSettings()->recentLocalConnections = connections;
        };

    m_systems.insert(target->localId(), target);
    connect(target.get(), &QnBaseSystemDescription::systemNameChanged, this,
        updateRecordInRecentConnections);
    connect(target.get(), &QnBaseSystemDescription::versionChanged, this,
        updateRecordInRecentConnections);
    updateRecordInRecentConnections();

    emit systemDiscovered(target.dynamicCast<QnBaseSystemDescription>());
}

void QnSystemsFinder::onSystemLost(const QString& systemId, int priority)
{
    const auto localIdIter = m_systemToLocalId.find(systemId);
    if (localIdIter == m_systemToLocalId.end())
        return;

    const auto it = m_systems.find(*localIdIter);
    if (it == m_systems.end())
        return;

    const auto aggregator = *it;
    if (!aggregator->containsSystem(priority))
        return;

    if (aggregator->isAggregator())
    {
        aggregator->removeSystem(priority);
        return;
    }

    m_systemToLocalId.erase(localIdIter);
    m_systems.erase(it);
    emit systemLost(systemId);
}

QnAbstractSystemsFinder::SystemDescriptionList QnSystemsFinder::systems() const
{
    SystemDescriptionList result;
    for (const auto& aggregator: m_systems)
        result.append(aggregator.dynamicCast<QnBaseSystemDescription>());

    return result;
}

QnSystemDescriptionPtr QnSystemsFinder::getSystem(const QString& systemId) const
{
    const auto localIdIter = m_systemToLocalId.find(systemId);
    if (localIdIter == m_systemToLocalId.end())
        return QnSystemDescriptionPtr();

    const auto it = m_systems.find(*localIdIter);
    return (it == m_systems.end()
        ? QnSystemDescriptionPtr()
        : it->dynamicCast<QnBaseSystemDescription>());
}
