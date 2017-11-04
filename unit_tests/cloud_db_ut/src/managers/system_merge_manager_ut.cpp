#include <memory>
#include <thread>

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/ec2/synchronization_engine.h>
#include <nx/cloud/cdb/managers/system_merge_manager.h>
#include <nx/cloud/cdb/settings.h>
#include <nx/cloud/cdb/stree/cdb_ns.h>
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

        m_systemMergeManager.reset();
    }

protected:
    void givenTwoOnlineSystems()
    {
        givenOnlineMasterSystem();
        givenOnlineSlaveSystem();
    }

    void givenOnlineMasterSystem()
    {
        m_masterSystem = cdb::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
        m_systemHealthInfoProviderStub.setSystemStatus(m_masterSystem.id, true);
        m_systemManagerStub.addSystem(m_masterSystem);
    }

    void givenOnlineSlaveSystem()
    {
        m_slaveSystem = cdb::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
        m_systemHealthInfoProviderStub.setSystemStatus(m_slaveSystem.id, true);
        m_systemManagerStub.addSystem(m_slaveSystem);
    }

    void givenOnlineSlaveSystemThatFailsEveryRequest()
    {
        givenOnlineSlaveSystem();
        m_vmsGatewayStub.failEveryRequestToSystem(m_slaveSystem.id);
    }

    void givenPausedVmsGateway()
    {
        m_vmsGatewayStub.pause();
    }

    void givenRunningMergeRequest()
    {
        whenStartMerge();
    }

    void givenSystemsBeingMerged()
    {
        givenTwoOnlineSystems();
        whenStartMerge();
        thenMergeSucceeded();
    }

    void whenStartMerge()
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer rc;
        rc.put(attr::authAccountEmail, QString::fromStdString(m_ownerAccount.email));
        AuthorizationInfo authorizationInfo(std::move(rc));
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

    void whenSuccessfullyStartMerge()
    {
        whenStartMerge();
        thenMergeSucceeded();
    }

    void whenMoveSlaveSystemTo(api::SystemStatus systemStatus)
    {
        ASSERT_EQ(
            nx::utils::db::DBResult::ok,
            m_systemManagerStub.updateSystemStatus(nullptr, m_slaveSystem.id, systemStatus));
    }

    void whenMergeHistoryRecordIsDelivered()
    {
        ASSERT_NO_THROW(deliverMergeHistoryRecord(m_slaveSystem.authKey));
    }

    void whenBrokenMergeHistoryRecordIsDelivered()
    {
        ASSERT_NO_THROW(
            deliverMergeHistoryRecord(nx::utils::random::generateName(7).toStdString()));
    }

    void thenMergeResultIsReported()
    {
        m_prevMergeResult = m_mergeResults.pop();
    }

    void thenMergeResultIs(api::ResultCode resultCode)
    {
        m_prevMergeResult = m_mergeResults.pop();
        ASSERT_EQ(resultCode, *m_prevMergeResult);
    }

    void thenMergeSucceeded()
    {
        thenMergeResultIs(api::ResultCode::ok);
    }

    void thenRequestToSlaveSystemDoneOnBehalfOfSystemOwner()
    {
        auto requestParams = m_vmsGatewayStub.popRequest();
        ASSERT_EQ(m_ownerAccount.email, requestParams.username);
    }

    void andRequestToSlaveSystemHasBeenInvoked()
    {
        auto requestParams = m_vmsGatewayStub.popRequest();
        ASSERT_EQ(m_slaveSystem.id, requestParams.targetSystemId);
    }

    void thenSlaveSystemMovedToState(api::SystemStatus systemStatus)
    {
        const auto systemData = m_systemManagerStub.findSystemById(m_slaveSystem.id);
        ASSERT_TRUE(static_cast<bool>(systemData));
        ASSERT_EQ(systemStatus, systemData->status);
    }

    void thenSlaveSystemIsRemoved()
    {
        thenSlaveSystemMovedToState(api::SystemStatus::deleted_);
    }

private:
    conf::Settings m_settings;
    SystemManagerStub m_systemManagerStub;
    SystemHealthInfoProviderStub m_systemHealthInfoProviderStub;
    VmsGatewayStub m_vmsGatewayStub;
    std::unique_ptr<ec2::SyncronizationEngine> m_syncronizationEngine;
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

        m_syncronizationEngine = std::make_unique<ec2::SyncronizationEngine>(
            QnUuid::createUuid(),
            m_settings.p2pDb(),
            &queryExecutor());

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

    void deliverMergeHistoryRecord(const std::string& authKey)
    {
        ::ec2::ApiSystemMergeHistoryRecord mergeHistoryRecord;
        mergeHistoryRecord.mergedSystemCloudId = m_slaveSystem.id.c_str();
        mergeHistoryRecord.mergedSystemLocalId = QnUuid::createUuid().toSimpleByteArray();
        mergeHistoryRecord.username = m_ownerAccount.email.c_str();
        mergeHistoryRecord.timestamp = nx::utils::random::number<qint64>();
        mergeHistoryRecord.sign(authKey.c_str());

        queryExecutor().executeUpdateQuerySync(
            [this, &mergeHistoryRecord](nx::utils::db::QueryContext* queryContext)
            {
                m_systemMergeManager->processMergeHistoryRecord(
                    queryContext,
                    mergeHistoryRecord);
            });
    }
};

TEST_F(SystemMergeManager, invokes_merge_request_to_the_slave_system)
{
    givenTwoOnlineSystems();

    whenStartMerge();

    thenMergeSucceeded();
    andRequestToSlaveSystemHasBeenInvoked();
}

TEST_F(SystemMergeManager, merged_system_is_moved_to_beingMerged_state)
{
    givenTwoOnlineSystems();
    whenSuccessfullyStartMerge();
    thenSlaveSystemMovedToState(api::SystemStatus::beingMerged);
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

TEST_F(SystemMergeManager, passes_authenticated_user_name_to_vms_gateway)
{
    givenTwoOnlineSystems();
    whenStartMerge();
    thenRequestToSlaveSystemDoneOnBehalfOfSystemOwner();
}

TEST_F(SystemMergeManager, reports_error_on_vms_request_failure)
{
    givenOnlineMasterSystem();
    givenOnlineSlaveSystemThatFailsEveryRequest();

    whenStartMerge();

    thenMergeResultIs(api::ResultCode::vmsRequestFailure);
}

TEST_F(SystemMergeManager, merge_history_record_with_valid_signature_removes_system)
{
    givenSystemsBeingMerged();
    whenMergeHistoryRecordIsDelivered();
    thenSlaveSystemIsRemoved();
}

TEST_F(SystemMergeManager, merge_history_record_with_invalid_signature_ignored)
{
    givenSystemsBeingMerged();
    whenBrokenMergeHistoryRecordIsDelivered();
    thenSlaveSystemMovedToState(api::SystemStatus::beingMerged);
}

TEST_F(SystemMergeManager, merge_history_record_ignored_if_system_is_in_unexpected_state)
{
    givenSystemsBeingMerged();

    whenMoveSlaveSystemTo(api::SystemStatus::activated);
    whenMergeHistoryRecordIsDelivered();

    thenSlaveSystemMovedToState(api::SystemStatus::activated);
}

} // namespace test
} // namespace cdb
} // namespace nx
