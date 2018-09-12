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
// TODO: Consider changing namespaces.
// TODO: Rename to MetadataPluginManifestBase or similar, considering namespaces.
/**
 * Description of the mediaserver metadata plugin.
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
        needUncompressedVideoFrames_yuv420 = 1 << 0,
        needUncompressedVideoFrames_argb = 1 << 1,
        needUncompressedVideoFrames_abgr = 1 << 2,
        needUncompressedVideoFrames_rgba = 1 << 3,
        needUncompressedVideoFrames_bgra = 1 << 4,
        needUncompressedVideoFrames_rgb = 1 << 5,
        needUncompressedVideoFrames_bgr = 1 << 6,
        cameraModelIndependent = 1 << 7,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    QString pluginId;
    TranslatableString pluginName;
    QStringList acceptedDataTypes;
    QStringList supportedCodecs;
    QStringList supportedHandleTypes;
    QStringList supportedPixelFormats;
    Capabilities capabilities;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AnalyticsDriverManifestBase::Capabilities)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AnalyticsDriverManifestBase::Capability)

#define AnalyticsDriverManifestBase_Fields (pluginId)(pluginName)(acceptedDataTypes)\
    (supportedCodecs)(supportedHandleTypes)(supportedPixelFormats)(capabilities)

struct AnalyticsManifestObjectAction
{
    QString id;
    TranslatableString name;
    QList<QString> supportedObjectTypeIds;
    // TODO: Add settings.
};
#define AnalyticsManifestObjectAction_Fields \
    (id)(name)(supportedObjectTypeIds)

struct AnalyticsDriverManifest: AnalyticsDriverManifestBase
{
    QList<Analytics::EventType> outputEventTypes;
    QList<Analytics::ObjectType> outputObjectTypes;
    QList<AnalyticsManifestObjectAction> objectActions;
    QList<Analytics::Group> groups;
    // TODO: #mike: Add other fields, see stub_metadata_plugin.
};
#define AnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields \
    (outputEventTypes)(outputObjectTypes)(objectActions)(groups)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::api::AnalyticsDriverManifestBase::Capability)
    (nx::api::AnalyticsDriverManifestBase::Capabilities),
    (metatype)(numeric)(lexical)
)
