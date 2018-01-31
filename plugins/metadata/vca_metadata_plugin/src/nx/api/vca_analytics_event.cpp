#include "vca_analytics_event.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace api {

bool operator==(const VcaAnalyticsEventType& lh, const VcaAnalyticsEventType& rh)
{
    return lh.eventTypeId == rh.eventTypeId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VcaAnalyticsEventType, (json), VcaAnalyticsEventType_Fields, (brief, true))

} // namespace api
} // namespace nx