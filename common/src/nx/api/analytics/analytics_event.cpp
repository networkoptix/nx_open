#include "analytics_event.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::api::Analytics, EventTypeFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::api::Analytics, EventTypeFlags)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::api::Analytics::EventType, (json), AnalyticsEventType_Fields, (brief, true))

namespace nx {
namespace api {

bool operator==(const Analytics::EventType& lh, const Analytics::EventType& rh)
{
    return lh.eventTypeId == rh.eventTypeId;
}

} // namespace api
} // namespace nx
