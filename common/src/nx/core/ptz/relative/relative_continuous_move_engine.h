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
        const std::shared_ptr<SequenceMaker>& sequenceMaker,
        const std::shared_ptr<SequenceExecutor>& sequenceExecutor);

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) override;

private:
    std::shared_ptr<SequenceMaker> m_sequenceMaker;
    std::shared_ptr<SequenceExecutor> m_sequenceExecutor;
};

} // namespace ptz
} // namespace core
} // namespace nx
