// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/math/rate_limiter.h>
#include <nx/utils/time.h>

namespace nx::utils::math::test {

class RateLimiter:
    public ::testing::Test
{
public:
    static constexpr int kSubPeriodCount = 20;

    RateLimiter():
        m_rateLimiter(m_period, m_limit, kSubPeriodCount),
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

protected:
    int limit() { return m_limit; }

    void assertRequestsAreAllowed(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            ASSERT_TRUE(m_rateLimiter.isAllowed());
        }
    }

    void assertRequestIsNotAllowed()
    {
        ASSERT_FALSE(m_rateLimiter.isAllowed());
    }

    void waitForLimitPeriod()
    {
        // Taking into account kSubPeriodCount to account for the computation error.
        m_timeShift.applyRelativeShift(
            m_period + m_period / kSubPeriodCount + std::chrono::milliseconds(1));
    }

private:
    std::chrono::milliseconds m_period = std::chrono::hours(1);
    int m_limit = 100;
    math::RateLimiter m_rateLimiter;
    nx::utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(RateLimiter, allows_expected_rps)
{
    assertRequestsAreAllowed(limit());
}

TEST_F(RateLimiter, forbids_rps_exceeding_the_allowance)
{
    assertRequestsAreAllowed(limit());
    assertRequestIsNotAllowed();
}

TEST_F(RateLimiter, limits_are_lifted_after_period_passes)
{
    assertRequestsAreAllowed(limit());
    waitForLimitPeriod();
    assertRequestsAreAllowed(limit());
}

} // namespace nx::utils::math::test
