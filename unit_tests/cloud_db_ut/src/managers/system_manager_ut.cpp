#include <gtest/gtest.h>

#include <nx/cloud/cdb/ec2/synchronization_engine.h>
#include <nx/cloud/cdb/managers/system_manager.h>
#include <nx/cloud/cdb/stree/stree_manager.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/timer_manager.h>

#include "account_manager_stub.h"
#include "base_persistent_data_test.h"
#include "system_health_info_provider_stub.h"
#include "../functional_tests/test_email_manager.h"

namespace nx {
namespace cdb {
namespace test {

class SystemManager:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    SystemManager():
        m_streeManager(m_settings.auth().rulesXmlPath),
        m_emailManager(nullptr),
        m_ec2SyncronizationEngine(
            QnUuid::createUuid(),
            m_settings.p2pDb(),
            &persistentDbManager()->queryExecutor()),
        m_systemManager(
            m_settings,
            &m_timerManager,
            &m_accountManagerStub,
            m_systemHealthInfoProvider,
            &persistentDbManager()->queryExecutor(),
            &m_emailManager,
            &m_ec2SyncronizationEngine)
    {
        m_timerManager.start();
    }

    ~SystemManager()
    {
        m_timerManager.stop();
    }

protected:
    void givenSystemInState(api::SystemStatus systemStatus)
    {
        // TODO
    }

    void thenEveryModificationRequestReturns(api::ResultCode resultCode)
    {
        // TODO
    }

private:
    conf::Settings m_settings;
    nx::utils::StandaloneTimerManager m_timerManager;
    StreeManager m_streeManager;
    AccountManagerStub m_accountManagerStub;
    SystemHealthInfoProviderStub m_systemHealthInfoProvider;
    TestEmailManager m_emailManager;
    ec2::SyncronizationEngine m_ec2SyncronizationEngine;
    cdb::SystemManager m_systemManager;
};

TEST_F(SystemManager, system_in_beingMerged_state_cannot_be_modified)
{
    givenSystemInState(api::SystemStatus::beingMerged);
    thenEveryModificationRequestReturns(api::ResultCode::forbidden);
}

} // namespace test
} // namespace cdb
} // namespace nx
