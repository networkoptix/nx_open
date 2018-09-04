#include "relative_move_workaround_controller.h"
#include "relative_continuous_move_engine.h"
#include "relative_absolute_move_engine.h"

#include <nx/utils/scope_guard.h>

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
        return relativeCapability;

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
    const std::shared_ptr<AbstractSequenceMaker>& sequenceMaker,
    const std::shared_ptr<AbstractSequenceExecutor>& sequenceExecutor)
    :
    base_type(controller),
    m_absoluteMoveEngine(std::make_unique<RelativeAbsoluteMoveEngine>(controller.data())),
    m_continuousMoveEngine(std::make_unique<RelativeContinuousMoveEngine>(
        controller.data(),
        sequenceMaker,
        sequenceExecutor))
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
    auto guard = nx::utils::makeScopeGuard(
        [this]()
        {
            if (m_relativeMoveDoneCallback)
                m_relativeMoveDoneCallback();
        });

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

    int enginesInvolved = !qFuzzyIsNull(absoluteVector) + !qFuzzyIsNull(continuousVector);
    if (enginesInvolved == 0)
        return true;

    guard.disarm();
    const auto id = createTrigger(
        [this]()
        {
            if (m_relativeMoveDoneCallback)
                m_relativeMoveDoneCallback();
        },
        enginesInvolved);

    if (!qFuzzyIsNull(absoluteVector))
    {
        if (!m_absoluteMoveEngine->relativeMove(
            absoluteVector,
            options,
            [this, id]() { trigger(id); }))
        {
            return false;
        }
    }

    if (!qFuzzyIsNull(continuousVector))
    {
        if (!m_continuousMoveEngine->relativeMove(
            continuousVector,
            options,
            [this, id]() { trigger(id); }))
        {
            return false;
        }
    }

    return true;
}

bool RelativeMoveWorkaroundController::relativeFocus(
    qreal direction,
    const nx::core::ptz::Options& options)
{
    auto guard = nx::utils::makeScopeGuard(
        [this]()
        {
            if (m_relativeMoveDoneCallback)
                m_relativeMoveDoneCallback();
        });

    if (qFuzzyIsNull(direction))
        return true;

    const auto extensionCapability = extendsWith(Ptz::RelativeFocusCapability, options);
    if (extensionCapability == Ptz::NoPtzCapabilities)
        return false;

    if (isRelative(extensionCapability))
        return base_type::relativeFocus(direction, options);

    if (isContinuous(extensionCapability))
    {
        guard.disarm(); //< Callback will be called by the movement engine.
        return m_continuousMoveEngine->relativeFocus(
            direction,
            options,
            m_relativeMoveDoneCallback);
    }

    return false;
}

void RelativeMoveWorkaroundController::setRelativeMoveDoneCallback(
    RelativeMoveDoneCallback callback)
{
    m_relativeMoveDoneCallback = callback;
}

Ptz::Capability RelativeMoveWorkaroundController::extendsWith(
    Ptz::Capability relativeCapability,
    const nx::core::ptz::Options& options) const
{
    const auto capabilities = base_type::getCapabilities(options);
    return extendsCapabilitiesWith(relativeCapability, capabilities);
}

QnUuid RelativeMoveWorkaroundController::createTrigger(
    RelativeMoveDoneCallback callback,
    int engineCount)
{
    QnMutexLocker lock(&m_triggerMutex);
    while (true)
    {
        const auto id = QnUuid::createUuid();
        if (m_callbackTriggers.find(id) != m_callbackTriggers.cend()) //< Just in case.
            continue;

        auto trigger = std::make_shared<CallbackTrigger>(callback, engineCount);
        m_callbackTriggers[id] = trigger;

        return id;
    }
}

void RelativeMoveWorkaroundController::trigger(const QnUuid& id)
{
    QnMutexLocker lock(&m_triggerMutex);
    auto itr = m_callbackTriggers.find(id);
    if (itr == m_callbackTriggers.cend())
        return;

    auto trigger = itr->second;

    m_triggerMutex.unlock();
    const bool hasBeenTriggered = trigger->trigger();
    m_triggerMutex.lock();

    if (hasBeenTriggered)
        m_callbackTriggers.erase(id);
}

} // namespace ptz
} // namespace core
} // namespace nx
