#pragma once

#include "uplink_speed_tester.h"

#include <nx/network/aio/basic_pollable.h>

namespace nx::network::cloud::speed_test {

/**
 * Schedules an uplink speed test to take place at some random time between minTime and maxTime,
 * repeating each day at that time.
 */
class NX_NETWORK_API ScheduledUplinkSpeedTester:
    public AbstractSpeedTester
{
    using base_type = aio::BasicPollable;
public:
    static constexpr std::chrono::milliseconds kInvalidTime = std::chrono::milliseconds(-1);

public:
    ScheduledUplinkSpeedTester(
        std::chrono::milliseconds minLocalTime = std::chrono::hours(1),
        std::chrono::milliseconds maxLocalTime = std::chrono::hours(4));
    ~ScheduledUplinkSpeedTester();

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    /**
     * Note: start will perform one test immediately, and then per schedule after that
     */
    virtual void start(const nx::utils::Url& speedTestUrl, CompletionHandler handler) override;

    std::chrono::milliseconds waitTimeBeforeNextTest() const;
    std::set<std::chrono::milliseconds> testSchedule() const;

private:
    void scheduleNextTest(const std::chrono::milliseconds& wait);

private:
    std::chrono::milliseconds m_minTime;
    std::chrono::milliseconds m_maxTime;
    std::set<std::chrono::milliseconds> m_testSchedule;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    std::unique_ptr<UplinkSpeedTester> m_speedTester;
    nx::utils::Url m_url;
    CompletionHandler m_handler;
};


} // namespace nx::network::cloud::speed_test

