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
            &persistentDbManager()->queryExecutor())
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
        m_system = cdb::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
        m_system.status = systemStatus;
        insertSystem(m_ownerAccount, m_system);
    }

    void whenUnbindSystem()
    {
        data::SystemId systemIdHolder;
        systemIdHolder.systemId = m_system.id;

        AuthorizationInfo authorizationInfo;
        nx::utils::promise<api::ResultCode> done;
        m_systemManager->unbindSystem(
            authorizationInfo,
            systemIdHolder,
            [&done](api::ResultCode resultCode) { done.set_value(resultCode); });
        m_prevResultCode = done.get_future().get();
    }

    void whenShareSystem()
    {
        data::SystemSharing systemSharing;
        systemSharing.accessRole = api::SystemAccessRole::cloudAdmin;
        systemSharing.systemId = m_system.id;
        systemSharing.accountEmail = m_otherAccount.email;

        AuthorizationInfo authorizationInfo;
        nx::utils::promise<api::ResultCode> done;
        m_systemManager->shareSystem(
            authorizationInfo,
            systemSharing,
            [&done](api::ResultCode resultCode) { done.set_value(resultCode); });
        m_prevResultCode = done.get_future().get();
    }

    void whenRenameSystem()
    {
        data::SystemAttributesUpdate systemUpdate;
        systemUpdate.systemId = m_system.id;
        systemUpdate.name = QnUuid::createUuid().toStdString();

        AuthorizationInfo authorizationInfo;
        nx::utils::promise<api::ResultCode> done;
        m_systemManager->updateSystem(
            authorizationInfo,
            systemUpdate,
            [&done](api::ResultCode resultCode) { done.set_value(resultCode); });
        m_prevResultCode = done.get_future().get();
    }

    void thenResultIs(api::ResultCode resultCode)
    {
        ASSERT_EQ(resultCode, m_prevResultCode);
    }

    void thenEveryReadRequestReturns(api::ResultCode /*resultCode*/)
    {
        // TODO
    }

    void thenEveryModificationRequestReturns(api::ResultCode resultCode)
    {
        initializeSystemManagerIfNeeded();

        thenUnbindSystemReturns(resultCode);
        thenShareSystemReturns(resultCode);
        thenUpdateSystemReturns(resultCode);
    }

    void thenUnbindSystemReturns(api::ResultCode resultCode)
    {
        whenUnbindSystem();
        thenResultIs(resultCode);
    }

    void thenShareSystemReturns(api::ResultCode resultCode)
    {
        whenShareSystem();
        thenResultIs(resultCode);
    }

    void thenUpdateSystemReturns(api::ResultCode resultCode)
    {
        whenRenameSystem();
        thenResultIs(resultCode);
    }

private:
    conf::Settings m_settings;
    nx::utils::StandaloneTimerManager m_timerManager;
    StreeManager m_streeManager;
    AccountManagerStub m_accountManagerStub;
    SystemHealthInfoProviderStub m_systemHealthInfoProvider;
    TestEmailManager m_emailManager;
    ec2::SyncronizationEngine m_ec2SyncronizationEngine;
    std::unique_ptr<cdb::SystemManager> m_systemManager;
    std::string m_systemId;
    api::AccountData m_ownerAccount;
    api::AccountData m_otherAccount;
    data::SystemData m_system;
    api::ResultCode m_prevResultCode = api::ResultCode::ok;

    virtual void SetUp() override
    {
        initializeDatabase();

        m_ownerAccount = insertRandomAccount();
        m_otherAccount = insertRandomAccount();
    }

    void initializeSystemManagerIfNeeded()
    {
        if (m_systemManager)
            return;

        m_systemManager = std::make_unique<cdb::SystemManager>(
            m_settings,
            &m_timerManager,
            &m_accountManagerStub,
            m_systemHealthInfoProvider,
            &persistentDbManager()->queryExecutor(),
            &m_emailManager,
            &m_ec2SyncronizationEngine);
    }
};

} // namespace test
} // namespace cdb
} // namespace nx
