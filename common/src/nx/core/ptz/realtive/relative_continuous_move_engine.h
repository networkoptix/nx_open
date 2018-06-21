#pragma once

#include <QtCore/QThreadPool>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/core/ptz/sequence_executor.h>
#include <nx/core/ptz/realtive/relative_move_engine.h>
#include <nx/core/ptz/realtive/relative_continuous_move_mapping.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeContinuousMoveEngine: public RelativeMoveEngine
{
public:
    RelativeContinuousMoveEngine(
        QnAbstractPtzController* controller,
        const RelativeContinuousMoveMapping& mapping,
        QThreadPool* threadPool);

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) override;

private:
    SequenceExecutor m_sequenceExecutor;
    RelativeContinuousMoveMapping m_mapping;
};

} // namespace ptz
} // namespace core
} // namespace nx
