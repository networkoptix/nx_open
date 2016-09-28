
#include "systems_finder.h"

#include <utils/common/instance_storage.h>
#include <network/system_description_aggregator.h>

QnSystemsFinder::QnSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_finders()
    , m_systems()
{
}

QnSystemsFinder::~QnSystemsFinder()
{}

void QnSystemsFinder::addSystemsFinder(QnAbstractSystemsFinder *finder,
    int priority)
{
    const auto discovered = connect(finder, &QnAbstractSystemsFinder::systemDiscovered, this,
        [this, priority](const QnSystemDescriptionPtr& system)
        {
            onSystemDiscovered(system, priority);
        });

    const auto lostConnection = connect(finder, &QnAbstractSystemsFinder::systemLost, this,
        [this, priority](const QString& id) { onSystemLost(id, priority); });

    const auto destroyedConnection = connect(finder, &QObject::destroyed, this,
        [this, finder]() { m_finders.remove(finder); });

    const auto connectionHolder = QnDisconnectHelper::create();
    *connectionHolder << discovered << lostConnection << destroyedConnection;

    m_finders.insert(finder, connectionHolder);
    for (const auto system : finder->systems())
        onSystemDiscovered(system, priority);
}

void QnSystemsFinder::onSystemDiscovered(const QnSystemDescriptionPtr& system,
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
    emit systemDiscovered(target.dynamicCast<QnBaseSystemDescription>());
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
