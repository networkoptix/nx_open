#include "sequence_executor.h"

#include <nx/utils/thread/functor_wrapper.h>

namespace nx {
namespace core {
namespace ptz {

SequenceExecutor::SequenceExecutor(
    QnAbstractPtzController* controller,
    QThreadPool* threadPool)
    :
    m_controller(controller),
    m_timer(std::make_unique<nx::network::aio::Timer>()),
    m_threadPool(threadPool)
{
}

SequenceExecutor::~SequenceExecutor()
{
}

bool SequenceExecutor::executeSequence(const CommandSequence& sequence)
{
    QnMutexLocker lock(&m_mutex);
    m_sequence = sequence;

    return executeSequenceInternal();
}

bool SequenceExecutor::executeSequenceInternal()
{
    if (m_sequence.empty())
        return true;

    const auto command = m_sequence.front();
    m_sequence.pop_front();

    auto timerExpiredCallback =
        [this]()
        {
            QnMutexLocker lock(&m_mutex);
            executeSequenceInternal();
        };

    m_threadPool->start(new nx::utils::FunctorWrapper(
        [this, command, callback = std::move(timerExpiredCallback)]()
        {
            QnMutexLocker lock(&m_mutex);
            executeTimedCommand(command, std::move(callback));
        }));
    return true;
}

bool SequenceExecutor::executeTimedCommand(
    const TimedCommand& timedCommand,
    TimedCommandDoneCallback commandDoneCallback)
{
    m_timer->start(timedCommand.duration, std::move(commandDoneCallback));
    return m_controller->continuousMove(timedCommand.command, timedCommand.options);
}

} // namespace ptz
} // namespace core
} // namespace nx
