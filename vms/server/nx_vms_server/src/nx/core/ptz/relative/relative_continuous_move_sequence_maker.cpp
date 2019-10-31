#include "relative_continuous_move_sequence_maker.h"

#include <chrono>
#include <map>

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

CommandSequence mergeSortedSequence(const CommandSequence& commandSequence)
{
    static const int kMaxComponents = 5; //< Pan, tilt, rotation, zoom, focus.
    const auto commandSequenceLength = commandSequence.size();

    if (commandSequenceLength > kMaxComponents)
    {
        NX_ASSERT(
            false,
            lm("Wrong command sequence length - %1 commands, expected no more than %2 componets "
                "(Pan, Tilt, Rotation, Zoom, Focus)")
            .args(commandSequenceLength, kMaxComponents));

        return CommandSequence();
    }

    if (commandSequence.size() < 2)
        return commandSequence;

    CommandSequence rest;
    TimedCommand merged = commandSequence[0];

    for (auto i = 1; i < commandSequence.size(); ++i)
    {
        auto commandToMerge = commandSequence[i];
        merged.command += commandToMerge.command;
        commandToMerge.duration -= merged.duration;

        if (commandToMerge.duration > 0ms)
            rest.push_back(commandToMerge);
    }

    if (!rest.empty())
        rest = mergeSortedSequence(rest);

    rest.push_front(merged);
    return rest;
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

    const milliseconds commandDuration = fullSpeedMovementTime + accelerationTime;

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

    std::map<double, CommandSequence> commandBySpeed;
    for (const auto& component: kComponents)
    {
        const auto value = *component.first;
        if (qFuzzyIsNull(value))
            continue;

        const auto componentType = component.second;
        const auto mapping = movementMapping.componentMapping(componentType);
        const auto command = makeComponentCommand(value, options, componentType, mapping);

        commandBySpeed[mapping.workingSpeed.absoluteValue].push_back(command);
    }

    CommandSequence result;
    for (auto& entry: commandBySpeed)
    {
        auto& sequence = entry.second;
        std::sort(
            sequence.begin(),
            sequence.end(),
            [](const TimedCommand& lhs, const TimedCommand& rhs)
            {
                return lhs.duration < rhs.duration;
            });

        const auto merged = mergeSortedSequence(sequence);
        result.insert(result.cend(), merged.cbegin(), merged.cend());
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
