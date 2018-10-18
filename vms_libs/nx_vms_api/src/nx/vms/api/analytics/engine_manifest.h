#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/vms/api/analytics/translatable_string.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API EngineManifest
{
    Q_GADGET
    Q_ENUMS(Capability)
    Q_FLAGS(Capabilities)

public: //< Required for Qt MOC run.

    struct ObjectAction
    {
        QString id;
        TranslatableString name;
        QList<QString> supportedObjectTypeIds;
        // TODO: Add params (settings).
    };
    #define ObjectAction_Fields (id)(name)(supportedObjectTypeIds)

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
    Capabilities capabilities;

    QList<EventType> eventTypes;
    QList<ObjectType> objectTypes;
    QList<Group> groups;

    QList<ObjectAction> objectActions;

    // TODO: Add DeviceAgent settings and Engine settings.
};
#define EngineManifest_Fields \
    (pluginId)(pluginName)(capabilities) \
    (eventTypes)(objectTypes)(objectActions)(groups)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json), NX_VMS_API)
Q_DECLARE_OPERATORS_FOR_FLAGS(EngineManifest::Capabilities)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EngineManifest::Capability)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capability,
    (metatype)(numeric)(lexical),
    NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capabilities,
    (metatype)(numeric)(lexical),
    NX_VMS_API)
