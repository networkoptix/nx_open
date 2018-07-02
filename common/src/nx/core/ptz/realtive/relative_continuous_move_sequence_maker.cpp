#include "relative_continuous_move_sequence_maker.h"

#include <chrono>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx {
namespace core {
namespace ptz {

namespace {

milliseconds defaultCycleDuration(Component component)
{
    switch (component)
    {
        case Component::pan:
        case Component::tilt:
        case Component::rotation:
            return 6s;
        case Component::zoom:
        case Component::focus:
            return 10s;
        default:
            NX_ASSERT(false, "Wrong component type.");
            return 6s;
    }
}

TimedCommand makeComponentCommand(
    double value,
    const Options& options,
    Component componentType,
    const RelativeContinuousMoveComponentMapping& mapping)
{
    const auto length = std::abs(value);
    const auto speed = mapping.workingSpeed.absoluteValue > 0.0
        ? mapping.workingSpeed.absoluteValue
        : 1.0;
    const auto cycleDuration = mapping.workingSpeed.cycleDuration > 0ms
        ? duration_cast<milliseconds>(mapping.workingSpeed.cycleDuration)
        : defaultCycleDuration(componentType);

    const auto accelerationTime =
        duration_cast<milliseconds>(mapping.accelerationParameters.accelerationTime);
    const auto decelerationTime =
        duration_cast<milliseconds>(mapping.decelerationParameters.accelerationTime);

    const milliseconds fullSpeedMovementTime =
        milliseconds((int)(length * cycleDuration.count() + 0.5))
        - (accelerationTime + decelerationTime) / 2;

    const milliseconds commandDuration = fullSpeedMovementTime
        + accelerationTime;

    ptz::Vector command;
    command.setComponent((value > 0) ? speed : -speed, componentType);

    return { command, options, commandDuration };
}

CommandSequence makeCommandSequence(
    const Vector& relativeMovement,
    const Options& options,
    const nx::core::ptz::RelativeContinuousMoveMapping& movementMapping)
{
    // TODO: #dmishin implement iterators for nx::core::ptz::Vector.
    const std::vector<std::pair<const double*, Component>> kComponents = {
        { &relativeMovement.pan, Component::pan },
        { &relativeMovement.tilt, Component::tilt },
        { &relativeMovement.rotation, Component::rotation },
        { &relativeMovement.zoom, Component::zoom }
    };

    CommandSequence result;
    for (const auto& component : kComponents)
    {
        const auto value = *component.first;
        if (qFuzzyIsNull(value))
            continue;

        const auto componentType = component.second;
        const auto mapping = movementMapping.componentMapping(componentType);
        const auto command = makeComponentCommand(value, options, componentType, mapping);

        if (!command.command.isNull())
            result.push_back(command);
    }
    return result;
}

} // namespace

RelativeContinuousMoveSequenceMaker::RelativeContinuousMoveSequenceMaker(
    const RelativeContinuousMoveMapping& mapping)
    :
    m_mapping(mapping)
{
}

CommandSequence RelativeContinuousMoveSequenceMaker::makeSequence(
    const Vector& relativeMoveVector, const Options& options) const
{
    return makeCommandSequence(relativeMoveVector, options, m_mapping);
}

} // namespace ptz
} // namespace core
} // namespace nx
