#include "continuous_move_sequence_executor.h"

#include <nx/utils/thread/functor_wrapper.h>

namespace nx {
namespace core {
namespace ptz {

namespace {

static const ptz::Vector kStopCommand(0.0, 0.0, 0.0, 0.0);

void callUnlocked(SequenceExecutedCallback callback, QnMutex* lockedMutex)
{
    SequenceExecutedCallback copy;
    copy.swap(callback);
    if (copy)
    {
        lockedMutex->unlock();
        copy();
        lockedMutex->lock();
    }
}

} // namespace

ContinuousMoveSequenceExecutor::ContinuousMoveSequenceExecutor(
    QnAbstractPtzController* controller,
    QThreadPool* threadPool)
    :
    m_controller(controller),
    m_timer(std::make_unique<nx::network::aio::Timer>()),
    m_threadPool(threadPool)
{
}

ContinuousMoveSequenceExecutor::~ContinuousMoveSequenceExecutor()
{
    terminate();
}

bool ContinuousMoveSequenceExecutor::executeSequence(
    const CommandSequence& sequence,
    SequenceExecutedCallback sequenceExecutedCallback)
{
    QnMutexLocker lock(&m_mutex);
    m_sequence = sequence;
    m_sequenceExecutedCallback = sequenceExecutedCallback;

    if (!m_isCommandRunning)
        return executeSequenceInternal(/*isContinuation*/ false);

    return true;
}

bool ContinuousMoveSequenceExecutor::executeSequenceInternal(bool isContinuation)
{
    if (m_terminated)
    {
        callUnlocked(m_sequenceExecutedCallback, &m_mutex);
        return false;
    }

    if (m_sequence.empty())
    {
        if (!isContinuation)
        {
            callUnlocked(m_sequenceExecutedCallback, &m_mutex);
            return true;
        }

        m_isCommandRunning = true;
        m_threadPool->start(new nx::utils::FunctorWrapper<std::function<void()>>(
            [this]()
            {
                QnMutexLocker lock(&m_mutex);
                m_controller->continuousMove(kStopCommand);
                m_isCommandRunning = false;
                m_wait.wakeAll();
                callUnlocked(m_sequenceExecutedCallback, &m_mutex);
            }));
        return true;
    }

    const auto command = m_sequence.front();
    m_sequence.pop_front();

    auto timerExpiredCallback =
        [this]()
        {
            QnMutexLocker lock(&m_mutex);
            executeSequenceInternal(/*isContinuation*/ true);
        };

    m_isCommandRunning = true;
    m_threadPool->start(new nx::utils::FunctorWrapper<std::function<void()>>(
        [this, command, callback = std::move(timerExpiredCallback)]()
        {
            QnMutexLocker lock(&m_mutex);
            if (!m_terminated)
                executeTimedCommand(command, std::move(callback));

            m_isCommandRunning = false;
            m_wait.wakeAll();
        }));
    return true;
}

bool ContinuousMoveSequenceExecutor::executeTimedCommand(
    const TimedCommand& timedCommand,
    TimedCommandDoneCallback commandDoneCallback)
{
    m_timer->start(timedCommand.duration, std::move(commandDoneCallback));
    return m_controller->continuousMove(timedCommand.command, timedCommand.options);
}

void ContinuousMoveSequenceExecutor::terminate()
{
    QnMutexLocker lock(&m_mutex);
    m_terminated = true;

    while (m_isCommandRunning)
        m_wait.wait(&m_mutex);
}

} // namespace ptz
} // namespace core
} // namespace nx
