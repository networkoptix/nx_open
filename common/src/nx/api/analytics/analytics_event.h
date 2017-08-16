#pragma once

#include <nx/api/common/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/**
 * Description of the analytics event.
 */
struct AnalyticsEventType
{
    QnUuid eventTypeId;
    TranslatableString eventName;
};
#define AnalyticsEventType_Fields (eventTypeId)(eventName)

/**
 * Description of the analytics event with it's driver reference.
 */
struct AnalyticsEventTypeWithRef: AnalyticsEventType
{
    QnUuid driverId;
    TranslatableString driverName;
};

/**
 * Description of the single analytics event type.
 */
struct AnalyticsEventTypeId
{
    AnalyticsEventTypeId() = default;
    AnalyticsEventTypeId(const QnUuid& driverId, const QnUuid& eventTypeId);
    QnUuid driverId;
    QnUuid eventTypeId;
};
#define AnalyticsEventTypeId_Fields (driverId)(eventTypeId)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventTypeId, (eq)(hash))

#define AnalyticsApiEventTypes (AnalyticsEventType)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(AnalyticsApiEventTypes, (json)(metatype))

} // namespace api
} // namespace nx
