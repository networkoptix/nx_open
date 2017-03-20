#pragma once

#include <nx_ec/ec_api.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class FtHealthMonitoring:
    public Ec2MserverCloudSynchronization
{
public:
    FtHealthMonitoring();

protected:
    void givenSystemWithSomeHistory();
    void establishConnectionFromMediaserverToCloud();
    void closeConnectionFromMediaserverToCloud();
    void whenCdbIsRestarted();
    void whenSystemIsSharedWithSomeone();
    void thenSomeoneDoesNotHaveAccessToTheHistory();
    void thenSystemCredentialsCannotBeUsedToAccessHistory();
    void assertSystemOnline();
    void assertSystemOffline();
    void assertHistoryIsCorrect();

private:
    api::SystemHealthHistory m_expectedHealthHistory;
    AccountWithPassword m_anotherUser;

    void init();
    void waitForSystemToBecome(api::SystemHealth status);
    void assertSystemStatusIs(api::SystemHealth status);
    void saveHistoryItem(api::SystemHealth status);
};

} // namespace test
} // namespace cdb
} // namespace nx
