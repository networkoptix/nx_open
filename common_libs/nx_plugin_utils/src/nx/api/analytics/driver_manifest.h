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
struct /*NX_PLUGIN_UTILS_API*/ AnalyticsDriverManifestBase
{
    Q_GADGET
    Q_ENUMS(Capability)
    Q_FLAGS(Capabilities)

public:
    enum Capability
    {
        noCapabilities = 0,
        needDeepCopyOfVideoFrames = 1 << 0,
        needUncompressedVideoFrames = 1 << 1,
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

struct AnalyticsManifestObjectAction
{
    QString id;
    TranslatableString name;
    QList<QnUuid> supportedObjectTypeIds;
    // TODO: Add settings.
};
#define AnalyticsManifestObjectAction_Fields \
    (id)(name)(supportedObjectTypeIds)

struct AnalyticsDriverManifest: AnalyticsDriverManifestBase
{
    QList<Analytics::EventType> outputEventTypes;
    QList<Analytics::EventType> outputObjectTypes;
    QList<AnalyticsManifestObjectAction> objectActions;
    // TODO: #mike: Add other fields, see stub_metadata_plugin.
};
#define AnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields \
    (outputEventTypes)(outputObjectTypes)(objectActions)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::api::AnalyticsDriverManifestBase::Capability)
    (nx::api::AnalyticsDriverManifestBase::Capabilities),
    (metatype)(numeric)(lexical)
)
