#include <gtest/gtest.h>

#include <nx/network/connection_server/user_locker.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>

namespace nx {
namespace network {
namespace server {
namespace test {

using AuthResult = server::AuthResult;

class UserLocker:
    public ::testing::Test
{
public:
    UserLocker():
        m_userKey(std::make_tuple(
            HostAddress::localhost,
            nx::utils::generateRandomName(7))),
        m_locker(m_settings),
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

protected:
    void givenNotLockedUser()
    {
        ASSERT_FALSE(m_locker.isLocked(m_userKey));
    }

    void givenLockedUser()
    {
        givenNotLockedUser();
        whenDoManyUnsuccesfulAuthentications();
    }

    void whenDoManyUnsuccesfulAuthentications()
    {
        for (int i = 0; i < m_settings.authFailureCount + 1; ++i)
            m_locker.updateLockoutState(m_userKey, AuthResult::failure);
    }

    void whenDoManyUnsuccesfulAuthenticationsDuringLargePeriod()
    {
        for (int i = 0; i < m_settings.authFailureCount * 2; ++i)
        {
            m_locker.updateLockoutState(m_userKey, AuthResult::failure);
            m_timeShift.applyRelativeShift(
                m_settings.checkPeriod / (m_settings.authFailureCount / 2));
        }
    }

    void whenLockPeriodPasses()
    {
        m_timeShift.applyRelativeShift(m_settings.lockPeriod);
    }

    void whenCheckPeriodPasses()
    {
        m_timeShift.applyRelativeShift(m_settings.checkPeriod);
    }

    void whenDoSuccessfulAuthentication()
    {
        m_locker.updateLockoutState(m_userKey, AuthResult::success);
    }

    void thenUserIsLocked()
    {
        ASSERT_TRUE(m_locker.isLocked(m_userKey));
    }

    void thenUserIsNotLocked()
    {
        ASSERT_FALSE(m_locker.isLocked(m_userKey));
    }

    void thenThereIsNoLockoutRecord()
    {
        ASSERT_FALSE(m_locker.lockerExists(m_userKey));
    }

    void thenLockReasonIsProvided()
    {
        const auto reason = m_locker.getCurrentLockReason(m_userKey);
        ASSERT_FALSE(reason.empty());
    }

private:
    const UserLockerPool<>::key_type m_userKey;
    server::UserLockerSettings m_settings;
    server::UserLockerPool<> m_locker;
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

TEST_F(UserLocker, lockout_context_is_not_added_on_successful_authentication)
{
    givenNotLockedUser();
    whenDoSuccessfulAuthentication();
    thenThereIsNoLockoutRecord();
}

TEST_F(UserLocker, lockout_context_removed_after_unlocking_user)
{
    givenLockedUser();

    whenLockPeriodPasses();
    whenDoSuccessfulAuthentication();

    thenThereIsNoLockoutRecord();
}

TEST_F(UserLocker, unused_context_removed_by_timeout)
{
    givenLockedUser();

    whenCheckPeriodPasses();

    thenThereIsNoLockoutRecord();
    thenUserIsNotLocked();
}

TEST_F(UserLocker, lockout_reason_is_provided)
{
    givenLockedUser();
    thenLockReasonIsProvided();
}

} // namespace test
} // namespace server
} // namespace network
} // namespace nx
