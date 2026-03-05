// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

#include "id_data.h"

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(PtzApiType,
    none,
    operational, //< Standard runtime PTZ control.
    configurational, //< Alternative PTZ control mode (may have different coordinate ranges).
    any
);

NX_REFLECTION_ENUM_CLASS(PtzPositionType,

    /**%apidoc Absolute position in the range defined by the Device. */
    absolute,

    /**%apidoc Logical position in the range -180 to 180. */
    logical
);

struct NX_VMS_API PtzDeviceId
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;
};
#define PtzDeviceId_Fields (deviceId)
QN_FUSION_DECLARE_FUNCTIONS(PtzDeviceId, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzDeviceId, PtzDeviceId_Fields)

struct NX_VMS_API PtzPositionFilter
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc[opt] Type of the position to be returned. */
    PtzPositionType type = PtzPositionType::absolute;

    /**%apidoc[opt] API type to use. */
    PtzApiType api = PtzApiType::operational;

    const PtzPositionFilter& getId() const { return *this; }
    bool operator==(const PtzPositionFilter&) const = default;
    QString toString() const;
};
#define PtzPositionFilter_Fields (deviceId)(type)(api)
QN_FUSION_DECLARE_FUNCTIONS(PtzPositionFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzPositionFilter, PtzPositionFilter_Fields)

struct NX_VMS_API PtzPosition: PtzPositionFilter
{
    /**%apidoc
     * A position on the X-axis. Range depends on the `type` parameter. In the case of an absolute
     * position, the range is defined by the Device. In the case of a logical position, the range
     * is -180 to +180.
     * %example 1.0
     */
    std::optional<float> pan = std::nullopt;

    /**%apidoc
     * A position on the Y-axis. Range depends on the `type` parameter. In the case of an absolute
     * position, the range is defined by the Device. In the case of a logical position, the range
     * is -180 to +180.
     * %example 1.0
     */
    std::optional<float> tilt = std::nullopt;

    /**%apidoc
     * A position on the Z-axis. Range depends on the `type` parameter. In the case of an absolute
     * position, the range is defined by the Device. In the case of a logical position, the range
     * is 0 to +180.
     * %example 1.0
     */
    std::optional<float> zoom = std::nullopt;

    /**%apidoc
     * Movement speed.
     * %example 0.1
     */
    float speed = 0.f;
};
#define PtzPosition_Fields PtzPositionFilter_Fields (pan)(tilt)(zoom)(speed)
QN_FUSION_DECLARE_FUNCTIONS(PtzPosition, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzPosition, PtzPosition_Fields)

struct NX_VMS_API MinMaxLimit
{
    /**%apidoc Minimum value. */
    qreal min = 0.0;

    /**%apidoc Maximum value. */
    qreal max = 0.0;
};
#define MinMaxLimit_Fields (min)(max)
QN_FUSION_DECLARE_FUNCTIONS(MinMaxLimit, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(MinMaxLimit, MinMaxLimit_Fields)

struct NX_VMS_API PtzPositionLimits
{
    /**%apidoc Pan limits. */
    MinMaxLimit pan;

    /**%apidoc Tilt limits. */
    MinMaxLimit tilt;

    /**%apidoc Field of View (Zoom) limits. */
    MinMaxLimit fov;

    /**%apidoc Rotation limits. */
    MinMaxLimit rotation;

    /**%apidoc Focus limits. */
    MinMaxLimit focus;

    /**%apidoc Pan speed limits. */
    MinMaxLimit panSpeed;

    /**%apidoc Tilt speed limits. */
    MinMaxLimit tiltSpeed;

    /**%apidoc Zoom speed limits. */
    MinMaxLimit zoomSpeed;

    /**%apidoc Rotation speed limits. */
    MinMaxLimit rotationSpeed;

    /**%apidoc Focus speed limits. */
    MinMaxLimit focusSpeed;
};
#define PtzPositionLimits_Fields (pan)(tilt)(fov)(rotation)(focus)\
    (panSpeed)(tiltSpeed)(zoomSpeed)(rotationSpeed)(focusSpeed)
QN_FUSION_DECLARE_FUNCTIONS(PtzPositionLimits, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzPositionLimits, PtzPositionLimits_Fields)

struct NX_VMS_API PtzMovement: PtzPosition
{
    /**%apidoc
     * Direction and speed of the focus action (-1.0 to 1.0).
     * %example -1.0
     */
    std::optional<float> focus = std::nullopt;
};
#define PtzMovement_Fields PtzPosition_Fields (focus)
QN_FUSION_DECLARE_FUNCTIONS(PtzMovement, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzMovement, PtzMovement_Fields)

struct NX_VMS_API PtzViewportMove
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc
     * Left edge of the viewport rectangle [0.0 to 1.0].
     * %example 0.0
     */
    float viewportLeft = 0.f;

    /**%apidoc
     * Top edge of the viewport rectangle [0.0 to 1.0].
     * %example 0.0
     */
    float viewportTop = 0.f;

    /**%apidoc
     * Right edge of the viewport rectangle [0.0 to 1.0].
     * %example 1.0
     */
    float viewportRight = 0.f;

    /**%apidoc
     * Bottom edge of the viewport rectangle [0.0 to 1.0].
     * %example 1.0
     */
    float viewportBottom = 0.f;

    /**%apidoc
     * Aspect ratio of the current view.
     * %example 1.777
     */
    float aspectRatio = 0.f;

    /**%apidoc[opt]
     * Movement speed [0.0 to 1.0].
     * %example 1.0
     */
    float speed = 1.f;

    /**%apidoc[opt] API type to use. */
    PtzApiType api = PtzApiType::operational;
};
#define PtzViewportMove_Fields (deviceId)(viewportLeft)(viewportTop)(viewportRight)\
    (viewportBottom)(aspectRatio)(speed)(api)
QN_FUSION_DECLARE_FUNCTIONS(PtzViewportMove, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzViewportMove, PtzViewportMove_Fields)

struct NX_VMS_API PtzPresetFilter
{
    /**%apidoc[immutable] */
    QString id;

    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    const PtzPresetFilter& getId() const { return *this; }
    bool operator==(const PtzPresetFilter&) const = default;
    QString toString() const;
    bool isNull() const { return id.isEmpty(); }
};
#define PtzPresetFilter_Fields (deviceId)(id)
QN_FUSION_DECLARE_FUNCTIONS(PtzPresetFilter, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzPresetFilter, PtzPresetFilter_Fields)

struct NX_VMS_API PtzPreset: PtzPresetFilter
{
    /**%apidoc Public preset name. */
    QString name;
};
#define PtzPreset_Fields PtzPresetFilter_Fields (name)
QN_FUSION_DECLARE_FUNCTIONS(PtzPreset, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzPreset, PtzPreset_Fields)

struct NX_VMS_API PtzPresetActivation: PtzPresetFilter
{
    /**%apidoc
     * Movement speed (0 to 1.0).
     * %example 0.1
     */
    float speed = 0.0;
};
#define PtzPresetActivation_Fields PtzPresetFilter_Fields (speed)
QN_FUSION_DECLARE_FUNCTIONS(PtzPresetActivation, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzPresetActivation, PtzPresetActivation_Fields)

struct NX_VMS_API PtzTourSpot
{
    /**%apidoc The id of the PTZ preset in the PTZ tour. */
    QString presetId;

    /**%apidoc[opt]
     * The amount of time in milliseconds to stay on the PTZ preset.
     * %example 1000
     */
    std::chrono::milliseconds stayTimeMs{0};

    /**%apidoc
     * Movement speed of the Device during the Tour.
     * %example 0.1
     */
    float speed = 0;
};
#define PtzTourSpot_Fields (presetId)(stayTimeMs)(speed)
QN_FUSION_DECLARE_FUNCTIONS(PtzTourSpot, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzTourSpot, PtzTourSpot_Fields)

struct NX_VMS_API PtzTour: PtzPreset
{
    std::vector<PtzTourSpot> spots;
};
#define PtzTour_Fields PtzPreset_Fields (spots)
QN_FUSION_DECLARE_FUNCTIONS(PtzTour, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzTour, PtzTour_Fields)

NX_REFLECTION_ENUM_CLASS(PtzObjectType,
    preset, //< PTZ preset.
    tour //< PTZ tour.
);

struct NX_VMS_API PtzObject
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc Object type. */
    PtzObjectType type = PtzObjectType::preset;

    /**%apidoc Preset or tour id. */
    QString objectId;
};
#define PtzObject_Fields (deviceId)(type)(objectId)
QN_FUSION_DECLARE_FUNCTIONS(PtzObject, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzObject, PtzObject_Fields)

struct NX_VMS_API PtzAuxiliaryCommand
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    nx::Uuid deviceId;

    /**%apidoc
     * Auxiliary trait name to execute. One of the traits from
     * GET /rest/v{4-}/devices/{deviceId}/ptz/aux.
     */
    QString trait;

    /**%apidoc[opt] Command data (trait-specific). */
    QString data;

    /**%apidoc[opt] API type to use. */
    PtzApiType api = PtzApiType::operational;
};
#define PtzAuxiliaryCommand_Fields (deviceId)(trait)(data)(api)
QN_FUSION_DECLARE_FUNCTIONS(PtzAuxiliaryCommand, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(PtzAuxiliaryCommand, PtzAuxiliaryCommand_Fields)

} // namespace nx::vms::api
