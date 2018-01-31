#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include "nx/api/vca_analytics_event.h"

namespace nx {
namespace api {

/**
* Description of the avc analytics driver, which can generate different events.
*/
struct VcaAnalyticsDriverManifest: AnalyticsDriverManifestBase
{
   QList<VcaAnalyticsEventType> outputEventTypes;
};
#define VcaAnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(VcaAnalyticsDriverManifest, (json))

} // namespace api
} // namespace nx
