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

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))

} // namespace api
} // namespace nx
