#pragma once

#include <array>
#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/std/optional.h>

#include <nx/core/ptz/component.h>

namespace nx {
namespace core {
namespace ptz {

struct SimpleRelativeContinuousMoveMapping
{
    std::chrono::milliseconds panCycleDuration{ 0 };
    std::chrono::milliseconds tiltCycleDuration{ 0 };
    std::chrono::milliseconds rotationCycleDuration{ 0 };
    std::chrono::milliseconds zoomCycleDuration{ 0 };
    std::chrono::milliseconds focusCycleDuration{ 0 };
};
#define SimpleRelativeContinuousMoveMapping_Fields \
    (panCycleDuration)\
    (tiltCycleDuration)\
    (rotationCycleDuration)\
    (zoomCycleDuration)\
    (focusCycleDuration)

struct Speed
{
    // The value that should be passed to the underlying controller.
    double absoluteValue{1.0};

    // Time to make full turn (or get from one edge point to the other)
    // on the 'absoluteValue' speed.
    std::chrono::milliseconds cycleDuration{0};
};
#define PtzSpeed_Fields (absoluteValue)(cycleDuration)

enum class AccelerationType
{
    linear,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(AccelerationType);

struct AccelerationParameters
{
    AccelerationType accelerationType{AccelerationType::linear};
    std::chrono::milliseconds accelerationTime{0};
};
#define PtzAccelerationParameters_Fields (accelerationType)(accelerationTime)

struct RelativeContinuousMoveComponentMapping
{
    RelativeContinuousMoveComponentMapping() = default;
    RelativeContinuousMoveComponentMapping(const Speed& speed);

    Speed workingSpeed;
    AccelerationParameters accelerationParameters;
    AccelerationParameters decelerationParameters;
};
#define RelativeContinuousMoveComponentMapping_Fields (workingSpeed)\
    (accelerationParameters)\
    (decelerationParameters)

struct RelativeContinuousMoveMapping
{
    RelativeContinuousMoveMapping() = default;
    RelativeContinuousMoveMapping(const SimpleRelativeContinuousMoveMapping& mapping);
    RelativeContinuousMoveMapping(const std::vector<Speed>& componentSpeed);

    RelativeContinuousMoveComponentMapping pan;
    RelativeContinuousMoveComponentMapping tilt;
    RelativeContinuousMoveComponentMapping rotation;
    RelativeContinuousMoveComponentMapping zoom;
    RelativeContinuousMoveComponentMapping focus;

    RelativeContinuousMoveComponentMapping componentMapping(Component component) const;
};
#define RelativeContinuousMoveMapping_Fields (pan)(tilt)(rotation)(zoom)(focus)

#define CONTINUOUS_PTZ_MAPPING_TYPES (Speed)\
    (AccelerationParameters)\
    (RelativeContinuousMoveMapping)\
    (RelativeContinuousMoveComponentMapping)\
    (SimpleRelativeContinuousMoveMapping)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(CONTINUOUS_PTZ_MAPPING_TYPES, (json)(eq))

} // namespace ptz
} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::ptz::SimpleRelativeContinuousMoveMapping);
Q_DECLARE_METATYPE(nx::core::ptz::RelativeContinuousMoveMapping);
QN_FUSION_DECLARE_FUNCTIONS(nx::core::ptz::AccelerationType, (lexical));
