#pragma once

#include <set>
#include <chrono>

#include <nx/utils/interruption_flag.h>
#include <nx/utils/move_only_func.h>

#include "timer.h"

namespace nx::network::aio {

/**
 * Invokes a handler on a set schedule in the local time zone, along standard time increments.
 * A schedule is defined by a period and a set of timepoints within the period.
 * For example "1:00am every day" will have
 * {period: std::chrono::hours(24), schedule: {std::chrono::hours(1)}}.
 * "Every 10 minutes" could have {period: minutes(10), schedule: {minutes(0)}}.
 * Such a schedule will invoke every 10 minutes at the following times:
 * 00:00(midnight), 00:10, 00:20, ... 1:00, 1:10, ... 23:00, 23:10, ... 23:50.
 * NOTE: Object can be deleted within its own aio thread.
 */
class NX_NETWORK_API Scheduler: public BasicPollable
{
    using base_type = BasicPollable;

public:
    /**
     * @param timepoint the corresponding timepoint from the schedule.
     */
    using Handler = nx::utils::MoveOnlyFunc<void(std::chrono::milliseconds /*timepoint*/)>;

    struct Timepoint
    {
        std::chrono::milliseconds timepoint; //< Corresponds to a timepoint within the schedule.
        std::chrono::milliseconds delay; //< The delay until the timepoint is scheduled.
    };

public:
    /**
     * @param period the period on which the schedule operates. MUST be > 0.
     * @param schedule the set of timepoints in the schedule. Each timepoint in the schedule MUST
     *     be < m_period. Each timepoint in the schedule is interpreted as local time.
     */
    Scheduler(
        std::chrono::milliseconds period,
        const std::set<std::chrono::milliseconds>& schedule);
    ~Scheduler();

    virtual void bindToAioThread(AbstractAioThread* aioThread) override;

    void start(Handler handler);

    std::chrono::milliseconds period() const;
    const std::set<std::chrono::milliseconds>& schedule() const;

    /**
     * @return the amount of time until the next timepoint in the schedule. Wraps around to first
     * timepoint if it is later in time than the final timepoint in the schedule.
     */
     Timepoint nextTimepoint() const;

protected:
    virtual void stopWhileInAioThread() override;

private:
    void scheduleNextTimer(const Timepoint& timepoint);

private:
    const std::chrono::milliseconds m_period;
    const std::set<std::chrono::milliseconds> m_schedule;
    Handler m_handler;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    nx::utils::InterruptionFlag m_interruptionFlag;
};

} // namespace nx::network::cloud::speed_test