#include <gtest/gtest.h>

#include <api/global_settings.h>
#include <nx/vms/api/metrics.h>
#include <server_for_tests.h>
#include <core/resource/media_server_resource.h>

namespace nx::vms::server::test {

using namespace nx::vms::api::metrics;
using namespace nx::vms::server;

class MetricsServersApi: public testing::Test, public ServerForTests {};

TEST_F(MetricsServersApi, values)
{
    auto startValues = get<SystemValues>("/ec2/metrics/values")["servers"][id];
    EXPECT_EQ(startValues["_"]["name"], mediaServerProcess()->thisServer()->getName());

    EXPECT_EQ(startValues["availability"]["status"], "Online");
    EXPECT_EQ(startValues["availability"]["offlineEvents"], 1);
    EXPECT_GT(startValues["availability"]["uptimeS"].toDouble(), 0);

    // TODO: Mockup platform monitor on check info group.
    EXPECT_NE(startValues["info"]["publicIp"].toString(), "");
    EXPECT_NE(startValues["info"]["os"].toString(), "");
    EXPECT_NE(startValues["info"]["osTime"].toString(), "");
    EXPECT_NE(startValues["info"]["vmsTime"].toString(), "");
    EXPECT_EQ(startValues["info"]["vmsTimeChanged24h"], 0);
    EXPECT_NE(startValues["info"]["cpu"].toString(), "");
    // EXPECT_GT(startValues["info"]["cpuCores"].toDouble(), 0);
    // EXPECT_GT(startValues["info"]["ram"].toDouble(), 0);

    // EXPECT_GT(startValues["load"]["cpuUsageP"].toDouble(), 0);
    // EXPECT_GT(startValues["load"]["serverCpuUsageP"].toDouble(), 0);
    // EXPECT_GT(startValues["load"]["ramUsageB"].toDouble(), 0);
    // EXPECT_GT(startValues["load"]["ramUsageP"].toDouble(), 0);
    // EXPECT_GT(startValues["load"]["serverRamUsage"].toDouble(), 0);
    // EXPECT_GT(startValues["load"]["serverRamUsageP"].toDouble(), 0);
    EXPECT_EQ(startValues["load"]["cameras"], 0);
    EXPECT_EQ(startValues["load"]["decodingSpeed3s"], 0);
    EXPECT_EQ(startValues["load"]["encodingSpeed3s"], 0);
    EXPECT_EQ(startValues["load"]["encodingThreads"], 0);
    EXPECT_EQ(startValues["load"]["primaryStreams"], 0);
    EXPECT_EQ(startValues["load"]["secondaryStreams"], 0);
    EXPECT_GT(startValues["load"]["incomingConnections"].toDouble(), 0);
    EXPECT_NE(startValues["load"]["logLevel"].toString(), "");

    EXPECT_GT(startValues["activity"]["transactionsPerSecond1m"].toDouble(), 0);
    EXPECT_EQ(startValues["activity"]["actionsTriggered"], 0);
    EXPECT_EQ(startValues["activity"]["apiCalls1m"], 0);
    EXPECT_EQ(startValues["activity"]["thumbnails1m"], 0);
    // EXPECT_NE(startValues["activity"]["plugins"].toString(), "");

    auto startAlarms = get<Alarms>("/ec2/metrics/alarms");
    EXPECT_TRUE(startAlarms.empty());

    // TODO: Cause values modification and check for response changes.
}

TEST_F(MetricsServersApi, twoServers)
{
    auto rules = get<SystemRules>("/ec2/metrics/rules");
    EXPECT_EQ(rules["systems"].size(), 1);
    EXPECT_EQ(rules["servers"].size(), 5);
    EXPECT_EQ(rules["cameras"].size(), 5);
    EXPECT_EQ(rules["storages"].size(), 4);

    auto manifest = get<SystemManifest>("/ec2/metrics/manifest");
    EXPECT_EQ(manifest["systems"].size(), 1);
    EXPECT_EQ(manifest["servers"].size(), 5);
    EXPECT_EQ(manifest["cameras"].size(), 5);
    EXPECT_EQ(manifest["storages"].size(), 4);

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
        auto secondRules = secondServer->get<SystemRules>("/ec2/metrics/rules");
        EXPECT_EQ(QJson::serialized(rules), QJson::serialized(secondRules));

        auto secondManifest = secondServer->get<SystemManifest>("/ec2/metrics/manifest");
        EXPECT_EQ(QJson::serialized(manifest), QJson::serialized(secondManifest));

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

} // namespace nx::vms::server::test
