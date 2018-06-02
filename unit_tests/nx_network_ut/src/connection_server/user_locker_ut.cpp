#include <gtest/gtest.h>

#include <nx/network/connection_server/user_locker.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>

namespace nx {
namespace network {
namespace server {
namespace test {

using AuthResult = server::UserLocker::AuthResult;

class UserLocker:
    public ::testing::Test
{
public:
    UserLocker():
        m_locker(m_settings),
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

protected:
    void givenNotLockedUser()
    {
        ASSERT_FALSE(m_locker.isLocked());
    }

    void givenLockedUser()
    {
        givenNotLockedUser();
        whenDoManyUnsuccesfulAuthentications();
    }

    void whenDoManyUnsuccesfulAuthentications()
    {
        for (int i = 0; i < m_settings.authFailureCount + 1; ++i)
            m_locker.updateLockoutState(AuthResult::failure);
    }

    void whenDoManyUnsuccesfulAuthenticationsDuringLargePeriod()
    {
        for (int i = 0; i < m_settings.authFailureCount * 2; ++i)
        {
            m_locker.updateLockoutState(AuthResult::failure);
            m_timeShift.applyRelativeShift(
                m_settings.checkPeriod / (m_settings.authFailureCount / 2));
        }
    }

    void whenLockPeriodPasses()
    {
        m_timeShift.applyRelativeShift(m_settings.lockPeriod);
    }

    void thenUserIsLocked()
    {
        ASSERT_TRUE(m_locker.isLocked());
    }

    void thenUserIsNotLocked()
    {
        ASSERT_FALSE(m_locker.isLocked());
    }

private:
    server::UserLockerSettings m_settings;
    server::UserLocker m_locker;
    nx::utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(UserLocker, user_locked_after_number_of_failed_logins)
{
    givenNotLockedUser();
    whenDoManyUnsuccesfulAuthentications();
    thenUserIsLocked();
}

TEST_F(UserLocker, user_not_locked_after_number_of_failed_logins_during_larger_period)
{
    givenNotLockedUser();
    whenDoManyUnsuccesfulAuthenticationsDuringLargePeriod();
    thenUserIsNotLocked();
}

TEST_F(UserLocker, locked_user_is_unlocked_after_lock_period_passes)
{
    givenLockedUser();
    whenLockPeriodPasses();
    thenUserIsNotLocked();
}

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
