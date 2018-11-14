#include <nx/cloud/db/test_support/business_data_generator.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/time.h>

#include "functional_tests/test_setup.h"

namespace nx::cloud::db::test {

class AttackAccountEnumeration:
    public CdbFunctionalTest
{
public:
    AttackAccountEnumeration():
        m_timeShift(nx::utils::test::ClockType::steady)
    {
    }

    ~AttackAccountEnumeration()
    {
        stopRequests();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_connection = connection("", "");
        m_connection->bindToAioThread(m_aioObject.getAioThread());
    }

    void givenBlock()
    {
        whenSendResetPasswordRequests();
        thenAccessIsBlockedEventually();
    }

    void whenSendResetPasswordRequests()
    {
        startSendingResetPasswordWithRandomEmail();
    }

    void thenAccessIsBlockedEventually()
    {
        for (;;)
        {
            if (m_resetPasswordResults.pop() == api::ResultCode::accountBlocked)
                return;
        }

        stopRequests();
        m_resetPasswordResults.clear();
    }

    void waitForBlockToBeRemoved()
    {
        for (;;)
        {
            m_timeShift.applyRelativeShift(std::chrono::minutes(1));
            std::string confirmationCode;
            const auto result = resetAccountPassword(
                BusinessDataGenerator::generateRandomEmailAddress(),
                &confirmationCode);
            if (result != api::ResultCode::accountBlocked)
                return;
        }
    }

private:
    nx::utils::SyncQueue<api::ResultCode> m_resetPasswordResults;
    std::unique_ptr<nx::cloud::db::api::Connection> m_connection;
    nx::utils::test::ScopedTimeShift m_timeShift;
    nx::network::aio::BasicPollable m_aioObject;

    void startSendingResetPasswordWithRandomEmail()
    {
        using namespace std::placeholders;

        m_connection->accountManager()->resetPassword(
            {BusinessDataGenerator::generateRandomEmailAddress()},
            [this](
                api::ResultCode resultCode,
                api::AccountConfirmationCode /*confirmationCode*/)
            {   
                m_resetPasswordResults.push(resultCode);

                startSendingResetPasswordWithRandomEmail();
            });
    }

    void stopRequests()
    {
        m_aioObject.executeInAioThreadSync([this]() { m_connection.reset(); });

        m_connection = connection("", "");
        m_connection->bindToAioThread(m_aioObject.getAioThread());
    }
};

TEST_F(AttackAccountEnumeration, attack_is_blocked)
{
    whenSendResetPasswordRequests();
    thenAccessIsBlockedEventually();
}

TEST_F(AttackAccountEnumeration, block_is_removed_after_some_time)
{
    givenBlock();
    waitForBlockToBeRemoved();
}

} // namespace nx::cloud::db::test
