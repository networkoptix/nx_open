// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

#include "id_data.h"

namespace nx::vms::api {

enum class PtzApiType
{
    none,
    operational,
    configurational,
    any,
};

NX_REFLECTION_ENUM_CLASS(PtzPositionType,

    /**%apidoc Absolute position in the range defined by the Device. */
    absolute,

    /**%apidoc Logical position in the range -180 to 180. */
    logical
);

struct NX_VMS_API PtzPositionFilter
{
    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QnUuid deviceId;

    /**%apidoc[opt] Type of the position to be returned. */
    PtzPositionType type = PtzPositionType::absolute;

    /**%apidoc[opt] API type to use. */
    PtzApiType api = PtzApiType::operational;

    const PtzPositionFilter& getId() const { return *this; }
    bool operator==(const PtzPositionFilter&) const = default;
    QString toString() const;
};
#define PtzPositionFilter_Fields (deviceId)(type)
QN_FUSION_DECLARE_FUNCTIONS(PtzPositionFilter, (json), NX_VMS_API)

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

struct NX_VMS_API PtzPresetFilter
{
    /**%apidoc[immutable] */
    QString id;

    /**%apidoc:string
     * Device id (can be obtained from "id", "physicalId" or "logicalId"
     * field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     */
    QnUuid deviceId;

    const PtzPresetFilter& getId() const { return *this; }
    bool operator==(const PtzPresetFilter&) const = default;
    QString toString() const;
    bool isNull() const { return id.isEmpty(); }
};
#define PtzPresetFilter_Fields (deviceId)(id)
QN_FUSION_DECLARE_FUNCTIONS(PtzPresetFilter, (json), NX_VMS_API)

struct NX_VMS_API PtzPreset: PtzPresetFilter
{
    /**%apidoc Public preset name. */
    QString name;
};
#define PtzPreset_Fields PtzPresetFilter_Fields (name)
QN_FUSION_DECLARE_FUNCTIONS(PtzPreset, (json), NX_VMS_API)

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

struct NX_VMS_API PtzTour: PtzPreset
{
    std::vector<PtzTourSpot> spots;
};
#define PtzTour_Fields PtzPreset_Fields (spots)
QN_FUSION_DECLARE_FUNCTIONS(PtzTour, (json), NX_VMS_API)

} // namespace nx::vms::api
