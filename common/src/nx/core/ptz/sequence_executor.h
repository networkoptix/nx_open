#pragma once

#include <memory>

#include <QtCore/QThreadPool>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/utils/thread/mutex.h>
#include <nx/core/ptz/timed_command.h>
#include <nx/network/aio/timer.h>

namespace nx {
namespace core {
namespace ptz {

using TimedCommandDoneCallback = nx::utils::MoveOnlyFunc<void()>;

class SequenceExecutor
{
public:
    SequenceExecutor(
        QnAbstractPtzController* controller,
        QThreadPool* threadPool);

    ~SequenceExecutor();

    bool executeSequence(const CommandSequence& sequence);

private:
    bool executeSequenceInternal();

    bool executeTimedCommand(
        const TimedCommand& timedCommand,
        TimedCommandDoneCallback commandDoneCallback);

private:
    mutable QnMutex m_mutex;
    QnAbstractPtzController* m_controller;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    QThreadPool* m_threadPool;
    CommandSequence m_sequence;
    bool m_terminated = false;
};

} // namespace ptz
} // namespace core
} // namespace nx
