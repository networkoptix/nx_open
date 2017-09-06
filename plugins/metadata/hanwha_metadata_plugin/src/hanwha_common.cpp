#include "hanwha_common.h"

#include <nx/fusion/model_functions.h>
#include <nx/fusion/fusion/fusion_adaptor.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::plugins::Hanwha, EventTypeFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::plugins::Hanwha, EventTypeFlags)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::mediaserver::plugins::Hanwha, EventItemType)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::mediaserver::plugins::Hanwha::EventDescriptor, (json), EventDescriptor_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::mediaserver::plugins::Hanwha::DriverManifest, (json), DriverManifest_Fields)

namespace nx {
namespace mediaserver {
namespace plugins {

QnUuid Hanwha::DriverManifest::eventTypeByInternalName(const QString& internalEventName) const
{
    QnUuid result = m_idByInternalName.value(internalEventName);
    if (!result.isNull())
        return result;

    for (const auto& eventDescriptor: outputEventTypes)
    {
        if (eventDescriptor.internalName == internalEventName)
        {
            m_idByInternalName.insert(internalEventName, eventDescriptor.eventTypeId);
            return eventDescriptor.eventTypeId;
        }
    }
    return QUuid();
}

const Hanwha::EventDescriptor& Hanwha::DriverManifest::eventDescriptorById(const QnUuid& id) const
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

    return Hanwha::EventDescriptor();
}

} // plugins
} // mediaserver
} // nx
