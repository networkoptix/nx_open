#pragma once

#include <nx_ec/ec_api.h>

#include "mserver_cloud_synchronization_connection_fixture.h"

namespace nx::cloud::db {
namespace test {

class HealthMonitoring:
    public Ec2MserverCloudSynchronizationConnection
{
public:
    HealthMonitoring();

protected:
    void givenSystemWithSomeHistory();
    void establishConnectionFromMediaserverToCloud();
    void establishConnectionFromMediaserverToCloudReusingPeerId();
    void closeConnectionFromMediaserverToCloud();
    void whenCdbIsRestarted();
    void whenSystemIsSharedWithSomeone();
    void thenSomeoneDoesNotHaveAccessToTheHistory();
    void thenSystemCredentialsCannotBeUsedToAccessHistory();
    void assertSystemOnline();
    void assertSystemOffline();
    void assertHistoryIsCorrect();
    void assertHistoryHasASingleOnlineRecord();

private:
    api::SystemHealthHistory m_expectedHealthHistory;
    AccountWithPassword m_anotherUser;

    void waitForSystemToBecome(api::SystemHealth status);
    void assertSystemStatusIs(api::SystemHealth status);
    void saveHistoryItem(api::SystemHealth status);
};

} // namespace test
} // namespace nx::cloud::db
