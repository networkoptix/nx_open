#include "relative_absolute_move_engine.h"

#include <nx/utils/scope_guard.h>
#include <nx/utils/math/cycled_range.h>

namespace nx {
namespace core {
namespace ptz {

RelativeAbsoluteMoveEngine::RelativeAbsoluteMoveEngine(QnAbstractPtzController* controller):
    m_controller(controller)
{
}

bool RelativeAbsoluteMoveEngine::relativeMove(
    const ptz::Vector& direction,
    const ptz::Options& options,
    RelativeMoveDoneCallback doneCallback)
{
    const auto guard = nx::utils::makeScopeGuard([&doneCallback]() { doneCallback(); });
    const auto space = bestSpace(options);
    if (space == Qn::InvalidPtzCoordinateSpace)
        return false;

    const auto limits = controllerLimits(space, options);
    if (limits == std::nullopt)
        return false;

    const auto position = currentPosition(space, options);
    if (position == std::nullopt)
        return false;

    const auto range = ptz::Vector::rangeVector(*limits, LimitsType::position);
    const auto delta = direction * range;

    ptz::Vector positionToMove = *position + delta;
    nx::utils::math::CycledRange<double> cycledRange(
        positionToMove.pan,
        limits->minPan,
        limits->maxPan);

    const auto normalizedPosition = cycledRange.position();
    if (normalizedPosition == std::nullopt)
        return false;

    positionToMove.pan = *normalizedPosition;
    positionToMove = positionToMove.restricted(*limits, LimitsType::position);
    return m_controller->absoluteMove(space, positionToMove, 1.0, options);
}

bool RelativeAbsoluteMoveEngine::relativeFocus(
    qreal /*direction*/,
    const ptz::Options& /*options*/,
    RelativeMoveDoneCallback doneCallback)
{
    NX_ASSERT(false, lit("Absolute focus positioning is not supported"));
    doneCallback();
    return false;
}

Qn::PtzCoordinateSpace RelativeAbsoluteMoveEngine::bestSpace(
    const ptz::Options& options) const
{
    const auto capabilities = m_controller->getCapabilities(options);
    if (capabilities.testFlag(Ptz::LogicalPositioningPtzCapability))
        return Qn::PtzCoordinateSpace::LogicalPtzCoordinateSpace;

    if (capabilities.testFlag(Ptz::DevicePositioningPtzCapability))
        return Qn::PtzCoordinateSpace::DevicePtzCoordinateSpace;

    return Qn::InvalidPtzCoordinateSpace;
}

std::optional<Vector> RelativeAbsoluteMoveEngine::currentPosition(
    Qn::PtzCoordinateSpace space,
    const Options& options) const
{
    nx::core::ptz::Vector position;
    const bool result = m_controller->getPosition(space, &position, options);
    if (!result)
        return std::nullopt;

    return position;
}

std::optional<QnPtzLimits> RelativeAbsoluteMoveEngine::controllerLimits(
    Qn::PtzCoordinateSpace space,
    const Options& options) const
{
    QnPtzLimits limits;
    const bool result = m_controller->getLimits(space, &limits, options);
    if (!result)
        return std::nullopt;

    return limits;
}

} // namespace ptz
} // namespace core
} // namespace nx
