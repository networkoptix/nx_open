#include <gtest/gtest.h>

#include <chrono>
#include <numeric>
#include <thread>

#include <audit/mserver_audit_manager.h>
#include <utils/common/synctime.h>


namespace nx {
namespace vms::server {
namespace test {

class AuditManagerTest: public ::testing::Test
{
public:
    void mergeRecords()
    {
        QnSyncTime syncTime;

        using Record = QnMServerAuditManager::CameraPlaybackInfo;
        QVector<Record> recordsToAggregate;

        Record record;
        record.session.id = QnUuid::createUuid();
        record.cameraId = QnUuid::createUuid();
        record.timeout.restart();

        recordsToAggregate.push_back(record);
        record.cameraId = QnUuid::createUuid();
        recordsToAggregate.push_back(record);

        record.session.id = QnUuid::createUuid();
        record.cameraId = QnUuid::createUuid();
        recordsToAggregate.push_back(record);

        QnMServerAuditManager::processDelayedRecordsInternal(recordsToAggregate, 1000 * 1000);
        ASSERT_EQ(3, recordsToAggregate.size());

        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        auto result = QnMServerAuditManager::processDelayedRecordsInternal(recordsToAggregate, 0);
        ASSERT_EQ(0, recordsToAggregate.size());

        ASSERT_EQ(2, result.size());

        int resourceCount = 0;
        for (const auto& record: result)
            resourceCount += record.resources.size();

        ASSERT_EQ(3, resourceCount);
    }
};

TEST_F(AuditManagerTest, testMergeRecords)
{
    mergeRecords();
}

} // namespace test
} // namespace vms::server
} // namespace nx
