#pragma once

#include <atomic>

#include <nx/utils/interruption_flag.h>

#include "timer.h"

namespace nx::network::aio {

class NX_NETWORK_API RepetitiveTimer:
    public BasicPollable
{
    using base_type = BasicPollable;

public:
    RepetitiveTimer();

    virtual void bindToAioThread(AbstractAioThread* aioThread) override;

    virtual void pleaseStopSync() override;

    /**
     * Starts timer.
     * timerFunc will be invoked every timeout until cancelled.
     */
    void start(
        std::chrono::milliseconds timeout,
        TimerEventHandler timerFunc);

    /**
     * Cancels timer waiting for timerFunc to complete.
     * Can be safely called within timer's aio thread.
     */
    void cancelSync();

protected:
    virtual void stopWhileInAioThread() override;

private:
    Timer m_timer;
    std::chrono::milliseconds m_timeout = std::chrono::milliseconds::zero();
    TimerEventHandler m_timerFunc;
    nx::utils::InterruptionFlag m_destructionFlag;
    std::atomic<bool> m_cancelled = {false};

    void onTimerEvent();
};

} // namespace nx::network::aio
