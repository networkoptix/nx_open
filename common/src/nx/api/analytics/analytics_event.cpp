#include "analytics_event.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

bool operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh)
{
    return lh.eventTypeId == rh.eventTypeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AnalyticsEventType, (json), AnalyticsEventType_Fields, (brief, true))

} // namespace api
} // namespace nx