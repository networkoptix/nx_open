#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/api/analytics/analytics_event.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

// TODO: Rename all classes replacing "driver" with "plugin".
/**
 * Description of the analytics driver, which can generate different events.
 */
struct AnalyticsDriverManifestBase
{
    Q_GADGET
    Q_ENUMS(Capability)
    Q_FLAGS(Capabilities)

public:
    enum Capability
    {
        noCapabilities = 0,
        needDeepCopyForMediaFrame = 1 << 0,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    QnUuid driverId;
    TranslatableString driverName;
    QStringList acceptedDataTypes;
    QStringList supportedCodecs;
    QStringList supportedHandleTypes;
    QStringList supportedPixelFormats;
    Capabilities capabilities;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AnalyticsDriverManifestBase::Capabilities)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AnalyticsDriverManifestBase::Capability)

#define AnalyticsDriverManifestBase_Fields (driverId)(driverName)(acceptedDataTypes)(supportedCodecs)\
    (supportedHandleTypes)(supportedPixelFormats)(capabilities)

// TODO: #mike: Eliminate the class.
struct AnalyticsDriverManifest: AnalyticsDriverManifestBase
{
   QList<Analytics::EventType> outputEventTypes;
   QList<Analytics::EventType> outputObjectTypes;
};
#define AnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)(outputObjectTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

} // namespace api
} // namespace nx
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::api::AnalyticsDriverManifestBase::Capability)
    (nx::api::AnalyticsDriverManifestBase::Capabilities),
    (metatype)(numeric)(lexical)
)
