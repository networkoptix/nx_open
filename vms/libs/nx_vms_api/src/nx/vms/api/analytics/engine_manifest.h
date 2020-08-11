#pragma once

#include <QtCore/QList>
#include <QtCore/QJsonObject>

#include <nx/vms/api/analytics/manifest_error.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/pixel_format.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/analytics/stream_type.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

// TODO: Move this class to a server-only library; eliminate its usages in the Client.
/**
 * The JSON-serializable data structure that is given by each Analytics Plugin's Engine to the
 * Server after the Engine has been created by the Plugin.
 *
 * See the description of the fields in manifests.md.
 */
struct NX_VMS_API EngineManifest
{
    struct ObjectAction
    {
        enum Capability
        {
            noCapabilities = 0,
            needBestShotVideoFrame = 1 << 0,
            needBestShotObjectMetadata = 1 << 1,
            needFullTrack = 1 << 2,
            needBestShotImage = 1 << 3,
        };
        Q_DECLARE_FLAGS(Capabilities, Capability);

        struct Requirements
        {
            Capabilities capabilities;

            // TODO: Investigate what happens if the Manifest is missing the value for this field -
            // it seems it will remain PixelFormat::undefined and an assertion will fail in
            // apiToAvPixelFormat().
            PixelFormat bestShotVideoFramePixelFormat;
        };
        #define nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields \
            (capabilities)(bestShotVideoFramePixelFormat)

        QString id;
        QString name;

        QList<QString> supportedObjectTypeIds;
        QJsonObject parametersModel;
        Requirements requirements;
    };
    #define ObjectAction_Fields (id)(name)(supportedObjectTypeIds)(parametersModel)\
        (requirements)

    // TODO: #dmishin replace it with the PixelFormat enum.
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
        deviceDependent = 1 << 7,
        keepObjectBoundingBoxRotation = 1 << 8,
        noAutoBestShots = 1 << 9,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    Capabilities capabilities;
    StreamTypes streamTypeFilter;
    StreamIndex preferredStream = StreamIndex::undefined;
    QList<EventType> eventTypes;
    QList<ObjectType> objectTypes;
    QList<Group> groups;
    QList<ObjectAction> objectActions;
    QJsonObject deviceAgentSettingsModel;
};
#define EngineManifest_Fields \
    (capabilities) \
    (streamTypeFilter) \
    (preferredStream) \
    (eventTypes) \
    (objectTypes) \
    (objectActions) \
    (deviceAgentSettingsModel) \
    (groups)

QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json), NX_VMS_API)
Q_DECLARE_OPERATORS_FOR_FLAGS(EngineManifest::Capabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(EngineManifest::ObjectAction::Capabilities)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EngineManifest::Capability)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EngineManifest::ObjectAction::Capability)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction::Requirements, (json)(eq), NX_VMS_API)

NX_VMS_API std::vector<ManifestError> validate(const EngineManifest& manifest);

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capability,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capabilities,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::ObjectAction::Capability,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::ObjectAction::Capabilities,
    (metatype)(numeric)(lexical), NX_VMS_API)
