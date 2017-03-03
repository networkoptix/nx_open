#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/time.h>

#include <nx_ec/ec_api.h>
#include <utils/common/sync_call.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "test_setup.h"

namespace nx {
namespace cdb {

class FtHealthMonitoring:
    public Ec2MserverCloudSynchronization
{
public:
    FtHealthMonitoring()
    {
        init();
    }

    ~FtHealthMonitoring()
    {
    }

protected:
    void givenSystemWithSomeHistory()
    {
        establishConnectionFromMediaserverToCloud();
        closeConnectionFromMediaserverToCloud();
        assertHistoryIsCorrect();
    }

    void establishConnectionFromMediaserverToCloud()
    {
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
        waitForCloudAndVmsToSyncUsers();

        waitForSystemToBecome(api::SystemHealth::online);
        saveHistoryItem(api::SystemHealth::online);
    }

    void closeConnectionFromMediaserverToCloud()
    {
        appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());
        saveHistoryItem(api::SystemHealth::offline);
        waitForSystemToBecome(api::SystemHealth::offline);
    }

    void whenCdbIsRestarted()
    {
        ASSERT_TRUE(cdb()->restart());
    }

    void whenSystemIsSharedWithSomeone()
    {
        const std::vector<api::SystemAccessRole> accessRolesToTest = {
            api::SystemAccessRole::custom,
            api::SystemAccessRole::liveViewer,
            api::SystemAccessRole::viewer,
            api::SystemAccessRole::advancedViewer,
            api::SystemAccessRole::localAdmin,
            api::SystemAccessRole::cloudAdmin,
            api::SystemAccessRole::maintenance };

        const auto accessRole = nx::utils::random::choice(accessRolesToTest);

        cdb()->shareSystemEx(
            ownerAccount(),
            registeredSystemData(),
            m_anotherUser,
            accessRole);
    }

    void thenSomeoneDoesNotHaveAccessToTheHistory()
    {
        api::SystemHealthHistory history;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            cdb()->getSystemHealthHistory(
                m_anotherUser.email, m_anotherUser.password,
                registeredSystemData().id, &history));
    }

    void thenSystemCredentialsCannotBeUsedToAccessHistory()
    {
        api::SystemHealthHistory history;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            cdb()->getSystemHealthHistory(
                registeredSystemData().id, registeredSystemData().authKey,
                registeredSystemData().id, &history));
    }

    void assertSystemOnline()
    {
        assertSystemStatusIs(api::SystemHealth::online);
    }

    void assertSystemOffline()
    {
        assertSystemStatusIs(api::SystemHealth::offline);
    }

    void assertHistoryIsCorrect()
    {
        api::SystemHealthHistory history;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->getSystemHealthHistory(
                ownerAccount().email, ownerAccount().password,
                registeredSystemData().id, &history));
        ASSERT_EQ(m_expectedHealthHistory.events.size(), history.events.size());

        for (size_t i = 0; i < m_expectedHealthHistory.events.size(); ++i)
        {
            ASSERT_EQ(m_expectedHealthHistory.events[i].state, history.events[i].state);
        }
    }

private:
    api::SystemHealthHistory m_expectedHealthHistory;
    AccountWithPassword m_anotherUser;

    void init()
    {
        ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
        ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
        ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());

        m_anotherUser = cdb()->addActivatedAccount2();
    }

    void waitForSystemToBecome(api::SystemHealth status)
    {
        for (;;)
        {
            api::SystemDataEx systemData;
            ASSERT_EQ(
                api::ResultCode::ok,
                cdb()->fetchSystemData(
                    ownerAccount().email, ownerAccount().password,
                    registeredSystemData().id, &systemData));
            if (systemData.stateOfHealth == status)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void assertSystemStatusIs(api::SystemHealth status)
    {
        api::SystemDataEx systemData;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->fetchSystemData(
                ownerAccount().email, ownerAccount().password,
                registeredSystemData().id, &systemData));
        ASSERT_EQ(status, systemData.stateOfHealth);
    }

    void saveHistoryItem(api::SystemHealth status)
    {
        api::SystemHealthHistoryItem item;
        item.state = status;
        item.timestamp = nx::utils::utcTime();
        m_expectedHealthHistory.events.push_back(std::move(item));
    }
};

TEST_F(FtHealthMonitoring, system_status_is_correct)
{
    for (int i = 0; i < 2; ++i)
    {
        assertSystemOffline();
        establishConnectionFromMediaserverToCloud();
        assertSystemOnline();

        if (i == 0)
        {
            closeConnectionFromMediaserverToCloud();
            whenCdbIsRestarted();
        }
    }
}

TEST_F(FtHealthMonitoring, history_is_persistent)
{
    establishConnectionFromMediaserverToCloud();
    closeConnectionFromMediaserverToCloud();

    assertHistoryIsCorrect();

    whenCdbIsRestarted();

    assertHistoryIsCorrect();
}

TEST_F(FtHealthMonitoring, history_is_not_reported_for_unknown_id)
{
    api::SystemHealthHistory history;
    ASSERT_NE(
        api::ResultCode::ok,
        cdb()->getSystemHealthHistory(
            ownerAccount().email, ownerAccount().password,
            QnUuid::createUuid().toStdString(), &history));
}

TEST_F(FtHealthMonitoring, history_is_available_to_system_owner_only)
{
    givenSystemWithSomeHistory();
    whenSystemIsSharedWithSomeone();
    thenSomeoneDoesNotHaveAccessToTheHistory();
}

TEST_F(FtHealthMonitoring, history_is_not_available_to_system_credentials)
{
    givenSystemWithSomeHistory();
    thenSystemCredentialsCannotBeUsedToAccessHistory();
}

} // namespace cdb
} // namespace nx
