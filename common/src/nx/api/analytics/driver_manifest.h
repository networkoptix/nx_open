#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

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

#define AnalyticsApiTypes (AnalyticsEventType)(AnalyticsDriverManifest)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(AnalyticsApiTypes, (json)(metatype))

} // namespace api
} // namespace nx

