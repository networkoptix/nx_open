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

class HealthMonitoring:
    public Ec2MserverCloudSynchronization
{
public:
    HealthMonitoring()
    {
        init();
    }

    ~HealthMonitoring()
    {
    }

protected:
    void establishConnectionFromMediaserverToCloud()
    {
        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
        waitForCloudAndVmsToSyncUsers();

        saveHistoryItem(api::SystemHealth::online);
    }

    void closeConnectionFromMediaserverToCloud()
    {
        appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());
        saveHistoryItem(api::SystemHealth::offline);
    }

    void whenCdbIsRestarted()
    {
        ASSERT_TRUE(cdb()->restart());
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
        ASSERT_EQ(m_expectedHealthHistory.items.size(), history.items.size());

        for (size_t i = 0; i < m_expectedHealthHistory.items.size(); ++i)
        {
            ASSERT_EQ(m_expectedHealthHistory.items[i].state, history.items[i].state);
        }
    }

private:
    api::SystemHealthHistory m_expectedHealthHistory;

    void init()
    {
        ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
        ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
        ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());
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
        m_expectedHealthHistory.items.push_back(std::move(item));
    }
};

TEST_F(HealthMonitoring, system_status_is_correct)
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

TEST_F(HealthMonitoring, history_is_saved)
{
    establishConnectionFromMediaserverToCloud();
    closeConnectionFromMediaserverToCloud();

    assertHistoryIsCorrect();
}

TEST_F(HealthMonitoring, history_is_persistent)
{
    establishConnectionFromMediaserverToCloud();
    closeConnectionFromMediaserverToCloud();

    whenCdbIsRestarted();

    assertHistoryIsCorrect();
}

//TEST_F(HealthMonitoring, history_is_not_reported_for_unknown_id)

//TEST_F(HealthMonitoring, history_is_available_to_system_owner_only)

//TEST_F(HealthMonitoring, history_is_not_available_to_system_credentials)

} // namespace cdb
} // namespace nx
