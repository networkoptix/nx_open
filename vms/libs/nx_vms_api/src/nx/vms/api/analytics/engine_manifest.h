#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QJsonObject>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/pixel_format.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

/**
 * The JSON-serializable data structure that is given by each Analytics Plugin's Engine to the
 * Server after the Engine has been created by the Plugin.
 */
struct NX_VMS_API EngineManifest
{
    /**
     * Declaration of an Engine ObjectAction - the user may select an analytics Object (e.g.
     * as a context action for the object rectangle on a video frame), and choose an ObjectAction
     * to trigger from the list of all compatible ObjectActions from all Engines.
     */
    struct ObjectAction
    {
        enum Capability
        {
            noCapabilities = 0,
            needBestShotVideoFrame = 1 << 0,
            needBestShotObjectMetadata = 1 << 1,
            needTrack = 1 << 2,
        };
        Q_DECLARE_FLAGS(Capabilities, Capability);

        struct Requirements
        {
            Capabilities capabilities;
            PixelFormat bestShotVideoFramePixelFormat;
        };
        #define nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields \
            (capabilities)(bestShotVideoFramePixelFormat)

        QString id; /**< Id of the action type, like "vendor.pluginName.actionName". */
        QString name; /**< Action name to be shown to the user. */
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
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    Capabilities capabilities;

    /** Types of Events that can potentially be produced by any DeviceAgent of this Engine. */
    QList<EventType> eventTypes;

    /** Types of Objects that can potentially be produced by any DeviceAgent of this Engine. */
    QList<ObjectType> objectTypes;

    /** Groups that are used to group Object and Event types declared by this manifest. */
    QList<Group> groups;

    QList<ObjectAction> objectActions;

    QJsonObject deviceAgentSettingsModel;
};
#define EngineManifest_Fields \
    (capabilities)(eventTypes)(objectTypes)(objectActions)(groups)(deviceAgentSettingsModel)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest, (json), NX_VMS_API)
Q_DECLARE_OPERATORS_FOR_FLAGS(EngineManifest::Capabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(EngineManifest::ObjectAction::Capabilities)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EngineManifest::Capability)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EngineManifest::ObjectAction::Capability)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction, (json), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(EngineManifest::ObjectAction::Requirements, (json)(eq), NX_VMS_API)

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capability,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::Capabilities,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::ObjectAction::Capability,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::EngineManifest::ObjectAction::Capabilities,
    (metatype)(numeric)(lexical), NX_VMS_API)
