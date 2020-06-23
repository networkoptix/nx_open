#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {

QnMutex Hikvision::EngineManifest::m_cachedIdMutex;
QMap<QString, QString> Hikvision::EngineManifest::m_eventTypeIdByInternalName;
QMap<QString, Hikvision::EventType> Hikvision::EngineManifest::m_eventTypeById;

QString Hikvision::EngineManifest::eventTypeIdByInternalName(const QString& value) const
{
    const QString internalEventName = value.toLower();
    QnMutexLocker lock(&m_cachedIdMutex);
    const QString result = m_eventTypeIdByInternalName.value(internalEventName);
    if (!result.isEmpty())
        return result;

    for (const EventType& eventType: eventTypes)
    {
        const auto possibleInternalNames = eventType.internalName.toLower().split(L',');
        for (const auto& name: possibleInternalNames)
        {
            if (internalEventName.contains(name))
            {
                m_eventTypeIdByInternalName.insert(internalEventName, eventType.id);
                return eventType.id;
            }
        }
    }

    return QString();
}

const Hikvision::EventType& Hikvision::EngineManifest::eventTypeById(const QString& id) const
{
    QnMutexLocker lock(&m_cachedIdMutex);

    if (const auto it = m_eventTypeById.find(id); it != m_eventTypeById.end())
        return it.value();

    for (const EventType& eventType: eventTypes)
    {
        if (eventType.id == id)
        {
            const auto it = m_eventTypeById.insert(id, eventType);
            return it.value();
        }
    }

    static const Hikvision::EventType kEmptyEventType;
    return kEmptyEventType;
}

const Hikvision::EventType& Hikvision::EngineManifest::eventTypeByInternalName(
    const QString& internalName) const
{
    return eventTypeById(eventTypeIdByInternalName(internalName));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::EventType, (json), \
    HikvisionEventType_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::EngineManifest, (json), \
    HikvisionEngineManifest_Fields)

} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
