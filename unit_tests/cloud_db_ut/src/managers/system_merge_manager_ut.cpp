#include <memory>
#include <thread>

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/managers/system_merge_manager.h>
#include <nx/cloud/cdb/settings.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include "base_persistent_data_test.h"
#include "system_health_info_provider_stub.h"
#include "system_manager_stub.h"
#include "vms_gateway_stub.h"

namespace nx {
namespace cdb {
namespace test {

class SystemMergeManager:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    ~SystemMergeManager()
    {
        if (m_systemMergeManagerDestroyed)
            m_systemMergeManagerDestroyed->wait();
    }

protected:
    void givenTwoOnlineSystems()
    {
        m_masterSystem = cdb::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
        m_systemHealthInfoProviderStub.setSystemStatus(m_masterSystem.id, true);
        m_systemManagerStub.addSystem(m_masterSystem);

        m_slaveSystem = cdb::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
        m_systemHealthInfoProviderStub.setSystemStatus(m_slaveSystem.id, true);
        m_systemManagerStub.addSystem(m_slaveSystem);
    }

    void givenPausedVmsGateway()
    {
        m_vmsGatewayStub.pause();
    }
    
    void givenRunningMergeRequest()
    {
        whenStartMerge();
    }

    void whenStartMerge()
    {
        using namespace std::placeholders;

        AuthorizationInfo authorizationInfo;
        m_systemMergeManager->startMergingSystems(
            authorizationInfo,
            m_masterSystem.id,
            m_slaveSystem.id,
            std::bind(&SystemMergeManager::saveMergeResult, this, _1));
    }

    void whenDestroySystemMergeManagerAsync()
    {
        m_systemMergeManagerDestroyed = 
            std::async([this]() { m_systemMergeManager.reset(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void whenResumeVmsGateway()
    {
        m_vmsGatewayStub.resume();
    }

    void thenMergeResultIsReported()
    {
        m_prevMergeResult = m_mergeResults.pop();
    }

    void thenMergeSucceeded()
    {
        m_prevMergeResult = m_mergeResults.pop();
        ASSERT_EQ(api::ResultCode::ok, *m_prevMergeResult);
    }
    
    void andRequestToSlaveSystemHasBeenInvoked()
    {
        ASSERT_TRUE(m_vmsGatewayStub.performedRequestToSystem(m_slaveSystem.id));
    }

private:
    conf::Settings m_settings;
    SystemManagerStub m_systemManagerStub;
    SystemHealthInfoProviderStub m_systemHealthInfoProviderStub;
    VmsGatewayStub m_vmsGatewayStub;
    std::unique_ptr<cdb::SystemMergeManager> m_systemMergeManager;
    nx::utils::SyncQueue<api::ResultCode> m_mergeResults;
    boost::optional<std::future<void>> m_systemMergeManagerDestroyed;
    api::AccountData m_ownerAccount;
    api::SystemData m_masterSystem;
    api::SystemData m_slaveSystem;
    boost::optional<api::ResultCode> m_prevMergeResult;

    virtual void SetUp() override
    {
        initializeDatabase();

        m_ownerAccount = insertRandomAccount();

        m_systemMergeManager = std::make_unique<cdb::SystemMergeManager>(
            &m_systemManagerStub,
            m_systemHealthInfoProviderStub,
            &m_vmsGatewayStub,
            &queryExecutor());
    }

    void saveMergeResult(api::ResultCode resultCode)
    {
        m_mergeResults.push(resultCode);
    }
};

TEST_F(SystemMergeManager, invokes_merge_request_to_the_slave_system)
{
    givenTwoOnlineSystems();

    whenStartMerge();

    thenMergeSucceeded();
    andRequestToSlaveSystemHasBeenInvoked();
}

TEST_F(SystemMergeManager, waits_for_every_request_completion_before_terminating)
{
    givenTwoOnlineSystems();
    givenPausedVmsGateway();
    givenRunningMergeRequest();

    whenDestroySystemMergeManagerAsync();
    whenResumeVmsGateway();

    thenMergeSucceeded();
}

} // namespace test
} // namespace cdb
} // namespace nx
