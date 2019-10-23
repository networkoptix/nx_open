#include "scheduler.h"

#include <QDateTime>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/time.h>

namespace nx::network::aio {

using namespace std::chrono;

namespace {

static milliseconds localTime()
{
    const auto now = QDateTime::currentDateTime();
    return milliseconds(nx::utils::millisSinceEpoch().count() + (now.offsetFromUtc() * 1000));
}

static QString toString(milliseconds localTime)
{
    const auto now = QDateTime::currentDateTime();
    const auto utc = localTime.count() - (now.offsetFromUtc() * 1000);
    return QDateTime::fromMSecsSinceEpoch(utc).toString();
}

} // namespace

Scheduler::Scheduler(
    milliseconds period,
    const std::set<milliseconds>& schedule)
    :
    m_period(period),
    m_schedule(schedule)
{
    NX_ASSERT(m_period > milliseconds(0), "Negative periods are not allowed");
    NX_ASSERT(!m_schedule.empty());
    for (const auto& timepoint: m_schedule)
    {
        NX_ASSERT(timepoint < m_period, "Scheduler has timepoint '%1' that is >= period '%2'",
            timepoint, m_period);
    }
}

Scheduler::~Scheduler()
{
    pleaseStopSync();
}

void Scheduler::bindToAioThread(AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_timer)
        m_timer->bindToAioThread(aioThread);
}

void Scheduler::start(Scheduler::Handler handler)
{
    m_handler = std::move(handler);

    dispatch(
        [this]() mutable
        {
            m_timer = std::make_unique<Timer>(getAioThread());
            scheduleNextTimer(nextTimepoint());
        });
}

milliseconds Scheduler::period() const
{
    return m_period;
}

const std::set<std::chrono::milliseconds>& Scheduler::schedule() const
{
    return m_schedule;
}

Scheduler::Timepoint Scheduler::nextTimepoint() const
{
    const auto timeSincePeriodStart = localTime() % m_period;
    const auto it = m_schedule.lower_bound(timeSincePeriodStart);

    Timepoint timepoint;
    if (it == m_schedule.end())
    {
        timepoint.timepoint = *m_schedule.begin();
        timepoint.delay = (m_period - timeSincePeriodStart) + *m_schedule.begin();
    }
    else
    {
        timepoint.timepoint = *it;
        timepoint.delay = *it - timeSincePeriodStart;
    }

    return timepoint;
}

void Scheduler::stopWhileInAioThread()
{
    m_interruptionFlag.interrupt();
    m_timer.reset();
}

void Scheduler::scheduleNextTimer(const Scheduler::Timepoint& timepoint)
{
    NX_VERBOSE(this, "Scheduling next timer in %1, will be invoked at: %2 (local time)",
        timepoint.delay, toString(localTime() + timepoint.delay));

    m_timer->start(
        timepoint.delay,
        [this, timepoint]()
        {
            nx::utils::InterruptionFlag::Watcher watcher(&m_interruptionFlag);

            m_handler(timepoint.timepoint);

            if (!watcher.interrupted())
                scheduleNextTimer(nextTimepoint());
        });
}

} // namespace nx::network::cloud::speed_test