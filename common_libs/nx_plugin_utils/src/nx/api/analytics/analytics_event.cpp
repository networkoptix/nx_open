#include "analytics_event.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::api::Analytics, EventTypeFlag)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::api::Analytics, EventTypeFlags)

namespace nx {
namespace api {

bool operator==(const Analytics::EventType& lh, const Analytics::EventType& rh)
{
    return lh.typeId == rh.typeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Analytics::EventType,
    (json),
    AnalyticsEventType_Fields,
    (brief, true))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Analytics::Group,
    (json),
    AnalyticsEventGroup_Fields,
    (brief, true))

} // namespace api
} // namespace nx
