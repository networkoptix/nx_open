#include "health_monitoring_fixture.h"

#include <nx/utils/random.h>
#include <nx/utils/time.h>

namespace nx {
namespace cdb {
namespace test {

FtHealthMonitoring::FtHealthMonitoring()
{
    init();
}

void FtHealthMonitoring::givenSystemWithSomeHistory()
{
    establishConnectionFromMediaserverToCloud();
    closeConnectionFromMediaserverToCloud();
    assertHistoryIsCorrect();
}

void FtHealthMonitoring::establishConnectionFromMediaserverToCloud()
{
    appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());
    waitForCloudAndVmsToSyncUsers();

    waitForSystemToBecome(api::SystemHealth::online);
    saveHistoryItem(api::SystemHealth::online);
}

void FtHealthMonitoring::closeConnectionFromMediaserverToCloud()
{
    appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());
    saveHistoryItem(api::SystemHealth::offline);
    waitForSystemToBecome(api::SystemHealth::offline);
}

void FtHealthMonitoring::whenCdbIsRestarted()
{
    ASSERT_TRUE(cdb()->restart());
}

void FtHealthMonitoring::whenSystemIsSharedWithSomeone()
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

void FtHealthMonitoring::thenSomeoneDoesNotHaveAccessToTheHistory()
{
    api::SystemHealthHistory history;
    ASSERT_EQ(
        api::ResultCode::forbidden,
        cdb()->getSystemHealthHistory(
            m_anotherUser.email, m_anotherUser.password,
            registeredSystemData().id, &history));
}

void FtHealthMonitoring::thenSystemCredentialsCannotBeUsedToAccessHistory()
{
    api::SystemHealthHistory history;
    ASSERT_EQ(
        api::ResultCode::forbidden,
        cdb()->getSystemHealthHistory(
            registeredSystemData().id, registeredSystemData().authKey,
            registeredSystemData().id, &history));
}

void FtHealthMonitoring::assertSystemOnline()
{
    assertSystemStatusIs(api::SystemHealth::online);
}

void FtHealthMonitoring::assertSystemOffline()
{
    assertSystemStatusIs(api::SystemHealth::offline);
}

void FtHealthMonitoring::assertHistoryIsCorrect()
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

void FtHealthMonitoring::init()
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());

    m_anotherUser = cdb()->addActivatedAccount2();
}

void FtHealthMonitoring::waitForSystemToBecome(api::SystemHealth status)
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

void FtHealthMonitoring::assertSystemStatusIs(api::SystemHealth status)
{
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        cdb()->fetchSystemData(
            ownerAccount().email, ownerAccount().password,
            registeredSystemData().id, &systemData));
    ASSERT_EQ(status, systemData.stateOfHealth);
}

void FtHealthMonitoring::saveHistoryItem(api::SystemHealth status)
{
    api::SystemHealthHistoryItem item;
    item.state = status;
    item.timestamp = nx::utils::utcTime();
    m_expectedHealthHistory.events.push_back(std::move(item));
}

} // namespace test
} // namespace cdb
} // namespace nx
