#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/api/analytics/event_type.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

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

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::api::AnalyticsDriverManifest, (json)(metatype))
