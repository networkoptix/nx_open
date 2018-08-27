#include "relative_continuous_move_engine.h"

namespace nx {
namespace core {
namespace ptz {

RelativeContinuousMoveEngine::RelativeContinuousMoveEngine(
    QnAbstractPtzController* controller,
    const std::shared_ptr<AbstractSequenceMaker>& sequenceMaker,
    const std::shared_ptr<AbstractSequenceExecutor>& sequenceExecutor)
    :
    m_sequenceMaker(sequenceMaker),
    m_sequenceExecutor(sequenceExecutor)
{
}

bool RelativeContinuousMoveEngine::relativeMove(
    const ptz::Vector& direction,
    const ptz::Options& options,
    RelativeMoveDoneCallback doneCallback)
{
    const auto commandSequence = m_sequenceMaker->makeSequence(direction, options);
    return m_sequenceExecutor->executeSequence(commandSequence, doneCallback);
}

bool RelativeContinuousMoveEngine::relativeFocus(
    qreal direction,
    const ptz::Options& options,
    RelativeMoveDoneCallback doneCallback)
{
    const auto sequence = m_sequenceMaker->makeSequence(
        ptz::Vector(0.0, 0.0, 0.0, 0.0, direction),
        options);

    return m_sequenceExecutor->executeSequence(sequence, doneCallback);
}

} // namespace ptz
} // namespace core
} // namespace nx
