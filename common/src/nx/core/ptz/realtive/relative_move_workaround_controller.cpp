#include "relative_move_workaround_controller.h"
#include "relative_continuous_move_engine.h"
#include "relative_absolute_move_engine.h"

#include <map>

namespace nx {
namespace core {
namespace ptz {

namespace {

static const std::map<Ptz::Capability, std::vector<Ptz::Capability>> kCapabilityRequirements = {
    {Ptz::RelativePanCapability, {Ptz::AbsolutePanCapability, Ptz::ContinuousPanCapability}},
    {Ptz::RelativeTiltCapability, {Ptz::AbsoluteTiltCapability, Ptz::ContinuousTiltCapability}},
    {
        Ptz::RelativeRotationCapability,
        {Ptz::AbsoluteRotationCapability, Ptz::ContinuousRotationCapability}
    },
    {Ptz::RelativeZoomCapability, {Ptz::AbsoluteZoomCapability, Ptz::ContinuousZoomCapability}},
    {Ptz::RelativeFocusCapability, {Ptz::ContinuousFocusCapability}}
};

bool isAbsolute(Ptz::Capability capability)
{
    return (capability & Ptz::AbsolutePtrzCapabilities) != Ptz::NoPtzCapabilities;
}

bool isContinuous(Ptz::Capability capability)
{
    return (capability & Ptz::ContinuousPtrzCapabilities) != Ptz::NoPtzCapabilities
        || capability == Ptz::ContinuousFocusCapability;
}

bool isRelative(Ptz::Capability capability)
{
    return (capability & Ptz::RelativePtrzCapabilities) != Ptz::NoPtzCapabilities
        || capability == Ptz::RelativeFocusCapability;
}

Ptz::Capability extendsCapabilitiesWith(
    Ptz::Capability relativeCapability,
    Ptz::Capabilities realCapabilities)
{
    if (realCapabilities.testFlag(relativeCapability))
        return Ptz::NoPtzCapabilities;

    const auto itr = kCapabilityRequirements.find(relativeCapability);
    if (itr == kCapabilityRequirements.cend())
        return Ptz::NoPtzCapabilities;

    for (const auto requiredCapability: itr->second)
    {
        if (realCapabilities.testFlag(requiredCapability))
            return requiredCapability;
    }

    return Ptz::NoPtzCapabilities;
}

Ptz::Capabilities extendedCapabilities(Ptz::Capabilities originalCapabilities)
{
    auto extendedCapabilities = originalCapabilities;
    for (const auto& entry: kCapabilityRequirements)
    {
        const auto relativeCapability = entry.first;
        const auto extensionCapability = extendsCapabilitiesWith(
            relativeCapability,
            originalCapabilities);

        if (extensionCapability != Ptz::NoPtzCapabilities)
            extendedCapabilities |= relativeCapability;
    }

    return extendedCapabilities;
}

} // namespace

RelativeMoveWorkaroundController::RelativeMoveWorkaroundController(
    const QnPtzControllerPtr& controller,
    const RelativeContinuousMoveMapping& mapping,
    QThreadPool* threadPool)
    :
    base_type(controller),
    m_continuousMoveEngine(
        std::make_unique<RelativeContinuousMoveEngine>(controller.data(), mapping, threadPool)),
    m_absoluteMoveEngine(std::make_unique<RelativeAbsoluteMoveEngine>(controller.data()))
{
}

/*static*/ bool RelativeMoveWorkaroundController::extends(Ptz::Capabilities capabilities)
{
    const auto extendedCaps = extendedCapabilities(capabilities);
    return extendedCaps != capabilities;
}

Ptz::Capabilities RelativeMoveWorkaroundController::getCapabilities(
    const nx::core::ptz::Options& options) const
{
    Ptz::Capabilities capabilities = base_type::getCapabilities(options);
    return extendedCapabilities(capabilities);
}

bool RelativeMoveWorkaroundController::relativeMove(
    const nx::core::ptz::Vector& direction,
    const nx::core::ptz::Options& options)
{
    if (qFuzzyIsNull(direction))
        return true;

    std::map<Ptz::Capability, double ptz::Vector::*> componentMap = {
        {Ptz::RelativePanCapability, &ptz::Vector::pan},
        {Ptz::RelativeTiltCapability, &ptz::Vector::tilt},
        {Ptz::RelativeRotationCapability, &ptz::Vector::rotation},
        {Ptz::RelativeZoomCapability, &ptz::Vector::zoom},
    };

    ptz::Vector absoluteVector;
    ptz::Vector continuousVector;
    ptz::Vector relativeVector;

    for (const auto& entry: componentMap)
    {
        const auto capability = entry.first;
        const auto field = entry.second;
        const auto componentValue = direction.*field;

        if (qFuzzyIsNull(direction.*field))
            continue;

        const auto extensionCapability = extendsWith(capability, options);
        if (extensionCapability == Ptz::NoPtzCapabilities)
            return false;

        if (isRelative(extensionCapability))
            relativeVector.*field = componentValue;
        else if (isAbsolute(extensionCapability))
            absoluteVector.*field = componentValue;
        else if (isContinuous(extensionCapability))
            continuousVector.*field = componentValue;
    }

    if (!qFuzzyIsNull(relativeVector))
    {
        if (!base_type::relativeMove(relativeVector, options))
            return false;
    }

    if (!qFuzzyIsNull(absoluteVector))
    {
        if (!m_absoluteMoveEngine->relativeMove(absoluteVector, options))
            return false;
    }

    if (!qFuzzyIsNull(continuousVector))
    {
        if (!m_continuousMoveEngine->relativeMove(continuousVector, options))
            return false;
    }

    return true;
}

bool RelativeMoveWorkaroundController::relativeFocus(
    qreal direction,
    const nx::core::ptz::Options& options)
{
    if (qFuzzyIsNull(direction))
        return true;

    const auto extensionCapability = extendsWith(Ptz::RelativeFocusCapability, options);
    if (extensionCapability == Ptz::NoPtzCapabilities)
        return false;

    if (isRelative(extensionCapability))
        return base_type::relativeFocus(direction, options);

    if (isContinuous(extensionCapability))
        return m_continuousMoveEngine->relativeFocus(direction, options);

    return false;
}

Ptz::Capability RelativeMoveWorkaroundController::extendsWith(
    Ptz::Capability relativeCapability,
    const nx::core::ptz::Options& options) const
{
    const auto capabilities = base_type::getCapabilities(options);
    return extendsCapabilitiesWith(relativeCapability, capabilities);
}

} // namespace ptz
} // namespace core
} // namespace nx
