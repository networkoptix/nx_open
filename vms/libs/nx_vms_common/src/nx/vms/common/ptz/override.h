// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/ptz_constants.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/json/qt_core_types.h>

#include "types_fwd.h"

namespace nx::vms::common {
namespace ptz {

struct OverridePart
{
    // Intentionally not serializable since fusion doesn't have a way to properly
    // handle std::optional fields.
    std::optional<Ptz::Capabilities> capabilitiesOverride;

    Ptz::Capabilities capabilitiesToAdd;
    Ptz::Capabilities capabilitiesToRemove;

    Qt::Orientations flip;
    Ptz::Traits traits;
};
#define PtzOverridePart_Fields (capabilitiesToAdd)\
    (capabilitiesToRemove)\
    (flip)\
    (traits)

QN_FUSION_DECLARE_FUNCTIONS(OverridePart, (json))

struct NX_VMS_COMMON_API Override
{
    static const QString kPtzOverrideKey;

    // This struct exists only because enums are not supported as map keys by fusion.
    OverridePart operational;
    OverridePart configurational;

    OverridePart overrideByType(Type ptzType) const;
};
#define PtzOverride_Fields (operational)(configurational)

QN_FUSION_DECLARE_FUNCTIONS(Override, (json))

} // namespace ptz
} // namespace nx::vms::common
