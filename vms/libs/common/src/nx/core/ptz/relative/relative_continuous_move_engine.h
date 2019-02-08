#pragma once

#include <memory>

#include <QtCore/QThreadPool>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/core/ptz/sequence_maker.h>
#include <nx/core/ptz/sequence_executor.h>
#include <nx/core/ptz/relative/relative_move_engine.h>

namespace nx {
namespace core {
namespace ptz {

class RelativeContinuousMoveEngine: public RelativeMoveEngine
{
public:
    RelativeContinuousMoveEngine(
        QnAbstractPtzController* controller,
        const std::shared_ptr<AbstractSequenceMaker>& sequenceMaker,
        const std::shared_ptr<AbstractSequenceExecutor>& sequenceExecutor);

    virtual bool relativeMove(
        const ptz::Vector& direction,
        const ptz::Options& options,
        RelativeMoveDoneCallback doneCallback) override;

    virtual bool relativeFocus(
        qreal direction,
        const ptz::Options& options,
        RelativeMoveDoneCallback doneCallback) override;

private:
    std::shared_ptr<AbstractSequenceMaker> m_sequenceMaker;
    std::shared_ptr<AbstractSequenceExecutor> m_sequenceExecutor;
};

} // namespace ptz
} // namespace core
} // namespace nx
