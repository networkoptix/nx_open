#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/api/analytics/analytics_event.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/** Description of the analytics driver, which can generate different events. */
struct NX_PLUGIN_UTILS_API AnalyticsDriverManifestBase
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

#define AnalyticsDriverManifestBase_Fields (driverId)(driverName)(acceptedDataTypes)\
    (supportedCodecs)(supportedHandleTypes)(supportedPixelFormats)(capabilities)

struct NX_PLUGIN_UTILS_API AnalyticsDriverManifest: AnalyticsDriverManifestBase
{
   QList<Analytics::EventType> outputEventTypes;
   QList<Analytics::Group> groups;

};
#define AnalyticsDriverManifest_Fields AnalyticsDriverManifestBase_Fields (outputEventTypes)(groups)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsDriverManifest, (json))

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
(nx::api::AnalyticsDriverManifestBase::Capability)
(nx::api::AnalyticsDriverManifestBase::Capabilities),
(metatype)(numeric)(lexical)
)
