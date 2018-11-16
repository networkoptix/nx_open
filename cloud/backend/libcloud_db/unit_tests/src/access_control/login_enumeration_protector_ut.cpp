#include <gtest/gtest.h>

#include <nx/cloud/db/access_control/login_enumeration_protector.h>
#include <nx/utils/time.h>

namespace nx::cloud::db::test {

class LoginEnumerationProtector:
    public ::testing::Test
{
public:
    LoginEnumerationProtector():
        m_timeShift(nx::utils::test::ClockType::steady)
    {
        m_settings.minBlockPeriod = std::chrono::hours(1);
        m_settings.maxBlockPeriod = std::chrono::hours(1);
        m_blocker = std::make_unique<nx::cloud::db::LoginEnumerationProtector>(m_settings);
    }

protected:
    void givenLock()
    {
        whenIssueManyUnauthenticatedRequestsWithDifferentLogins();
        thenBlockHasBeenSet();
    }

    void whenIssueManyUnauthenticatedRequestsWithDifferentLogins()
    {
        for (int i = 0; i < m_settings.unsuccessfulLoginsThreshold+1; ++i)
        {
            std::chrono::milliseconds lockPeriod;
            m_blocker->updateLockoutState(
                nx::network::server::AuthResult::failure,
                nx::utils::generateRandomName(7).toStdString(),
                &lockPeriod);
        }
    }
    
    void whenLockExpirationPeriodPasses()
    {
        m_timeShift.applyRelativeShift(m_settings.maxBlockPeriod);
    }

    void thenBlockHasBeenSet()
    {
        ASSERT_TRUE(m_blocker->isLocked());
    }

    void thenLockIsRemoved()
    {
        ASSERT_FALSE(m_blocker->isLocked());
    }

private:
    nx::cloud::db::LoginEnumerationProtectionSettings m_settings;
    std::unique_ptr<nx::cloud::db::LoginEnumerationProtector> m_blocker;
    nx::utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(LoginEnumerationProtector, locked)
{
    whenIssueManyUnauthenticatedRequestsWithDifferentLogins();
    thenBlockHasBeenSet();
}

TEST_F(LoginEnumerationProtector, lock_removed_after_timeout)
{
    givenLock();
    whenLockExpirationPeriodPasses();
    thenLockIsRemoved();
}

} // namespace nx::cloud::db::test
