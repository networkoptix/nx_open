#include <memory>
#include <thread>

#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/cloud/db/controller.h>
#include <nx/cloud/db/managers/system_merge_manager.h>
#include <nx/cloud/db/settings.h>
#include <nx/cloud/db/stree/cdb_ns.h>
#include <nx/cloud/db/test_support/base_persistent_data_test.h>
#include <nx/cloud/db/test_support/business_data_generator.h>

#include "system_health_info_provider_stub.h"
#include "system_manager_stub.h"
#include "vms_gateway_stub.h"

namespace nx::cloud::db {
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
        m_masterSystem = nx::cloud::db::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
        m_systemHealthInfoProviderStub.setSystemStatus(m_masterSystem.id, true);
        m_systemManagerStub.addSystem(m_masterSystem);
    }

    void givenOnlineSlaveSystem()
    {
        m_slaveSystem = nx::cloud::db::test::BusinessDataGenerator::generateRandomSystem(m_ownerAccount);
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
        nx::utils::stree::ResourceContainer rc;
        rc.put(attr::authAccountEmail, QString::fromStdString(m_ownerAccount.email));
        AuthorizationInfo authorizationInfo(std::move(rc));
        m_systemMergeManager->startMergingSystems(
            authorizationInfo,
            m_masterSystem.id,
            m_slaveSystem.id,
            [this](auto&&... args) { saveMergeResult(std::move(args)...); });
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
            nx::sql::DBResult::ok,
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

    void whenMergeHistoryRecordWithUnknownSystemIdIsDelivered()
    {
        auto mergeHistoryRecord = prepareMergeHistoryRecord(m_slaveSystem.authKey);
        mergeHistoryRecord.mergedSystemCloudId =
            nx::utils::random::generateName(nx::utils::random::number<int>(0, 7));
        ASSERT_NO_THROW(deliverMergeHistoryRecord(mergeHistoryRecord));
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
    std::unique_ptr<clusterdb::engine::SyncronizationEngine> m_syncronizationEngine;
    std::unique_ptr<nx::cloud::db::SystemMergeManager> m_systemMergeManager;
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

        m_syncronizationEngine = std::make_unique<clusterdb::engine::SyncronizationEngine>(
            std::string(),
            m_settings.p2pDb(),
            nx::clusterdb::engine::ProtocolVersionRange(
                kMinSupportedProtocolVersion,
                kMaxSupportedProtocolVersion),
            &queryExecutor());

        m_systemMergeManager = std::make_unique<nx::cloud::db::SystemMergeManager>(
            &m_systemManagerStub,
            m_systemHealthInfoProviderStub,
            &m_vmsGatewayStub,
            &queryExecutor());
    }

    void saveMergeResult(api::Result result)
    {
        m_mergeResults.push(result.code);
    }

    void deliverMergeHistoryRecord(const std::string& authKey)
    {
        const auto mergeHistoryRecord = prepareMergeHistoryRecord(authKey);
        deliverMergeHistoryRecord(mergeHistoryRecord);
    }

    void deliverMergeHistoryRecord(
        const nx::vms::api::SystemMergeHistoryRecord& mergeHistoryRecord)
    {
        queryExecutor().executeUpdateQuerySync(
            [this, &mergeHistoryRecord](nx::sql::QueryContext* queryContext)
            {
                m_systemMergeManager->processMergeHistoryRecord(
                    queryContext,
                    mergeHistoryRecord);
            });
    }

    nx::vms::api::SystemMergeHistoryRecord prepareMergeHistoryRecord(
        const std::string& authKey)
    {
        nx::vms::api::SystemMergeHistoryRecord mergeHistoryRecord;
        mergeHistoryRecord.mergedSystemCloudId = m_slaveSystem.id.c_str();
        mergeHistoryRecord.mergedSystemLocalId = QnUuid::createUuid().toSimpleByteArray();
        mergeHistoryRecord.username = m_ownerAccount.email.c_str();
        mergeHistoryRecord.timestamp = nx::utils::random::number<qint64>();
        mergeHistoryRecord.sign(authKey.c_str());
        return mergeHistoryRecord;
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

TEST_F(SystemMergeManager, merge_history_with_unknown_cloud_system_id_is_ignored)
{
    givenSystemsBeingMerged();
    whenMergeHistoryRecordWithUnknownSystemIdIsDelivered();
    thenSlaveSystemMovedToState(api::SystemStatus::beingMerged);
}

TEST_F(SystemMergeManager, merge_history_record_is_processed_even_if_system_is_in_unexpected_state)
{
    givenSystemsBeingMerged();

    whenMoveSlaveSystemTo(api::SystemStatus::activated);
    whenMergeHistoryRecordIsDelivered();

    thenSlaveSystemIsRemoved();
}

} // namespace test
} // namespace nx::cloud::db
