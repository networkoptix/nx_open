// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/json/qjson.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/manifest_error.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/object_action.h>
#include <nx/vms/api/analytics/stream_type.h>
#include <nx/vms/api/analytics/type_library.h>
#include <nx/vms/api/types/motion_types.h>

namespace nx::vms::api::analytics {

NX_REFLECTION_ENUM_CLASS(EngineCapability,
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
Q_DECLARE_FLAGS(EngineCapabilities, EngineCapability)

/**%apidoc
 * The data structure that is given by each Analytics Integration's Engine to the Server after the
 * Engine has been created by the Integration.
 * <br/>
 * See the description of the fields in `src/nx/sdk/analytics/manifests.md` in the SDK.
 */
struct NX_VMS_API EngineManifest
{
    /**%apidoc[opt] */
    EngineCapabilities capabilities;

    /**%apidoc[opt] */
    StreamTypes streamTypeFilter;

    /**%apidoc[opt] */
    StreamIndex preferredStream = StreamIndex::undefined;

    /**%apidoc[opt]
     * %deprecated Use "typeLibrary" section instead.
     */
    QList<AnalyticsEventType> eventTypes;

    /**%apidoc[opt]
     * %deprecated Use "typeLibrary" section instead.
     */
    QList<ObjectType> objectTypes;

    /**%apidoc[opt]
     * %deprecated Use "typeLibrary" section instead.
     */
    QList<Group> groups;

    /**%apidoc[opt] */
    QList<ObjectAction> objectActions;

    /**%apidoc[opt] */
    QJsonObject deviceAgentSettingsModel;

    /**%apidoc[opt] */
    TypeLibrary typeLibrary;

    bool operator==(const EngineManifest& other) const = default;
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
Q_DECLARE_OPERATORS_FOR_FLAGS(EngineCapabilities)

NX_REFLECTION_INSTRUMENT(EngineManifest, EngineManifest_Fields);

NX_VMS_API std::vector<ManifestError> validate(const EngineManifest& manifest);

} // namespace nx::vms::api::analytics
