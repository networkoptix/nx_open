#pragma once

#include <core/ptz/abstract_ptz_controller.h>
#include <nx/core/ptz/relative/relative_move_engine.h>
#include <nx/utils/std/optional.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeAbsoluteMoveEngine: public RelativeMoveEngine
{

public:
    RelativeAbsoluteMoveEngine(QnAbstractPtzController* controller);

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) override;

private:
    Qn::PtzCoordinateSpace bestSpace(const nx::core::ptz::Options& options) const;

    std::optional<nx::core::ptz::Vector> currentPosition(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Options& options) const;

    std::optional<QnPtzLimits> controllerLimits(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Options& options) const;

private:
    QnAbstractPtzController* m_controller;
};

} // namespace ptz
} // namespace core
} // namespace nx
