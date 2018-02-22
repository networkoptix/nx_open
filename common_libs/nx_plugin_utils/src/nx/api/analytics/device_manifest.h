#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

struct /*NX_PLUGIN_UTILS_API*/ AnalyticsDeviceManifest
{
    QList<QnUuid> supportedEventTypes;
};

#define AnalyticsDeviceManifest_Fields (supportedEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDeviceManifest, (json))

} // namespace api
} // namespace nx
