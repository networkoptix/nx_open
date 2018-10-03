#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace plugins {

QnMutex Hikvision::PluginManifest::m_cachedIdMutex;
QMap<QString, QString> Hikvision::PluginManifest::m_eventTypeIdByInternalName;
QMap<QString, Hikvision::EventType> Hikvision::PluginManifest::m_eventTypeDescriptorById;

QString Hikvision::PluginManifest::eventTypeByInternalName(const QString& value) const
{
    const auto internalEventName = value.toLower();
    QnMutexLocker lock(&m_cachedIdMutex);
    const QString result = m_eventTypeIdByInternalName.value(internalEventName);
    if (!result.isNull())
        return result;

    for (const auto& eventTypeDescriptor: outputEventTypes)
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

const Hikvision::EventType& Hikvision::PluginManifest::eventTypeDescriptorById(
    const QString& id) const
{
    QnMutexLocker lock(&m_cachedIdMutex);
    auto it = m_eventTypeDescriptorById.find(id);
    if (it != m_eventTypeDescriptorById.end())
        return it.value();
    for (const auto& eventTypeDescriptor: outputEventTypes)
    {
        if (eventTypeDescriptor.id == id)
        {
            it = m_eventTypeDescriptorById.insert(id, eventTypeDescriptor);
            return it.value();
        }
    }

    static const Hikvision::EventType kEmptyDescriptor;
    return kEmptyDescriptor;
}

Hikvision::EventType Hikvision::PluginManifest::eventTypeDescriptorByInternalName(
    const QString& internalName) const
{
    return eventTypeDescriptorById(eventTypeByInternalName(internalName));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::EventType, (json), \
    HikvisionEventType_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::PluginManifest, (json), \
    HikvisionPluginManifest_Fields)

} // plugins
} // mediaserver
} // nx
