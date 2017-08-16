#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/api/analytics/analytics_event.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/**
* Description of the analytics driver, which can generate different events.
*/
struct AnalyticsDriverManifest
{
   QnUuid driverId;
   TranslatableString driverName;
   QStringList acceptedDataTypes;
   QStringList supportedCodecs;
   QStringList supportedHandleTypes;
   QStringList supportedPixelFormats;
   QList<AnalyticsEventType> outputEventTypes;
};
#define AnalyticsDriverManifest_Fields (driverId)(driverName)(acceptedDataTypes)(supportedCodecs)\
    (supportedHandleTypes)(supportedPixelFormats)(outputEventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json)(metatype))

} // namespace api
} // namespace nx

