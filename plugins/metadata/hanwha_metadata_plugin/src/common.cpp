#include "common.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver_plugins::metadata::hanwha::Hanwha,
    EventTypeFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver_plugins::metadata::hanwha::Hanwha,
    EventTypeFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver_plugins::metadata::hanwha::Hanwha,
    EventItemType)

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

QnUuid Hanwha::DriverManifest::eventTypeByInternalName(const QString& internalEventName) const
{
    QnUuid result = m_idByInternalName.value(internalEventName);
    if (!result.isNull())
        return result;

    for (const auto& eventDescriptor: outputEventTypes)
    {
        const auto& possibleInternalNames = eventDescriptor.internalName.split(L',');
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

const Hanwha::EventDescriptor& Hanwha::DriverManifest::eventDescriptorById(const QnUuid& id) const
{
    static const Hanwha::EventDescriptor defaultEventDescriptor{};

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

    return defaultEventDescriptor;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::mediaserver_plugins::metadata::hanwha::Hanwha::EventDescriptor,
    (json), EventDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::mediaserver_plugins::metadata::hanwha::Hanwha::DriverManifest,
    (json), DriverManifest_Fields)

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
