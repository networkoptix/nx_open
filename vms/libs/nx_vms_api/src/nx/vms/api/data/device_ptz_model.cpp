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

} // namespace nx::vms::api
