
#include "systems_finder.h"

#include <utils/common/instance_storage.h>

QnSystemsFinder::QnSystemsFinder(QObject *parent)
    : base_type(parent)
    , m_finders()
{
}

QnSystemsFinder::~QnSystemsFinder()
{}

void QnSystemsFinder::addSystemsFinder(QnAbstractSystemsFinder *finder)
{
    const auto discoveredConnection = 
        connect(finder, &QnAbstractSystemsFinder::systemDiscovered
        , this, &QnSystemsFinder::systemDiscovered);

    const auto lostConnection = 
        connect(finder, &QnAbstractSystemsFinder::systemLost
        , this, &QnSystemsFinder::systemLost);

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

QnAbstractSystemsFinder::SystemDescriptionList QnSystemsFinder::systems() const
{
    SystemDescriptionList result;
    for (const auto finder: m_finders.keys())
        result << finder->systems();
    return result;
}