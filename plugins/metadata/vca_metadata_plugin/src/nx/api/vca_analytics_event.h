#pragma once

#include <nx/api/common/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/**
 * Description of the vca analytics event.
 */
struct VcaAnalyticsEventType
{
    QnUuid eventTypeId;
    TranslatableString eventName;
    QString internalName; //< VCA event type name (this name is sent by vca tcp notification server)
};

bool operator==(const VcaAnalyticsEventType& lh, const VcaAnalyticsEventType& rh);

#define VcaAnalyticsEventType_Fields (eventTypeId)(eventName)(internalName)

QN_FUSION_DECLARE_FUNCTIONS(VcaAnalyticsEventType, (json))

} // namespace api
} // namespace nx
