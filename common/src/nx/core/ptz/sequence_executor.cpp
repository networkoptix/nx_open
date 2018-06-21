#include "sequence_executor.h"

namespace nx {
namespace core {
namespace ptz {

SequenceExecutor::SequenceExecutor(QnAbstractPtzController* controller):
    m_controller(controller)
{
}

bool SequenceExecutor::executeSequence(const CommandSequence& sequence)
{
    // TODO: #dmishin implement.
    return false;
}

} // namespace ptz
} // namespace core
} // namespace nx
