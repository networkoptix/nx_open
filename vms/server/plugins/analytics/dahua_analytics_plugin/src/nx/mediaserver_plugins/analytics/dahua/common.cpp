#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {

QnMutex Dahua::EngineManifest::m_cachedIdMutex;
QMap<QString, QString> Dahua::EngineManifest::m_eventTypeIdByInternalName;
QMap<QString, Dahua::EventType> Dahua::EngineManifest::m_eventTypeDescriptorById;

QString Dahua::EngineManifest::eventTypeByInternalName(const QString& value) const
{
    const auto internalEventName = value.toLower();
    QnMutexLocker lock(&m_cachedIdMutex);
    const QString result = m_eventTypeIdByInternalName.value(internalEventName);
    if (!result.isEmpty())
        return result;

    for (const auto& eventTypeDescriptor: eventTypes)
    {
        const auto possibleInternalNames = eventTypeDescriptor.internalName.toLower().split(L',');
        for (const auto& name: possibleInternalNames)
        {
            if (internalEventName.contains(name))
            {
                m_eventTypeIdByInternalName.insert(internalEventName, eventTypeDescriptor.id);
                return eventTypeDescriptor.id;
            }
        }
    }

    return QString();
}

const Dahua::EventType& Dahua::EngineManifest::eventTypeDescriptorById(
    const QString& id) const
{
    QnMutexLocker lock(&m_cachedIdMutex);
    auto it = m_eventTypeDescriptorById.find(id);
    if (it != m_eventTypeDescriptorById.end())
        return it.value();
    for (const auto& eventTypeDescriptor: eventTypes)
    {
        if (eventTypeDescriptor.id == id)
        {
            it = m_eventTypeDescriptorById.insert(id, eventTypeDescriptor);
            return it.value();
        }
    }

    static const Dahua::EventType kEmptyDescriptor;
    return kEmptyDescriptor;
}

Dahua::EventType Dahua::EngineManifest::eventTypeDescriptorByInternalName(
    const QString& internalName) const
{
    return eventTypeDescriptorById(eventTypeByInternalName(internalName));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Dahua::EventType, (json), \
    DahuaEventType_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Dahua::EngineManifest, (json), \
    DahuaEngineManifest_Fields)

} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
