#include "health_monitoring_fixture.h"

#include <nx/utils/random.h>
#include <nx/utils/time.h>

namespace nx::cloud::db {
namespace test {

HealthMonitoring::HealthMonitoring()
{
    m_anotherUser = addActivatedAccount2();
}

void HealthMonitoring::givenSystemWithSomeHistory()
{
    establishConnectionFromMediaserverToCloud();
    closeConnectionFromMediaserverToCloud();
    assertHistoryIsCorrect();
}

void HealthMonitoring::establishConnectionFromMediaserverToCloud()
{
    openTransactionConnections(1);
    waitForConnectionsToMoveToACertainState(
        {::ec2::QnTransactionTransportBase::Connected,
            ::ec2::QnTransactionTransportBase::ReadyForStreaming },
        std::chrono::hours(1));

    //waitForSystemToBecome(api::SystemHealth::online);
    saveHistoryItem(api::SystemHealth::online);
}

void HealthMonitoring::establishConnectionFromMediaserverToCloudReusingPeerId()
{
    QnUuid peerId;
    connectionHelper().getAccessToConnectionByIndex(
        0,
        [&peerId](test::TransactionConnectionHelper::ConnectionContext* connectionContext)
        {
            peerId = connectionContext->peerInfo.id;
        });

    const auto connectionId = connectionHelper().establishTransactionConnection(
        cdbSynchronizationUrl(),
        system().id,
        system().authKey,
        KeepAlivePolicy::enableKeepAlive,
        nx_ec::EC2_PROTO_VERSION,
        peerId);

    connectionHelper().waitForState(
        {::ec2::QnTransactionTransportBase::Connected,
            ::ec2::QnTransactionTransportBase::ReadyForStreaming},
        connectionId,
        std::chrono::hours(1));
}

void HealthMonitoring::closeConnectionFromMediaserverToCloud()
{
    closeAllConnections();
    saveHistoryItem(api::SystemHealth::offline);
    waitForSystemToBecome(api::SystemHealth::offline);
}

void HealthMonitoring::whenCdbIsRestarted()
{
    ASSERT_TRUE(restart());
}

void HealthMonitoring::whenSystemIsSharedWithSomeone()
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

    shareSystemEx(
        account(),
        system(),
        m_anotherUser,
        accessRole);
}

void HealthMonitoring::thenSomeoneDoesNotHaveAccessToTheHistory()
{
    api::SystemHealthHistory history;
    ASSERT_EQ(
        api::ResultCode::forbidden,
        getSystemHealthHistory(
            m_anotherUser.email, m_anotherUser.password,
            system().id, &history));
}

void HealthMonitoring::thenSystemCredentialsCannotBeUsedToAccessHistory()
{
    api::SystemHealthHistory history;
    ASSERT_EQ(
        api::ResultCode::forbidden,
        getSystemHealthHistory(
            system().id, system().authKey,
            system().id, &history));
}

void HealthMonitoring::assertSystemOnline()
{
    assertSystemStatusIs(api::SystemHealth::online);
}

void HealthMonitoring::assertSystemOffline()
{
    assertSystemStatusIs(api::SystemHealth::offline);
}

void HealthMonitoring::assertHistoryIsCorrect()
{
    api::SystemHealthHistory history;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystemHealthHistory(
            account().email, account().password,
            system().id, &history));
    ASSERT_EQ(m_expectedHealthHistory.events.size(), history.events.size());

    for (size_t i = 0; i < m_expectedHealthHistory.events.size(); ++i)
    {
        ASSERT_EQ(m_expectedHealthHistory.events[i].state, history.events[i].state);
    }
}

void HealthMonitoring::assertHistoryHasASingleOnlineRecord()
{
    api::SystemHealthHistory history;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystemHealthHistory(
            account().email, account().password,
            system().id, &history));
    ASSERT_EQ(1U, history.events.size());
}

void HealthMonitoring::waitForSystemToBecome(api::SystemHealth status)
{
    for (;;)
    {
        api::SystemDataEx systemData;
        ASSERT_EQ(
            api::ResultCode::ok,
            fetchSystemData(
                account().email, account().password,
                system().id, &systemData));
        if (systemData.stateOfHealth == status)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void HealthMonitoring::assertSystemStatusIs(api::SystemHealth status)
{
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        fetchSystemData(
            account().email, account().password,
            system().id, &systemData));
    ASSERT_EQ(status, systemData.stateOfHealth);
}

void HealthMonitoring::saveHistoryItem(api::SystemHealth status)
{
    api::SystemHealthHistoryItem item;
    item.state = status;
    item.timestamp = nx::utils::utcTime();
    m_expectedHealthHistory.events.push_back(std::move(item));
}

} // namespace test
} // namespace nx::cloud::db
