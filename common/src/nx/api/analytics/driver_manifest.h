#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

struct AnalyticsEventType
{
    QnUuid eventId;
    TranslatableString eventName;
};
#define AnalyticsEventType_Fields (eventId)(eventName)

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

#define AnalyticsApiTypes (AnalyticsEventType)(AnalyticsDriverManifest)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(AnalyticsApiTypes, (json)(metatype))

} // namespace api
} // namespace nx

