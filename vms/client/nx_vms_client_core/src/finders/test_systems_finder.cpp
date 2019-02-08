
#include "test_systems_finder.h"

#include <QtCore/QUrl>

#include <nx/utils/log/assert.h>

QnTestSystemsFinder::QnTestSystemsFinder(QObject* parent):
    base_type(parent)
{
    connect(this, &QnAbstractSystemsFinder::systemLostInternal, this,
        [this](const QString& id, const QnUuid& /* localId */)
        {
            emit systemLost(id);
        });
}

QnTestSystemsFinder::SystemDescriptionList QnTestSystemsFinder::systems() const
{
    return m_systems.values();
}

QnSystemDescriptionPtr QnTestSystemsFinder::getSystem(const QString &id) const
{
    return m_systems.value(id);
}

void QnTestSystemsFinder::addSystem(const QnSystemDescriptionPtr& system)
{
    if (m_systems.contains(system->id()))
    {
        NX_ASSERT(false, "System exist alreay");
        return;
    }

    m_systems.insert(system->id(), system);
    emit systemDiscovered(system);
}

void QnTestSystemsFinder::removeSystem(const QString& id)
{
    const auto it = m_systems.find(id);
    if (it == m_systems.end())
    {
        NX_ASSERT(false, "System does not exist");
        return;
    }

    const auto localId = it.value()->localId();
    m_systems.remove(id);
    emit systemLostInternal(id, localId);
}

void QnTestSystemsFinder::clearSystems()
{
    for (const auto& system: m_systems)
        emit systemLostInternal(system->id(), system->localId());

    m_systems.clear();
}
