
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


    const QPointer<QnSystemsFinder> guard;
    const auto destroyedConnection = 
        connect(finder, &QObject::destroyed, this, [this, finder, guard]()
    {
        if (!guard)
            return;

        m_finders.remove(finder);
    });

    const auto connectionHolder = QnConnectionsHolder::create();
    connectionHolder->add(discoveredConnection);
    connectionHolder->add(lostConnection);
    connectionHolder->add(destroyedConnection);

    m_finders.insert(finder, connectionHolder);
}

QnAbstractSystemsFinder::SystemDescriptionList QnSystemsFinder::systems() const
{
    SystemDescriptionList result;
    for (auto it = m_finders.begin(); it != m_finders.end(); ++it)
        result << it.key()->systems();
    return result;
}