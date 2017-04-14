
#include "systems_finder.h"

#include <utils/common/instance_storage.h>
#include <network/system_description_aggregator.h>
#include <finders/direct_systems_finder.h>
#include <finders/cloud_systems_finder.h>
#include <finders/recent_local_systems_finder.h>
#include <client_core/client_core_settings.h>

QnSystemsFinder::QnSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_finders()
    , m_systems()
{
    enum
    {
        kCloudPriority,
        kDirectFinder,
        kRecentFinder,
    };

    auto cloudSystemsFinder = new QnCloudSystemsFinder(this);
    addSystemsFinder(cloudSystemsFinder, kCloudPriority);

    auto directSystemsFinder = new QnDirectSystemsFinder(this);
    addSystemsFinder(directSystemsFinder, kDirectFinder);

    auto recentLocalSystemsFinder = new QnRecentLocalSystemsFinder(this);
    addSystemsFinder(recentLocalSystemsFinder, kRecentFinder);

    const auto initRecentFinderBy =
        [recentLocalSystemsFinder](const QnAbstractSystemsFinder* finder)
        {
            connect(finder, &QnAbstractSystemsFinder::systemDiscovered,
                recentLocalSystemsFinder, &QnRecentLocalSystemsFinder::processSystemAdded);
            connect(finder, &QnAbstractSystemsFinder::systemLostInternal,
                recentLocalSystemsFinder, &QnRecentLocalSystemsFinder::processSystemRemoved);

            for (const auto& system: finder->systems())
                recentLocalSystemsFinder->processSystemAdded(system);
        };

    initRecentFinderBy(cloudSystemsFinder);
    initRecentFinderBy(directSystemsFinder);
}

QnSystemsFinder::~QnSystemsFinder()
{}

void QnSystemsFinder::addSystemsFinder(QnAbstractSystemsFinder *finder,
    int priority)
{
    const auto discovered = connect(finder, &QnAbstractSystemsFinder::systemDiscovered, this,
        [this, priority](const QnSystemDescriptionPtr& system)
        {
            onBaseSystemDiscovered(system, priority);
        });

    const auto lostConnection = connect(finder, &QnAbstractSystemsFinder::systemLost, this,
        [this, priority](const QString& id) { onSystemLost(id, priority); });

    const auto destroyedConnection = connect(finder, &QObject::destroyed, this,
        [this, finder]() { m_finders.remove(finder); });

    const auto connectionHolder = QnDisconnectHelper::create();
    *connectionHolder << discovered << lostConnection << destroyedConnection;

    m_finders.insert(finder, connectionHolder);
    for (const auto& system: finder->systems())
        onBaseSystemDiscovered(system, priority);
}

void QnSystemsFinder::onBaseSystemDiscovered(const QnSystemDescriptionPtr& system,
    int priority)
{
    const auto it = m_systems.find(system->id());
    if (it != m_systems.end())
    {
        const auto existingSystem = *it;
        existingSystem->mergeSystem(priority, system);
        return;
    }

    const AggregatorPtr target(new QnSystemDescriptionAggregator(priority, system));
    m_systems.insert(target->id(), target);
    connect(target, &QnBaseSystemDescription::systemNameChanged, this,
        [this, target]() { updateRecentConnections(target->localId(), target->name()); });

    updateRecentConnections(target->localId(), target->name());
    emit systemDiscovered(target.dynamicCast<QnBaseSystemDescription>());
}

void QnSystemsFinder::updateRecentConnections(const QnUuid& localSystemId, const QString& name)
{
    auto connections = qnClientCoreSettings->recentLocalConnections();

    auto it = connections.find(localSystemId);

    if (it == connections.end())
        return;

    if (it->systemName == name)
        return;

    it->systemName = name;

    qnClientCoreSettings->setRecentLocalConnections(connections);
    qnClientCoreSettings->save();
}

void QnSystemsFinder::onSystemLost(const QString& systemId,
    int priority)
{
    const auto it = m_systems.find(systemId);
    if (it == m_systems.end())
        return;

    const auto aggregator = *it;
    if (aggregator->isAggregator())
    {
        aggregator->removeSystem(priority);
        return;
    }

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

QnSystemDescriptionPtr QnSystemsFinder::getSystem(const QString &id) const
{
    const auto it = m_systems.find(id);
    return (it == m_systems.end()
        ? QnSystemDescriptionPtr()
        : it->dynamicCast<QnBaseSystemDescription>());
}
