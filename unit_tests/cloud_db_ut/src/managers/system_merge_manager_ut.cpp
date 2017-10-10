#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/managers/system_merge_manager.h>
#include <nx/cloud/cdb/settings.h>

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
    SystemMergeManager()
    {
    }

protected:
    void givenPausedVmsGateway()
    {
        m_vmsGatewayStub.pause();
    }
    
    void givenRunningMergeRequest()
    {
        using namespace std::placeholders;

        AuthorizationInfo authorizationInfo;
        m_systemMergeManager->startMergingSystems(
            authorizationInfo,
            m_masterSystemId,
            m_slaveSystemId,
            std::bind(&SystemMergeManager::saveMergeResult, this, _1));
    }

    void whenDestroySystemMergeManagerAsync()
    {
        std::async([this]() { m_systemMergeManager.reset(); }).wait();
    }

    void whenResumeVmsGateway()
    {
        m_vmsGatewayStub.resume();
    }

    void thenMergeResultIsReported()
    {
        m_mergeResults.pop();
    }

private:
    conf::Settings m_settings;
    SystemManagerStub m_systemManagerStub;
    SystemHealthInfoProviderStub m_systemHealthInfoProviderStub;
    VmsGatewayStub m_vmsGatewayStub;
    std::unique_ptr<cdb::SystemMergeManager> m_systemMergeManager;
    std::string m_masterSystemId;
    std::string m_slaveSystemId;
    nx::utils::SyncQueue<api::ResultCode> m_mergeResults;

    virtual void SetUp() override
    {
        initializeDatabase();

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

TEST_F(SystemMergeManager, waits_for_every_request_completion_before_terminating)
{
    givenPausedVmsGateway();
    givenRunningMergeRequest();

    whenDestroySystemMergeManagerAsync();
    whenResumeVmsGateway();

    thenMergeResultIsReported();
}

} // namespace test
} // namespace cdb
} // namespace nx
