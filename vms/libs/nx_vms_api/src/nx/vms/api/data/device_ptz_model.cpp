// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_ptz_model.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzPositionFilter, (json), PtzPositionFilter_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzPosition, (json), PtzPosition_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MinMaxLimit, (json), MinMaxLimit_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzPositionLimits, (json), PtzPositionLimits_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzMovement, (json), PtzMovement_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzPresetFilter, (json), PtzPresetFilter_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzPreset, (json), PtzPreset_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzPresetActivation, (json), PtzPresetActivation_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzTourSpot, (json), PtzTourSpot_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzTour, (json), PtzTour_Fields)

QString PtzPositionFilter::toString() const
{
    return QJson::serialized(*this);
}

QString PtzPresetFilter::toString() const
{
    return QJson::serialized(*this);
}

qreal MinMaxLimit::range() const noexcept
{
    return max - min;
}

bool MinMaxLimit::between(qreal val) const noexcept
{
    return !(val < min) && !(max < val);
}

qreal MinMaxLimit::clamp(qreal val) const noexcept
{
    return std::clamp(val, min, max);
}

bool MinMaxLimit::operator==(const MinMaxLimit& other) const noexcept
{
    return qFuzzyCompare(min, other.min) && qFuzzyCompare(max, other.max);
}

PtzPositionLimits PtzPositionLimits::defaults() noexcept
{
    return PtzPositionLimits{
        .pan = MinMaxLimit{0.0, 360},
        .tilt = MinMaxLimit{-90, 90},
        .fov = MinMaxLimit{0.0, 360},
        .rotation = MinMaxLimit{0.0, 360},
        .focus = MinMaxLimit{0.0, 1.0},
        .panSpeed = MinMaxLimit{-1.0, 1.0},
        .tiltSpeed = MinMaxLimit{-1.0, 1.0},
        .zoomSpeed = MinMaxLimit{-1.0, 1.0},
        .rotationSpeed = MinMaxLimit{-1.0, 1.0},
        .focusSpeed = MinMaxLimit{-1.0, 1.0},
    };
}

} // namespace nx::vms::api
