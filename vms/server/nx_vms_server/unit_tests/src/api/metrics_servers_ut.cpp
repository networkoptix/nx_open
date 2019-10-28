#include <gtest/gtest.h>

#include <api/global_settings.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/api/metrics.h>
#include <platform/hardware_information.h>
#include <server_for_tests.h>
#include <nx/metrics/streams_metric_helper.h>
#include "test_api_requests.h"
#include <nx/vms/server/metrics/helpers.h>
#include <test_support/storage_utils.h>
#include <recorder/storage_manager.h>

namespace nx::vms::server::test {

using namespace nx::vms::api::metrics;
using namespace nx::vms::server;

constexpr qreal operator""_GB(unsigned long long int v) { return v * 1024 * 1024 * 1024; }
const static int kTestDataWidth = 640;
const static int kTestDataHeight = 480;

class MetricsServersApi: public testing::Test, public ServerForTests
{
public:
    static void SetUpTestCase()
    {
        auto& hardware = const_cast<HardwareInformation&>(HardwareInformation::instance());
        hardware.physicalMemory = 8_GB;
        hardware.cpuModelName = "NX Core 10 Trio";
        hardware.logicalCores = 8;
        hardware.physicalCores = 4;
    }

    MetricsServersApi()
    {
        serverModule()->platform()->setCustomMonitor(std::make_unique<StubMonitor>());
    }
};

#define EXPECT_DOUBLE(VALUE, EXPECTED) EXPECT_NEAR(VALUE.toDouble(), EXPECTED, double(EXPECTED) / 1000)

TEST_F(MetricsServersApi, oneServer)
{
    auto systemMonitor = static_cast<StubMonitor*>(serverModule()->platform()->monitor());

    systemMonitor->totalCpuUsage_ = 0.2;
    systemMonitor->totalRamUsageBytes_ = 2_GB;
    systemMonitor->thisProcessCpuUsage_ = 0.1;
    systemMonitor->thisProcessRamUsageBytes_ = 1_GB;

    auto startValues = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    EXPECT_EQ(startValues["_"]["name"], mediaServerProcess()->thisServer()->getName());
    EXPECT_EQ(startValues["availability"]["status"], "Online");
    EXPECT_EQ(startValues["availability"]["offlineEvents"], 1);
    EXPECT_EQ(startValues["availability"]["uptimeS"], 1);

    // TODO: Mockup & check.
    EXPECT_EQ(startValues["info"]["publicIp"].toString(), "");
    EXPECT_NE(startValues["info"]["os"].toString(), "");
    EXPECT_NE(startValues["info"]["osTime"].toString(), "");
    EXPECT_NE(startValues["info"]["vmsTime"].toString(), "");
    EXPECT_EQ(startValues["info"]["vmsTimeChanged24h"], 0);

    EXPECT_EQ(startValues["info"]["cpu"], "NX Core 10 Trio");
    EXPECT_EQ(startValues["info"]["cpuCores"], 4);
    EXPECT_DOUBLE(startValues["info"]["ram"], 8_GB);
    EXPECT_DOUBLE(startValues["load"]["cpuUsageP"], 0.2);
    EXPECT_DOUBLE(startValues["load"]["serverCpuUsageP"], 0.1);
    EXPECT_DOUBLE(startValues["load"]["ramUsageB"], 2_GB);
    EXPECT_DOUBLE(startValues["load"]["ramUsageP"], 2.0 / 8);
    EXPECT_DOUBLE(startValues["load"]["serverRamUsage"], 1_GB);
    EXPECT_DOUBLE(startValues["load"]["serverRamUsageP"], 1.0 / 8);
    EXPECT_EQ(startValues["load"]["cameras"], 0);

    // TODO: Mockup & check.
    EXPECT_EQ(startValues["load"]["decodingSpeed3s"], 0);
    EXPECT_EQ(startValues["load"]["encodingSpeed3s"], 0);
    EXPECT_EQ(startValues["load"]["encodingThreads"], 0);
    EXPECT_GT(startValues["load"]["incomingConnections"].toDouble(), 0);
    EXPECT_NE(startValues["load"]["logLevel"].toString(), "");
    EXPECT_GT(startValues["activity"]["transactionsPerSecond1m"].toDouble(), 0);
    EXPECT_EQ(startValues["activity"]["actionsTriggered"], 0);
    EXPECT_EQ(startValues["activity"]["apiCalls1m"], 0);
    EXPECT_EQ(startValues["activity"]["thumbnails1m"], 0);
    EXPECT_EQ(startValues["activity"]["plugins"], api::metrics::Value());

    auto startAlarms = getFlat<SystemAlarms>("/ec2/metrics/alarms");
    startAlarms = nx::utils::filter_if(
        startAlarms, [](auto k, auto /*v*/) { return !k.contains(".load.logLevel."); });
    EXPECT_EQ(startAlarms.size(), 0) << QJson::serialized(startAlarms).data();

    systemMonitor->totalCpuUsage_ = 0.95;
    systemMonitor->totalRamUsageBytes_ = 7_GB;
    systemMonitor->thisProcessCpuUsage_ = 0.85;
    systemMonitor->thisProcessRamUsageBytes_ = 6_GB;
    systemMonitor->processUptime_ = std::chrono::minutes{1};

    auto runValues = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    EXPECT_EQ(runValues["_"]["name"], mediaServerProcess()->thisServer()->getName());
    EXPECT_EQ(runValues["availability"]["uptimeS"], 60);
    EXPECT_DOUBLE(runValues["load"]["cpuUsageP"], 0.95);
    EXPECT_DOUBLE(runValues["load"]["serverCpuUsageP"], 0.85);
    EXPECT_DOUBLE(runValues["load"]["ramUsageB"], 7_GB);
    EXPECT_DOUBLE(runValues["load"]["ramUsageP"], 7.0 / 8);
    EXPECT_DOUBLE(runValues["load"]["serverRamUsage"], 6_GB);
    EXPECT_DOUBLE(runValues["load"]["serverRamUsageP"], 6.0 / 8);

    auto runAlarms = getFlat<SystemAlarms>("/ec2/metrics/alarms");
    runAlarms = nx::utils::filter_if(
        runAlarms, [](auto k, auto /*v*/) { return !k.contains(".load.logLevel."); });
    ASSERT_EQ(runAlarms.size(), 2) << QJson::serialized(runAlarms).data();
    EXPECT_EQ(runAlarms["servers." + id + ".load.cpuUsageP.0"].level, api::metrics::AlarmLevel::warning);
    EXPECT_EQ(runAlarms["servers." + id + ".load.ramUsageP.0"].level, api::metrics::AlarmLevel::warning);

    for (int i = 0; i < 5; ++i)
        addCamera(i);

    while (get<SystemValues>("/ec2/metrics/values")["servers"][id]["load"]["cameras"].toDouble() != 5)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(MetricsServersApi, twoServers)
{
    const auto systemId = commonModule()->globalSettings()->localSystemId().toSimpleString();
    const auto mainServerId = commonModule()->moduleGUID().toSimpleString();
    {
        auto serverValues = get<SystemValues>("/api/metrics/values");
        EXPECT_EQ(serverValues["systems"].size(), 0);
        EXPECT_EQ(serverValues["servers"].size(), 1);
        EXPECT_FALSE(serverValues["servers"][mainServerId].count("info"));
        EXPECT_GT(serverValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(serverValues["servers"][mainServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(serverValues["cameras"].size(), 0);
        EXPECT_EQ(serverValues["storages"].size(), 0);

        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        EXPECT_EQ(systemValues["systems"].size(), 1);
        EXPECT_EQ(systemValues["systems"][systemId]["info"]["servers"], 1);
        EXPECT_EQ(systemValues["servers"].size(), 1);
        EXPECT_EQ(systemValues["servers"][mainServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][mainServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(systemValues["cameras"].size(), 0);
        EXPECT_EQ(systemValues["storages"].size(), 0);
    }

    auto secondServer = addServer();
    const auto secondServerId = secondServer->commonModule()->moduleGUID().toSimpleString();
    {
        auto mainServerValues = get<SystemValues>("/api/metrics/values");
        EXPECT_EQ(mainServerValues["systems"].size(), 0);
        EXPECT_EQ(mainServerValues["servers"].size(), 1);
        EXPECT_GT(mainServerValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(mainServerValues["servers"][mainServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);

        auto secondServerValues = secondServer->get<SystemValues>("/api/metrics/values");
        EXPECT_EQ(secondServerValues["systems"].size(), 0);
        EXPECT_EQ(secondServerValues["servers"].size(), 1);
        EXPECT_GT(secondServerValues["servers"][secondServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(secondServerValues["servers"][secondServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);

        while (secondServer->get<SystemValues>("/ec2/metrics/values")["servers"].size() != 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); //< Wait for sync.

        auto systemValues = secondServer->get<SystemValues>("/ec2/metrics/values");
        EXPECT_EQ(systemValues["systems"].size(), 1);
        EXPECT_EQ(systemValues["systems"][systemId]["info"]["servers"], 2);
        EXPECT_EQ(systemValues["servers"].size(), 2);
        EXPECT_EQ(systemValues["servers"][mainServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][mainServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(systemValues["servers"][secondServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][secondServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][secondServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);
    }

    secondServer.reset();
    {
        auto serverValues = get<SystemValues>("/api/metrics/values");
        EXPECT_EQ(serverValues["systems"].size(), 0);
        EXPECT_EQ(serverValues["servers"].size(), 1);
        EXPECT_FALSE(serverValues["servers"][mainServerId].count("info"));
        EXPECT_GT(serverValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(serverValues["servers"][mainServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);

        while (get<SystemValues>("/ec2/metrics/values")["servers"].size() != 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); //< Wait for sync.

        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        EXPECT_EQ(systemValues["systems"].size(), 1);
        EXPECT_EQ(systemValues["systems"][systemId]["info"]["servers"], 2);
        EXPECT_EQ(systemValues["servers"].size(), 2);
        EXPECT_EQ(systemValues["servers"][mainServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][mainServerId]["activity"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(systemValues["servers"][secondServerId]["availability"]["status"], "Offline");
        EXPECT_FALSE(systemValues["servers"][secondServerId]["availability"].count("uptimeS"));
        EXPECT_FALSE(systemValues["servers"][secondServerId]["activity"].count("transactionsPerSecond1m"));
    }
}

TEST_F(MetricsServersApi, streamCount)
{
    auto checkCounters =
        [this](int primary, int secondary)
        {
            auto valuess = get<SystemValues>("/ec2/metrics/values")["servers"][id];
            EXPECT_EQ(valuess["load"]["primaryStreams"], primary);
            EXPECT_EQ(valuess["load"]["secondaryStreams"], secondary);
        };

    {
        nx::vms::metrics::StreamMetricHelper helper(commonModule()->metrics());
        checkCounters(/*primary*/ 0, /*secondary*/ 0);

        helper.setStream(api::StreamIndex::primary);
        checkCounters(/*primary*/ 1, /*secondary*/ 0);

        helper.setStream(api::StreamIndex::secondary);
        checkCounters(/*primary*/ 0, /*secondary*/ 1);
    }
    checkCounters(/*primary*/ 0, /*secondary*/ 0);
}

TEST_F(MetricsServersApi, apiCalls)
{
    nx::vms::server::metrics::setTimerMultiplier(100);

    auto values = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    int apiCalls1 = values["activity"]["apiCalls1m"].toDouble();
    int apiCalls2 = 0;
    do
    {
        values = get<SystemValues>("/ec2/metrics/values")["servers"][id];
        apiCalls2 = values["activity"]["apiCalls1m"].toDouble();
    } while (apiCalls2 == apiCalls1);
    ASSERT_GT(apiCalls2, apiCalls1);
    nx::vms::server::metrics::setTimerMultiplier(1);
}

TEST_F(MetricsServersApi, thumbnailAndDecodingSpeed)
{
    nx::vms::server::metrics::setTimerMultiplier(100);

    auto camera = addCamera(0);
    auto storage = addStorage("Storage 1");

    auto catalog = serverModule()->normalStorageManager()->getFileCatalog(
        camera->getUniqueId(), QnServer::HiQualityCatalog);

    static const int kDurationMs = 5000;
    auto data = test_support::createTestMkvFileData(kDurationMs/1000, kTestDataWidth, kTestDataHeight);
    const auto chunks = test_support::generateChunksForQuality(
        storage->getUrl(),
        camera->getPhysicalId(),
        "hi_quality",
        /*time*/ 0, kDurationMs,
        /*count*/ 1, data);
    for (const auto& testChunk: chunks)
    {
        Chunk chunk;
        chunk.startTimeMs = testChunk.startTimeMs;
        chunk.durationMs = testChunk.durationsMs;
        chunk.fileIndex = nx::vms::server::Chunk::FILE_INDEX_WITH_DURATION;
        chunk.setFileSize(data.size());
        chunk.storageIndex = serverModule()->storageDbPool()->getStorageIndex(storage);
        catalog->addRecord(chunk);
    }

    const int kTestDataPixels  = kTestDataWidth * kTestDataHeight;

    double expectedThumbnailsRate = 0;
    double expectedPixelsRate = 0;
    double thumbnailsRate = 0;
    double pixelsRate = 0;

    static const int kIterations = 10;
    for (int i = 0; i < kIterations; ++i)
        get<QByteArray>(lm("/ec2/cameraThumbnails?time=0&cameraId=%1").arg(camera->getPhysicalId()));

    bool done = false;
    do
    {
        auto values = get<SystemValues>("/ec2/metrics/values")["servers"][id];
        thumbnailsRate = std::max(thumbnailsRate, values["activity"]["thumbnails1m"].toDouble());
        pixelsRate = std::max(pixelsRate, values["load"]["decodingSpeed3s"].toDouble());
        const double seconds = serverStartTimer.elapsedMs() / 1000.0;
        expectedThumbnailsRate = kIterations / seconds;
        expectedPixelsRate = kIterations / seconds * kTestDataPixels;
        done = thumbnailsRate >= expectedThumbnailsRate && pixelsRate >= expectedPixelsRate;
    } while (!done);

    GTEST_ASSERT_GE(thumbnailsRate, expectedThumbnailsRate);
    GTEST_ASSERT_GE(pixelsRate, expectedPixelsRate);


    nx::vms::server::metrics::setTimerMultiplier(1);
}

TEST_F(MetricsServersApi, outgoingConnections)
{
    auto values = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    int connections1 = values["load"]["outgoingConnections"].toInt();

    std::vector<std::unique_ptr<nx::network::AbstractStreamSocket>> tmpData;
    static const int kIterations = 10;
    for (int i = 0; i < kIterations; ++i)
        tmpData.emplace_back(nx::network::SocketFactory::createStreamSocket());

    values = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    int connections2 = values["load"]["outgoingConnections"].toInt();

    tmpData.clear();
    values = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    int connections3 = values["load"]["outgoingConnections"].toInt();

    int incomingConnections = values["load"]["incomingConnections"].toInt();

    EXPECT_EQ(connections2, connections1 + kIterations);
    EXPECT_EQ(connections1, connections3);
}

} // namespace nx::vms::server::test
