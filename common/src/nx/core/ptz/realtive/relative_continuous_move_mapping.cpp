#include "relative_continuous_move_mapping.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Speed, (json), PtzSpeed_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccelerationParameters,
    (json),
    PtzAccelerationParameters_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RelativeContinuousMoveComponentMapping,
    (json),
    RelativeContinuousMoveComponentMapping_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RelativeContinuousMoveMapping,
        (json),
        RelativeContinuousMoveMapping_Fields);

RelativeContinuousMoveComponentMapping RelativeContinuousMoveMapping::componentMapping(
    Component component) const
{
    switch (component)
    {
        case Component::pan:
            return pan;
        case Component::tilt:
            return tilt;
        case Component::rotation:
            return rotation;
        case Component::zoom:
            return zoom;
        case Component::focus:
            return focus;
        default:
            NX_ASSERT(false, lit("Wrong component. We should never be here."));
            return RelativeContinuousMoveComponentMapping();
    }
}

} // namespace ptz
} // namespace core
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, AccelerationType,
    (nx::core::ptz::AccelerationType::linear, "linear")
);
