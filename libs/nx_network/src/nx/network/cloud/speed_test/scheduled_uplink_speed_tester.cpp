#include "scheduled_uplink_speed_tester.h"

#include <nx/utils/random.h>

namespace nx::network::cloud::speed_test {

using namespace std::chrono;
using namespace nx::hpm::api;

namespace {

static const std::chrono::milliseconds kOneDay = hours(24);

}

ScheduledUplinkSpeedTester::ScheduledUplinkSpeedTester(
    milliseconds minLocalTime,
    milliseconds maxLocalTime):
    m_minTime(minLocalTime),
    m_maxTime(maxLocalTime)
{
    m_testSchedule.emplace(
        milliseconds(nx::utils::random::number(m_minTime.count(), m_maxTime.count())));
}

ScheduledUplinkSpeedTester::~ScheduledUplinkSpeedTester()
{
    pleaseStopSync();
}

void ScheduledUplinkSpeedTester::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    if (m_timer)
        m_timer->bindToAioThread(aioThread);
    if (m_speedTester)
        m_speedTester->bindToAioThread(aioThread);
}

void ScheduledUplinkSpeedTester::stopWhileInAioThread()
{
    m_timer.reset();
    m_speedTester.reset();
}

void ScheduledUplinkSpeedTester::start(
    const nx::utils::Url& speedTestUrl,
    CompletionHandler handler)
{
    dispatch(
        [this, speedTestUrl, handler = std::move(handler)]() mutable
    {
        m_url = speedTestUrl;
        m_handler = std::move(handler);
        m_timer = std::make_unique<nx::network::aio::Timer>();
        scheduleNextTest(milliseconds::zero());
    });
}

milliseconds ScheduledUplinkSpeedTester::waitTimeBeforeNextTest() const
{
    if (m_testSchedule.empty())
        return kInvalidTime;

    auto now = localNow();
    std::optional<milliseconds> startTime;
    for (auto it = m_testSchedule.begin(); it != m_testSchedule.end(); ++it)
    {
        if (now < *it)
        {
            startTime = *it;
            break;
        }
    }

    // If startTime is empty, we are later in the day than anything in m_testSchedule, so take the
    // earliest start time and shift it back 24 hours.
    if (!startTime)
    {
        // Braces required to avoid "misleading indentation" error.
        startTime = *m_testSchedule.begin() + kOneDay;
    }

    return *startTime - now;
}

std::set<milliseconds> ScheduledUplinkSpeedTester::testSchedule() const
{
    return m_testSchedule;
}

void ScheduledUplinkSpeedTester::scheduleNextTest(const milliseconds& wait)
{
    NX_VERBOSE(this, "The next test is scheduled in %1", wait);

    m_timer->start(
        wait,
        [this]()
        {
            m_speedTester = std::make_unique<UplinkSpeedTester>();
            m_speedTester->start(
                m_url,
                [this](SystemError::ErrorCode errorCode, std::optional<ConnectionSpeed> result)
                {
                    m_handler(errorCode, std::move(result));

                    scheduleNextTest(waitTimeBeforeNextTest());

                    m_speedTester.reset();
                });
        });
}

} // namespace nx::network::cloud::speed_test

