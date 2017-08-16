#include "analytics_event.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEventTypeId, (eq)(hash), AnalyticsEventTypeId_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(AnalyticsApiEventTypes, (json), _Fields, (brief, true))

AnalyticsEventTypeId::AnalyticsEventTypeId(const QnUuid& driverId, const QnUuid& eventTypeId):
    driverId(driverId),
    eventTypeId(eventTypeId)
{
}

} // namespace api
} // namespace nx
