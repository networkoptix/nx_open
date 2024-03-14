// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/manifest_error.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/pixel_format.h>
#include <nx/vms/api/analytics/stream_type.h>
#include <nx/vms/api/analytics/type_library.h>
#include <nx/vms/api/types/motion_types.h>

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
        NX_REFLECTION_ENUM_IN_CLASS(Capability,
            noCapabilities = 0,
            needBestShotVideoFrame = 1 << 0,
            needBestShotObjectMetadata = 1 << 1,
            needFullTrack = 1 << 2,
            needBestShotImage = 1 << 3
        )
        Q_DECLARE_FLAGS(Capabilities, Capability)

        struct Requirements
        {
            Capabilities capabilities;

            // TODO: Investigate what happens if the Manifest is missing the value for this field -
            // it seems it will remain PixelFormat::undefined and an assertion will fail in
            // apiToAvPixelFormat().
            PixelFormat bestShotVideoFramePixelFormat;

            bool operator==(const Requirements& other) const = default;
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
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Capability,
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
        noAutoBestShots = 1 << 9
    )
    Q_DECLARE_FLAGS(Capabilities, Capability)

    Capabilities capabilities;
    StreamTypes streamTypeFilter;
    StreamIndex preferredStream = StreamIndex::undefined;

    /**%apidoc
     * %deprecated Use "typeLibrary" section instead.
     */
    QList<EventType> eventTypes;

    /**%apidoc
     * %deprecated Use "typeLibrary" section instead.
     */
    QList<ObjectType> objectTypes;

    /**%apidoc
     * %deprecated Use "typeLibrary" section instead.
     */
    QList<Group> groups;
    QList<ObjectAction> objectActions;
    QJsonObject deviceAgentSettingsModel;

    TypeLibrary typeLibrary;
};
#define EngineManifest_Fields \
    (capabilities) \
    (streamTypeFilter) \
    (preferredStream) \
    (eventTypes) \
    (objectTypes) \
    (objectActions) \
    (deviceAgentSettingsModel) \
    (groups) \
    (typeLibrary)

QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(EngineManifest::ObjectAction::Requirements, nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields);
NX_REFLECTION_INSTRUMENT(EngineManifest::ObjectAction, ObjectAction_Fields);
NX_REFLECTION_INSTRUMENT(EngineManifest, EngineManifest_Fields);

// Needed for struct ActionTypeDescriptor
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction::Requirements, (json), NX_VMS_API)

NX_VMS_API std::vector<ManifestError> validate(const EngineManifest& manifest);

} // namespace nx::vms::api::analytics
