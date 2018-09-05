#include "common.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver {
namespace plugins {

QnMutex Hikvision::DriverManifest::m_cachedIdMutex;
QMap<QString, QString> Hikvision::DriverManifest::m_eventTypeIdByInternalName;
QMap<QString, Hikvision::EventTypeDescriptor> Hikvision::DriverManifest::m_eventTypeDescriptorById;

QString Hikvision::DriverManifest::eventTypeByInternalName(const QString& value) const
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

const Hikvision::EventTypeDescriptor& Hikvision::DriverManifest::eventTypeDescriptorById(
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

    static const Hikvision::EventTypeDescriptor kEmptyDescriptor;
    return kEmptyDescriptor;
}

Hikvision::EventTypeDescriptor Hikvision::DriverManifest::eventTypeDescriptorByInternalName(
    const QString& internalName) const
{
    return eventTypeDescriptorById(eventTypeByInternalName(internalName));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::EventTypeDescriptor, (json), EventTypeDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::DriverManifest, (json), DriverManifest_Fields)

} // plugins
} // mediaserver
} // nx
