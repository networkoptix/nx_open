
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

void QnSystemsFinder::addSystemsFinder(QnAbstractSystemsFinder *finder)
{
    const auto discoveredConnection = 
        connect(finder, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnSystemsFinder::onSystemDiscovered);

    const auto lostConnection = 
        connect(finder, &QnAbstractSystemsFinder::systemLost
        , this, &QnSystemsFinder::onSystemLost);

    const auto destroyedConnection = 
        connect(finder, &QObject::destroyed, this, [this, finder]()
    {
        m_finders.remove(finder);
    });

    const auto connectionHolder = QnDisconnectHelper::create();
    *connectionHolder << discoveredConnection << lostConnection
        << destroyedConnection;

    m_finders.insert(finder, connectionHolder);
}

void QnSystemsFinder::onSystemDiscovered(const QnSystemDescriptionPtr& systemDescription)
{
    const auto it = std::find_if(m_systems.begin(), m_systems.end(), 
        [targetName = systemDescription->name()](const QnSystemDescriptionPtr &description)
    {
        return (description->name() == targetName);
    });

    if (it != m_systems.end())
    {
        (*it)->mergeSystem(systemDescription);
        return;
    }

    const AggregatorPtr target(new QnSystemDescriptionAggregator(systemDescription));
    m_systems.append(target);
    emit systemDiscovered(target.dynamicCast<QnBaseSystemDescription>());
}

void QnSystemsFinder::onSystemLost(const QString& systemId) 
{
    const auto it = std::find_if(m_systems.begin(), m_systems.end(), 
        [systemId](const AggregatorPtr& aggregator)
    {
        return aggregator->containsSystem(systemId);
    });

    if (it == m_systems.end())
        return;

    const auto aggregator = *it;
    if (aggregator->isAggregator())
    {
        aggregator->removeSystem(systemId);
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
    QnSystemDescriptionPtr result;
    for (const auto finder : m_finders.keys())
    {
        result = finder->getSystem(id);
        if (result)
            break;
    }
    return result;
}
