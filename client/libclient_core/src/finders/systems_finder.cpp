
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
    bool isCloudFinder)
{
    const auto discoveredConnection =
        connect(finder, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnSystemsFinder::onSystemDiscovered);

    const auto lostConnection =
        connect(finder, &QnAbstractSystemsFinder::systemLost, this,
            [this, isCloudFinder](const QString& id) { onSystemLost(id, isCloudFinder); });

    const auto destroyedConnection =
        connect(finder, &QObject::destroyed, this, [this, finder]()
    {
        m_finders.remove(finder);
    });

    const auto connectionHolder = QnDisconnectHelper::create();
    *connectionHolder << discoveredConnection << lostConnection
        << destroyedConnection;

    m_finders.insert(finder, connectionHolder);
    for (const auto system : finder->systems())
        onSystemDiscovered(system);
}

void QnSystemsFinder::onSystemDiscovered(const QnSystemDescriptionPtr& systemDescription)
{
    const auto it = m_systems.find(systemDescription->id());
    if (it != m_systems.end())
    {
        const auto existingSystem = *it;
        if (!systemDescription->isCloudSystem() || !existingSystem->isCloudSystem())
        {
            (*it)->mergeSystem(systemDescription);
            return;
        }
    }

    const AggregatorPtr target(new QnSystemDescriptionAggregator(systemDescription));
    m_systems.insert(target->id(), target);

    connect(target, &QnSystemDescriptionAggregator::idChanged, target,
        [this, lastId = target->id(), target]() mutable
        {
            m_systems.remove(lastId);
            m_systems.insert(target->id(), target);
            lastId = target->id();
        });
    emit systemDiscovered(target.dynamicCast<QnBaseSystemDescription>());
}

void QnSystemsFinder::onSystemLost(const QString& systemId, bool isCloudSystem)
{
    const auto it = m_systems.find(systemId);
    if (it == m_systems.end())
        return;

    const auto aggregator = *it;
    if (aggregator->isAggregator())
    {
        aggregator->removeSystem(systemId, isCloudSystem);
        return;
    }

    m_systems.erase(it);
    emit systemLost(aggregator->id());
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
