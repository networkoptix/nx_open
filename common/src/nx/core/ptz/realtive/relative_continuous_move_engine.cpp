#include "relative_continuous_move_engine.h"

#include <nx/core/ptz/timed_command.h>

namespace nx {
namespace core {
namespace ptz {

using namespace std::chrono;

namespace {

TimedCommand makeComponentCommand(
    double value,
    Component componentType,
    const RelativeContinuousMoveComponentMapping& mapping)
{
    const auto length = std::abs(value);
    const auto cycleDuration
        = duration_cast<milliseconds>(mapping.workingSpeed.cycleDuration);

    const auto accelerationTime =
        duration_cast<milliseconds>(mapping.accelerationParameters.accelerationTime);
    const auto decelerationTime =
        duration_cast<milliseconds>(mapping.decelerationParameters.accelerationTime);

    const milliseconds fullSpeedMovementTime =
        milliseconds((int) (length * cycleDuration.count() + 0.5))
        - (accelerationTime + decelerationTime) / 2;

    const milliseconds commandDuration = fullSpeedMovementTime
        + accelerationTime;

    ptz::Vector command;
    command.setComponent(
        value > 0
            ? mapping.workingSpeed.absoluteValue
            : -mapping.workingSpeed.absoluteValue,
        componentType);

    return {command, commandDuration};
}

CommandSequence makeCommandSequence(
    const Vector& relativeMovement,
    const nx::core::ptz::RelativeContinuousMoveMapping& movementMapping)
{
    // TODO: #dmishin implement iterators for nx::core::ptz::Vector.
    static const std::vector<std::pair<const double*, Component>> kComponents = {
        {&relativeMovement.pan, Component::pan},
        {&relativeMovement.tilt, Component::tilt},
        {&relativeMovement.rotation, Component::rotation},
        {&relativeMovement.zoom, Component::zoom}
    };

    CommandSequence result;
    for (const auto& component: kComponents)
    {
        const auto value = *component.first;
        if (qFuzzyIsNull(value))
            continue;

        const auto componentType = component.second;
        const auto mapping = movementMapping.componentMapping(componentType);
        const auto command = makeComponentCommand(value, componentType, mapping);

        if (!command.command.isNull())
            result.push_back(command);
    }
    return result;
}

} // namespace

RelativeContinuousMoveEngine::RelativeContinuousMoveEngine(
    QnAbstractPtzController* controller,
    const RelativeContinuousMoveMapping& mapping)
    :
    m_sequenceExecutor(controller),
    m_mapping(mapping)
{
}

bool RelativeContinuousMoveEngine::relativeMove(
    const nx::core::ptz::Vector& direction,
    const nx::core::ptz::Options& options)
{
    const auto commandSequence = makeCommandSequence(direction, m_mapping);
    return m_sequenceExecutor.executeSequence(commandSequence);
}

bool RelativeContinuousMoveEngine::relativeFocus(
    qreal direction,
    const nx::core::ptz::Options& options)
{
    const auto command = makeComponentCommand(
        direction,
        Component::focus,
        m_mapping.componentMapping(Component::focus));

    return m_sequenceExecutor.executeSequence({command});
}

} // namespace ptz
} // namespace core
} // namespace nx
