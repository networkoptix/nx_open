#include "override.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OverridePart, (json), PtzOverridePart_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Override, (json), PtzOverride_Fields)

const QString Override::kPtzOverrideKey = "ptzOverride";

OverridePart Override::overrideByType(nx::core::ptz::Type ptzType) const
{
    switch (ptzType)
    {
        case nx::core::ptz::Type::operational:
            return operational;
        case nx::core::ptz::Type::configurational:
            return configurational;
        default:
            return OverridePart();
    }
}

} // namespace ptz
} // namespace core
} // namespace nx
