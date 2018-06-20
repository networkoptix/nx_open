#pragma once

#include <array>
#include <chrono>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/core/ptz/component.h>

namespace nx {
namespace core {
namespace ptz {

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
    Speed workingSpeed;

    std::chrono::milliseconds startRequestProcessingTime{0};
    std::chrono::milliseconds stopRequestProcessingTime{0};

    AccelerationParameters accelerationParameters;
    AccelerationParameters decelerationParameters;
};
#define RelativeContinuousMoveComponentMapping_Fields (workingSpeed)\
    (startRequestProcessingTime)\
    (stopRequestProcessingTime)\
    (accelerationParameters)\
    (decelerationParameters)

struct RelativeContinuousMoveMapping
{
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
    (RelativeContinuousMoveComponentMapping)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(CONTINUOUS_PTZ_MAPPING_TYPES, (json))

} // namespace ptz
} // namespace core
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::core::ptz::AccelerationType, (lexical));
