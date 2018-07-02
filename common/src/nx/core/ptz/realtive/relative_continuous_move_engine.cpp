#include "relative_continuous_move_engine.h"

namespace nx {
namespace core {
namespace ptz {

RelativeContinuousMoveEngine::RelativeContinuousMoveEngine(
    QnAbstractPtzController* controller,
    const std::shared_ptr<SequenceMaker>& sequenceMaker,
    const std::shared_ptr<SequenceExecutor>& sequenceExecutor)
    :
    m_sequenceMaker(sequenceMaker),
    m_sequenceExecutor(sequenceExecutor)
{
}

bool RelativeContinuousMoveEngine::relativeMove(
    const nx::core::ptz::Vector& direction,
    const nx::core::ptz::Options& options)
{
    const auto commandSequence = m_sequenceMaker->makeSequence(direction, options);
    return m_sequenceExecutor->executeSequence(commandSequence);
}

bool RelativeContinuousMoveEngine::relativeFocus(
    qreal direction,
    const nx::core::ptz::Options& options)
{
    const auto sequence = m_sequenceMaker->makeSequence(
        ptz::Vector(0.0, 0.0, 0.0, 0.0, direction),
        options);

    return m_sequenceExecutor->executeSequence(sequence);
}

} // namespace ptz
} // namespace core
} // namespace nx
