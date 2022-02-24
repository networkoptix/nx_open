// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "override.h"

#include <nx/fusion/model_functions.h>

#include "type.h"

namespace nx::vms::common {
namespace ptz {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OverridePart, (json), PtzOverridePart_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Override, (json), PtzOverride_Fields)

const QString Override::kPtzOverrideKey = "ptzOverride";

OverridePart Override::overrideByType(Type ptzType) const
{
    switch (ptzType)
    {
        case Type::operational:
            return operational;
        case Type::configurational:
            return configurational;
        default:
            return OverridePart();
    }
}

} // namespace ptz
} // namespace nx::vms::common
