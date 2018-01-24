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
    QnUuid typeId;
    TranslatableString name;
};

bool operator==(const AnalyticsEventType& lh, const AnalyticsEventType& rh);

#define AnalyticsEventType_Fields (typeId)(name)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsEventType, (json))

} // namespace api
} // namespace nx
