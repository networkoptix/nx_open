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
        const ptz::Vector& direction,
        const ptz::Options& options,
        RelativeMoveDoneCallback doneCallback) override;

    virtual bool relativeFocus(
        qreal direction,
        const ptz::Options& options,
        RelativeMoveDoneCallback doneCallback) override;

private:
    Qn::PtzCoordinateSpace bestSpace(const ptz::Options& options) const;

    std::optional<ptz::Vector> currentPosition(
        Qn::PtzCoordinateSpace space,
        const ptz::Options& options) const;

    std::optional<QnPtzLimits> controllerLimits(
        Qn::PtzCoordinateSpace space,
        const ptz::Options& options) const;

private:
    QnAbstractPtzController* m_controller;
    RelativeMoveDoneCallback m_moveDoneCallback;
};

} // namespace ptz
} // namespace core
} // namespace nx
