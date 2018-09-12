#include <gtest/gtest.h>

#include <nx/cloud/cdb/access_control/login_enumeration_protector.h>
#include <nx/utils/time.h>

namespace nx::cdb::test {

class LoginEnumerationProtector:
    public ::testing::Test
{
public:
    LoginEnumerationProtector():
        m_timeShift(nx::utils::test::ClockType::steady)
    {
        m_settings.minBlockPeriod = std::chrono::hours(1);
        m_settings.maxBlockPeriod = std::chrono::hours(1);
        m_blocker = std::make_unique<cdb::LoginEnumerationProtector>(m_settings);
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
    cdb::LoginEnumerationProtectionSettings m_settings;
    std::unique_ptr<cdb::LoginEnumerationProtector> m_blocker;
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

} // namespace nx::cdb::test
