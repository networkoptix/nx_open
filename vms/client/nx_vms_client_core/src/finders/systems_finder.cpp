// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "systems_finder.h"

#include <finders/cloud_systems_finder.h>
#include <finders/direct_systems_finder.h>
#include <finders/recent_local_systems_finder.h>
#include <finders/scope_local_systems_finder.h>
#include <network/system_description_aggregator.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/ini.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/common/network/server_compatibility_validator.h>
#include <utils/common/instance_storage.h>

#include "search_address_manager.h"

using namespace nx::vms::client::core;

namespace {

enum class SystemHideFlag
{
    none = 0,
    incompatible = 1 << 0,
    notConnectableCloud = 1 << 1,
    requireCompatibilityMode = 1 << 2,
    nonLocalHost = 1 << 3,
    autoDiscovered = 1 << 4,
};
Q_DECLARE_FLAGS(SystemHideFlags, SystemHideFlag)

// Only CloudSystemFinder and DirectSystemFinder know whether the system has Cloud ID, and we can
// rely on this knowledge.
bool hasCloudIdInfo(int source)
{
    return source == QnSystemsFinder::Source::cloud || source == QnSystemsFinder::Source::direct;
}

// Only CloudSystemFinder and DirectSystemFinder know whether the system has Cloud ID, and we can
// rely on this knowledge.
bool hasCloudIdInfo(SystemDescriptionAggregatorPtr aggregator)
{
    return hasCloudIdInfo(aggregator->mostReliableSource());
}

} // namespace

QnSystemsFinder::QnSystemsFinder(QObject* parent):
    QnSystemsFinder(/*addDefaultFinders*/ true, parent)
{
}

QnSystemsFinder::QnSystemsFinder(bool addDefaultFinders, QObject* parent):
    base_type(parent)
{
    if (addDefaultFinders)
        initializeFinders();
}

QnSystemsFinder* QnSystemsFinder::instance()
{
    return appContext()->systemsFinder();
}

void QnSystemsFinder::initializeFinders()
{
    NX_ASSERT(nx::vms::common::ServerCompatibilityValidator::isInitialized(),
        "Internal finders use it on start when processing existing system descriptions");

    // Load saved systems first.
    auto recentLocalSystemsFinder = new QnRecentLocalSystemsFinder(this);
    addSystemsFinder(recentLocalSystemsFinder, Source::recent);

    auto scopeLocalSystemsFinder = new ScopeLocalSystemsFinder(this);
    addSystemsFinder(scopeLocalSystemsFinder, Source::saved);

    auto cloudSystemsFinder = new CloudSystemsFinder(this);
    addSystemsFinder(cloudSystemsFinder, Source::cloud);

    auto searchUrlManager = new SearchAddressManager(this);
    auto directSystemsFinder = new QnDirectSystemsFinder(searchUrlManager, this);
    addSystemsFinder(directSystemsFinder, Source::direct);
}

void QnSystemsFinder::addSystemsFinder(QnAbstractSystemsFinder* finder, Source source)
{
    nx::utils::ScopedConnectionsPtr connections(new nx::utils::ScopedConnections());

    *connections << connect(finder, &QnAbstractSystemsFinder::systemDiscovered, this,
        [this, source](const QnSystemDescriptionPtr& system)
        {
            onBaseSystemDiscovered(system, source);
        });

    *connections << connect(finder, &QnAbstractSystemsFinder::systemLost, this,
        [this, source](const QString& id, const nx::Uuid& localId)
        {
            onSystemLost(id, localId, source);
        });

    *connections << connect(finder, &QObject::destroyed, this,
        [this, finder]() { m_finders.remove(finder); });

    m_finders.insert(finder, connections);
    for (const auto& system: finder->systems())
        onBaseSystemDiscovered(system, source);
}

bool QnSystemsFinder::mergeSystemIntoExisting(const QnSystemDescriptionPtr& system, Source source)
{
    const auto it = m_systems.find(system->id());
    if (it != m_systems.end())
    {
        const auto existingSystem = *it;
        NX_VERBOSE(this, "Merging system %1 into existing one %2 by id %3",
            system, existingSystem, system->id());
        existingSystem->mergeSystem(source, system);
        return true;
    }

    // Recent and Scope finders do not know anything about the cloud ids, so we need to iterate
    // over all systems to check whether system with the same local id already exists.
    QList<SystemDescriptionAggregatorPtr> systemsWithSameLocalId;
    const nx::Uuid localId = system->localId();
    for (const auto& existingSystem: m_systems)
    {
        if (existingSystem->localId() == localId)
            systemsWithSameLocalId.append(existingSystem);
    }
    if (systemsWithSameLocalId.empty())
        return false;

    // Check whether newly found system knows about cloud id, but existing does not.
    if (hasCloudIdInfo(source))
    {
        for (const auto& existingSystem: systemsWithSameLocalId)
        {
            // Several systems with the same local id but different cloud ids are allowed.
            if (hasCloudIdInfo(existingSystem))
            {
                NX_ASSERT(existingSystem->cloudId() != system->cloudId(),
                    "If the systems have the same cloud id, their id also should match: %1 : %2.",
                    existingSystem,
                    system);
                continue;
            }

            NX_VERBOSE(this, "Replacing system %1 by newly found %2 by local id %3",
                existingSystem, system, localId);
            m_systems.remove(existingSystem->id());
            emit systemLost(existingSystem->id(), localId);

            auto replacementSystem = createNewAggregator(system, source);
            replacementSystem->mergeSystem(existingSystem);
            m_systems.insert(replacementSystem->id(), replacementSystem);
            emit systemDiscovered(replacementSystem);

            return true;
        }
    }
    else //< Check whether newly found system does not know about cloud id, but existing knows.
    {
        for (const auto& existingSystem: systemsWithSameLocalId)
        {
            if (hasCloudIdInfo(existingSystem))
            {
                NX_VERBOSE(this, "Merging system %1 into existing one %2 by local id %3",
                    system, existingSystem, system->id());
                existingSystem->mergeSystem(source, system);
                return true;
            }
        }
    }

    return false;
}

SystemDescriptionAggregatorPtr QnSystemsFinder::createNewAggregator(
    const QnSystemDescriptionPtr& system,
    Source source) const
{
    NX_VERBOSE(this, "Creating a new aggregator from %1", system);
    const SystemDescriptionAggregatorPtr target{new QnSystemDescriptionAggregator(source, system)};

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

    connect(target.get(), &QnBaseSystemDescription::systemNameChanged, this,
        updateRecordInRecentConnections);
    connect(target.get(), &QnBaseSystemDescription::versionChanged, this,
        updateRecordInRecentConnections);
    updateRecordInRecentConnections();

    return target;
}

bool QnSystemsFinder::systemIsHidden(const QnSystemDescriptionPtr& system, Source source) const
{
    const SystemHideFlags systemHideOptions(nx::vms::client::core::ini().systemsHideOptions);
    const auto systemCompatibility = system->systemCompatibility();

    if (systemHideOptions.testFlag(SystemHideFlag::incompatible)
        && systemCompatibility == QnSystemCompatibility::incompatible)
    {
        return true;
    }

    if (systemHideOptions.testFlag(SystemHideFlag::notConnectableCloud)
        && system->isCloudSystem()
        && !system->isOnline())
    {
        return true;
    }

    if (systemHideOptions.testFlag(SystemHideFlag::requireCompatibilityMode)
        && systemCompatibility == QnSystemCompatibility::requireCompatibilityMode)
    {
        return true;
    }

    if (systemHideOptions.testFlag(SystemHideFlag::nonLocalHost) && !system->hasLocalServer())
        return true;

    if (systemHideOptions.testFlag(SystemHideFlag::autoDiscovered) && source == Source::direct)
        return true;

    return false;
}

void QnSystemsFinder::onBaseSystemDiscovered(const QnSystemDescriptionPtr& system, Source source)
{
    NX_VERBOSE(this, "Discovered system %1 (source %2)", system, source);
    if (mergeSystemIntoExisting(system, source))
        return;

    if (systemIsHidden(system, source))
    {
        NX_VERBOSE(this, "System %1 is hidden by developer options", system);
        return;
    }

    const SystemDescriptionAggregatorPtr target = createNewAggregator(system, source);
    m_systems.insert(target->id(), target);
    emit systemDiscovered(target);
}

void QnSystemsFinder::onSystemLost(const QString& systemId, const nx::Uuid& localId, Source source)
{
    NX_VERBOSE(this, "Lost system %1 (local id %2, source %3)", systemId, localId, source);

    const auto it = m_systems.find(systemId);
    if (it == m_systems.end())
        return;

    const auto aggregator = *it;
    if (!aggregator->containsSystem(source))
        return;

    if (aggregator->isAggregator())
    {
        NX_VERBOSE(this, "Removing system %1 from aggregator %2", systemId, aggregator);
        aggregator->removeSystem(systemId, source);
        return;
    }

    NX_VERBOSE(this, "Removing aggregator %1 as the system is the last one", aggregator);
    m_systems.erase(it);
    emit systemLost(systemId, localId);
}

QnAbstractSystemsFinder::SystemDescriptionList QnSystemsFinder::systems() const
{
    SystemDescriptionList result;
    for (const auto& aggregator: m_systems)
        result.append(aggregator);

    return result;
}

QnSystemDescriptionPtr QnSystemsFinder::getSystem(const QString& systemId) const
{
    for (const auto& system: m_systems)
    {
        if (system->id() == systemId)
            return system;
    }
    return {};
}
