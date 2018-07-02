#include "relative_continuous_move_mapping.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

namespace {

static const double kDefaultSpeedValue = 1.0;

} // namespace

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Speed, (json)(eq), PtzSpeed_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AccelerationParameters,
    (json)(eq),
    PtzAccelerationParameters_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RelativeContinuousMoveComponentMapping,
    (json)(eq),
    RelativeContinuousMoveComponentMapping_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    RelativeContinuousMoveMapping,
    (json)(eq),
    RelativeContinuousMoveMapping_Fields);

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    SimpleRelativeContinuousMoveMapping,
    (json)(eq),
    SimpleRelativeContinuousMoveMapping_Fields);

RelativeContinuousMoveComponentMapping::RelativeContinuousMoveComponentMapping(const Speed& speed):
    workingSpeed(speed)
{
}

RelativeContinuousMoveMapping::RelativeContinuousMoveMapping(
    const SimpleRelativeContinuousMoveMapping& mapping)
    :
    pan({kDefaultSpeedValue, mapping.panCycleDuration}),
    tilt({kDefaultSpeedValue, mapping.tiltCycleDuration}),
    rotation({kDefaultSpeedValue, mapping.rotationCycleDuration}),
    zoom({kDefaultSpeedValue, mapping.zoomCycleDuration}),
    focus({kDefaultSpeedValue, mapping.focusCycleDuration})
{
}

RelativeContinuousMoveComponentMapping RelativeContinuousMoveMapping::componentMapping(
    Component component) const
{
    switch (component)
    {
        case Component::pan: return pan;
        case Component::tilt: return tilt;
        case Component::rotation: return rotation;
        case Component::zoom: return zoom;
        case Component::focus: return focus;
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
