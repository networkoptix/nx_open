#include "relative_absolute_move_engine.h"

namespace nx {
namespace core {
namespace ptz {

RelativeAbsoluteMoveEngine::RelativeAbsoluteMoveEngine(QnAbstractPtzController* controller):
    m_controller(controller)
{
}

bool RelativeAbsoluteMoveEngine::relativeMove(
    const nx::core::ptz::Vector& direction,
    const nx::core::ptz::Options& options)
{
    const auto space = bestSpace(options);
    if (space == Qn::InvalidPtzCoordinateSpace)
        return false;

    const auto limits = controllerLimits(space, options);
    if (limits == std::nullopt)
        return false;

    const auto position = currentPosition(space, options);
    if (position == std::nullopt)
        return false;

    const auto delta = direction * ptz::Vector::rangeVector(*limits, LimitsType::position);
    const auto positionToMove = (*position + delta)
        .restricted(*limits, LimitsType::position);

    return m_controller->absoluteMove(space, positionToMove, 1.0, options);
}

bool RelativeAbsoluteMoveEngine::relativeFocus(
    qreal direction,
    const nx::core::ptz::Options& options)
{
    NX_ASSERT(false, lit("Absolute focus positioning is not supported"));
    return false;
}

Qn::PtzCoordinateSpace RelativeAbsoluteMoveEngine::bestSpace(
    const nx::core::ptz::Options& options) const
{
    const auto capabilities = m_controller->getCapabilities(options);
    if (capabilities & Ptz::LogicalPositioningPtzCapability)
        return Qn::PtzCoordinateSpace::LogicalPtzCoordinateSpace;

    if (capabilities & Ptz::DevicePositioningPtzCapability)
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
