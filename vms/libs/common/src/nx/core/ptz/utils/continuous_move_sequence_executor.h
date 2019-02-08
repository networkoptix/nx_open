#pragma once

#include <memory>

#include <QtCore/QThreadPool>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/network/aio/timer.h>
#include <nx/core/ptz/sequence_executor.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/move_only_func.h>


namespace nx {
namespace core {
namespace ptz {

using TimedCommandDoneCallback = nx::utils::MoveOnlyFunc<void()>;

class ContinuousMoveSequenceExecutor: public AbstractSequenceExecutor
{
public:
    ContinuousMoveSequenceExecutor(
        QnAbstractPtzController* controller,
        QThreadPool* threadPool);

    virtual ~ContinuousMoveSequenceExecutor() override;

    virtual bool executeSequence(
        const CommandSequence& sequence,
        SequenceExecutedCallback callback) override;

private:
    bool executeSequenceInternal(bool isContinuation);

    bool executeTimedCommand(
        const TimedCommand& timedCommand,
        TimedCommandDoneCallback commandDoneCallback);

    void terminate();

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_wait;

    QnAbstractPtzController* m_controller;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    QThreadPool* m_threadPool;
    CommandSequence m_sequence;
    bool m_terminated = false;
    bool m_isCommandRunning = false;

    SequenceExecutedCallback m_sequenceExecutedCallback;
};

} // namespace ptz
} // namespace core
} // namespace nx
