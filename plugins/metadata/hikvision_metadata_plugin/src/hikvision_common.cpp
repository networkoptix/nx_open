#include "hikvision_common.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::plugins::Hikvision, EventTypeFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::plugins::Hikvision, EventTypeFlags)

namespace nx {
namespace mediaserver {
namespace plugins {

QnUuid Hikvision::DriverManifest::eventTypeByInternalName(const QString& internalEventName) const
{
    QnUuid result = m_idByInternalName.value(internalEventName);
    if (!result.isNull())
        return result;

    for (const auto& eventDescriptor: outputEventTypes)
    {
        const auto possibleInternalNames = eventDescriptor.internalName.split(L',');
        for (const auto& name: possibleInternalNames)
        {
            if (internalEventName.contains(name))
            {
                m_idByInternalName.insert(internalEventName, eventDescriptor.eventTypeId);
                return eventDescriptor.eventTypeId;
            }
        }
    }

    return QUuid();
}

const Hikvision::EventDescriptor& Hikvision::DriverManifest::eventDescriptorById(const QnUuid& id) const
{
    auto itr = m_recordById.find(id);
    if (itr != m_recordById.end())
        return itr.value();
    for (const auto& eventDescriptor: outputEventTypes)
    {
        if (eventDescriptor.eventTypeId == id)
        {
            itr = m_recordById.insert(id, eventDescriptor);
            return itr.value();
        }
    }

    return Hikvision::EventDescriptor();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::EventDescriptor, (json), EventDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Hikvision::DriverManifest, (json), DriverManifest_Fields)

} // plugins
} // mediaserver
} // nx
