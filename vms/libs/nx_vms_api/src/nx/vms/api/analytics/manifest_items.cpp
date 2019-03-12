#include "manifest_items.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::analytics {

bool operator==(const EventType& lh, const EventType& rh)
{
    return lh.id == rh.id;
}

uint qHash(const EventType& eventType)
{
    return qHash(eventType.id);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(EventType, (json), EventType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ObjectType, (json), ObjectType_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Group, (json), Group_Fields, (brief, true))

} // namespace nx::vms::api::analytics

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, EventTypeFlag,
    (nx::vms::api::analytics::EventTypeFlag::noFlags, "noFlags")
    (nx::vms::api::analytics::EventTypeFlag::stateDependent, "stateDependent")
    (nx::vms::api::analytics::EventTypeFlag::regionDependent, "regionDependent")
    (nx::vms::api::analytics::EventTypeFlag::hidden, "hidden")
)
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, EventTypeFlags,
    (nx::vms::api::analytics::EventTypeFlag::noFlags, "noFlags")
    (nx::vms::api::analytics::EventTypeFlag::stateDependent, "stateDependent")
    (nx::vms::api::analytics::EventTypeFlag::regionDependent, "regionDependent")
    (nx::vms::api::analytics::EventTypeFlag::hidden, "hidden")
)
