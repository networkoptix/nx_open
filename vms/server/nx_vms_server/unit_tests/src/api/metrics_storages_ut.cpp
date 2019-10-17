#include <camera/video_camera_mock.h>
#include <core/misc/schedule_task.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/metrics.h>
#include <nx/vms/server/metrics/helpers.h>
#include <recorder/storage_manager.h>
#include <server_for_tests.h>
#include <plugins/storage/file_storage/file_storage_resource.h>
#include <core/resource/media_server_resource.h>
#include <analytics/db/abstract_storage.h>
#include <nx/vms/server/event/event_connector.h>

#include "test_api_requests.h"
#include <thread>

namespace nx::vms::server::test {

using namespace nx::vms::api::metrics;
using namespace nx::test;
using namespace nx::analytics::db;

class MetricsStoragesApi: public ::testing::Test
{
public:
    static std::unique_ptr<ServerForTests> launcher;

    static void SetUpTestCase()
    {
        nx::vms::server::metrics::setTimerMultiplier(100);

        launcher = std::make_unique<ServerForTests>();

        vms::api::StorageDataList storages;
        vms::api::StorageData storage;
        for (int i = 0; i < 2; ++i)
            launcher->addStorage(lm("Storage %1").arg(i));
    }

    static void TearDownTestCase()
    {
        nx::vms::server::metrics::setTimerMultiplier(1);
        launcher.reset();
    }

};

std::unique_ptr<ServerForTests> MetricsStoragesApi::launcher;

TEST_F(MetricsStoragesApi, info)
{
    auto storages = launcher->commonModule()->resourcePool()->getResources<QnStorageResource>();
    ASSERT_FALSE(storages.isEmpty());
    auto storage = storages[0];
    static const QString kTestServerName("Test name");
    storage->getParentServer()->setName(kTestServerName);

    auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    auto storageData = systemValues["storages"][storage->getId().toSimpleString()];
    auto infoData = storageData["info"];
    EXPECT_EQ(kTestServerName, infoData["server"].toString());
    EXPECT_EQ("local", infoData["type"].toString());
}

TEST_F(MetricsStoragesApi, space)
{
    auto storages = launcher->commonModule()->resourcePool()->getResources<QnStorageResource>();
    ASSERT_FALSE(storages.isEmpty());
    auto storage = storages[0];
    auto storage2 = storages[1];

    const QString kUniqueId = "12345";
    auto catalog = launcher->serverModule()->normalStorageManager()->getFileCatalog(
        kUniqueId, QnServer::HiQualityCatalog);

    auto currentTimeMs = 0;
    static const qint64 kMsInMinute = 1000 * 60;
    static const qint64 kMsInHour = kMsInMinute * 60;
    static const qint64 kMsInDay = kMsInHour * 24;
    nx::vms::server::Chunk chunk;
    chunk.durationMs = 45 * 1000;
    static const int kChunkSize = 1000 * 1000;
    chunk.setFileSize(kChunkSize);
    static const int kChunks = 100;
    for (int i = 0; i < kChunks; ++i)
    {
        auto s = i % 2 ? storage : storage2;
        chunk.storageIndex = launcher->serverModule()->storageDbPool()->getStorageIndex(s);
        chunk.startTimeMs = currentTimeMs;
        catalog->addRecord(chunk);
        currentTimeMs += kMsInMinute * 2;
    }

    auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    auto storageData = systemValues["storages"][storage->getId().toSimpleString()];
    auto spaceData = storageData["space"];
    ASSERT_GT(1024*1024, spaceData["totalSpaceB"].toDouble());
    auto mediaSpace = spaceData["mediaSpaceB"].toDouble();
    ASSERT_EQ(kChunkSize * kChunks / 2, mediaSpace);

    auto storageData2 = systemValues["storages"][storage2->getId().toSimpleString()];
    auto spaceData2 = storageData2["space"];
    ASSERT_GT(1024 * 1024, spaceData2["totalSpaceB"].toDouble());
    auto mediaSpace2 = spaceData2["mediaSpaceB"].toDouble();
    ASSERT_EQ(kChunkSize * kChunks / 2, mediaSpace);
}

TEST_F(MetricsStoragesApi, state)
{
    auto storages = launcher->commonModule()->resourcePool()->getResources<QnStorageResource>();
    ASSERT_FALSE(storages.isEmpty());
    auto storage = storages[0];
    auto storage2 = storages[1];
    storage.dynamicCast<QnFileStorageResource>()->setMounted(true);
    storage->setStatus(Qn::Offline);

    auto systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
    auto storageData = systemValues["storages"][storage->getId().toSimpleString()];
    auto stateData = storageData["state"];
    ASSERT_EQ("Offline", stateData["status"].toString());
    ASSERT_EQ(0, stateData["issues24h"].toInt());

    auto eventConnector = launcher->serverModule()->eventConnector();
    static const int kIssues = 10;
    for (int i = 0; i < kIssues; ++i)
    {
        eventConnector->at_storageFailure(storage->getParentServer(), /*time*/ 0,
            nx::vms::api::EventReason::storageFull, storage);
    }
    storage->setStatus(Qn::Online);

    do
    {
        systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
        storageData = systemValues["storages"][storage->getId().toSimpleString()];
        stateData = storageData["state"];
    } while (kIssues != stateData["issues24h"].toInt());
    ASSERT_EQ("Online", stateData["status"].toString());
    ASSERT_EQ(kIssues, stateData["issues24h"].toInt());

    auto storageData2 = systemValues["storages"][storage2->getId().toSimpleString()];
    auto stateData2 = storageData2["state"];
    ASSERT_EQ(0, stateData2["issues24h"].toInt());
}

TEST_F(MetricsStoragesApi, activity)
{
    auto storages = launcher->commonModule()->resourcePool()->getResources<QnStorageResource>();
    ASSERT_FALSE(storages.isEmpty());
    auto storage = storages[0];
    storage.dynamicCast<QnFileStorageResource>()->setMounted(true);
    storage->initOrUpdate();

    auto resourcePool = launcher->commonModule()->resourcePool();
    auto server = resourcePool->getOwnMediaServer();
    server->setMetadataStorageId(storage->getId());

    // 'MediaServerProcess' doesn't call it in the test mode.
    launcher->mediaServerProcess()->initializeAnalyticsEvents();

    double readRate = 0;
    double writeRate = 0;
    SystemValues systemValues;
    do
    {
        auto file = std::unique_ptr<QIODevice>(storage->open("test1.mkv", QIODevice::WriteOnly));
        std::vector<char> data(1024 * 128);
        for (int i = 0; i < 8; ++i)
            file->write(&data[0], data.size());
        file = std::unique_ptr<QIODevice>(storage->open("test1.mkv", QIODevice::ReadOnly));
        file->read(&data[0], data.size());

        systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
        auto storageData = systemValues["storages"][storage->getId().toSimpleString()];
        auto activityData = storageData["activity"];
        readRate = activityData["readRateBps1m"].toDouble();
        writeRate = activityData["writeRateBps1m"].toDouble();
    } while (readRate == 0 || writeRate == 0);

    double transactions = 0;
    do
    {
        auto db = launcher->serverModule()->analyticsEventsStorage();
        std::promise<void> future;
        db->lookupTimePeriods(Filter(), TimePeriodsLookupOptions(),
            [&future](auto&&...) { future.set_value(); });
        future.get_future().wait();

        systemValues = launcher->get<SystemValues>("/ec2/metrics/values");
        auto storageData = systemValues["storages"][storage->getId().toSimpleString()];
        auto activityData = storageData["activity"];
        transactions = activityData["transactionsPerSecond"].toDouble();
    } while (transactions == 0);

    auto storage2 = storages[1];
    auto storageData2 = systemValues["storages"][storage2->getId().toSimpleString()];
    auto activityData2 = storageData2["activity"];
    ASSERT_EQ(0, activityData2.count("transactionsPerSecond"));

}

} // namespace nx::vms::server::test
