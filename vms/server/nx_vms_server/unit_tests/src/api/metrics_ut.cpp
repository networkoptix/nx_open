#include <gtest/gtest.h>

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <nx/mediaserver/camera_mock.h>
#include <nx/vms/api/metrics.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"

namespace nx::test {

using namespace nx::vms::api::metrics;
using namespace nx::vms::server;

static const QString kCameraName("Camera1");
static const QString kCameraHostAddress("192.168.0.2");
static const QString kCameraFirmware("1.2.3.4");
static const QString kCameraModel("model1");
static const QString kCameraVendor("vendor1");

class DataProviderStub: public QnAbstractStreamDataProvider
{
public:
    using QnAbstractStreamDataProvider::QnAbstractStreamDataProvider;
};

class MetricsApi: public ::testing::Test
{
public:
    MetricsApi()
    {
        mainServer = std::make_unique<MediaServerLauncher>();
        EXPECT_TRUE(mainServer->start());
    }

    std::unique_ptr<MediaServerLauncher> addServer() const
    {
        auto newServer = std::make_unique<MediaServerLauncher>();
        EXPECT_TRUE(newServer->start());
        newServer->connectAndWaitForSync(mainServer.get());
        return newServer;
    }

    template<typename T>
    T get(const QString& api, const std::unique_ptr<MediaServerLauncher>& targetServer = {})
    {
        QnJsonRestResult result;
        [&](){ NX_TEST_API_GET(targetServer ? targetServer.get() : mainServer.get(), api, &result); }();
        EXPECT_EQ(result.error, QnJsonRestResult::NoError);
        return result.deserialized<T>();
    }

protected:
    std::unique_ptr<MediaServerLauncher> mainServer;
};

TEST_F(MetricsApi, Api)
{
    auto rules = get<SystemRules>("/ec2/metrics/rules");
    EXPECT_EQ(rules["systems"].size(), 1);
    EXPECT_EQ(rules["servers"].size(), 3);
    EXPECT_EQ(rules["cameras"].size(), 5);
    EXPECT_EQ(rules["storages"].size(), 4);

    auto manifest = get<SystemManifest>("/ec2/metrics/manifest");
    EXPECT_EQ(manifest["systems"].size(), 1);
    EXPECT_EQ(manifest["servers"].size(), 3);
    EXPECT_EQ(manifest["cameras"].size(), 5);
    EXPECT_EQ(manifest["storages"].size(), 4);

    const auto systemId = mainServer->commonModule()->globalSettings()->localSystemId().toSimpleString();
    const auto mainServerId = mainServer->commonModule()->moduleGUID().toSimpleString();
    {
        auto serverValues = get<SystemValues>("/api/metrics/values");
        EXPECT_EQ(serverValues["systems"].size(), 0);
        EXPECT_EQ(serverValues["servers"].size(), 1);
        EXPECT_FALSE(serverValues["servers"][mainServerId].count("info"));
        EXPECT_GT(serverValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(serverValues["servers"][mainServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(serverValues["cameras"].size(), 0);
        EXPECT_EQ(serverValues["storages"].size(), 0);

        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        EXPECT_EQ(systemValues["systems"].size(), 1);
        EXPECT_EQ(systemValues["systems"][systemId]["info"]["servers"], 1);
        EXPECT_EQ(systemValues["servers"].size(), 1);
        EXPECT_EQ(systemValues["servers"][mainServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][mainServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(systemValues["cameras"].size(), 0);
        EXPECT_EQ(systemValues["storages"].size(), 0);
    }

    auto secondServer = addServer();
    const auto secondServerId = secondServer->commonModule()->moduleGUID().toSimpleString();
    {
        auto secondRules = get<SystemRules>("/ec2/metrics/rules", secondServer);
        EXPECT_EQ(QJson::serialized(rules), QJson::serialized(secondRules));

        auto secondManifest = get<SystemManifest>("/ec2/metrics/manifest", secondServer);
        EXPECT_EQ(QJson::serialized(manifest), QJson::serialized(secondManifest));

        auto mainServerValues = get<SystemValues>("/api/metrics/values", mainServer);
        EXPECT_EQ(mainServerValues["systems"].size(), 0);
        EXPECT_EQ(mainServerValues["servers"].size(), 1);
        EXPECT_GT(mainServerValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(mainServerValues["servers"][mainServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);

        auto secondServerValues = get<SystemValues>("/api/metrics/values", secondServer);
        EXPECT_EQ(secondServerValues["systems"].size(), 0);
        EXPECT_EQ(secondServerValues["servers"].size(), 1);
        EXPECT_GT(secondServerValues["servers"][secondServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(secondServerValues["servers"][secondServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);

        auto systemValues = get<SystemValues>("/ec2/metrics/values", secondServer);
        EXPECT_EQ(systemValues["systems"].size(), 1);
        EXPECT_EQ(systemValues["systems"][systemId]["info"]["servers"], 2);
        EXPECT_EQ(systemValues["servers"].size(), 2);
        EXPECT_EQ(systemValues["servers"][mainServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][mainServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(systemValues["servers"][secondServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][secondServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][secondServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);
    }

    secondServer.reset();
    {
        auto serverValues = get<SystemValues>("/api/metrics/values");
        EXPECT_EQ(serverValues["systems"].size(), 0);
        EXPECT_EQ(serverValues["servers"].size(), 1);
        EXPECT_FALSE(serverValues["servers"][mainServerId].count("info"));
        EXPECT_GT(serverValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(serverValues["servers"][mainServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);

        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        EXPECT_EQ(systemValues["systems"].size(), 1);
        EXPECT_EQ(systemValues["systems"][systemId]["info"]["servers"], 2);
        EXPECT_EQ(systemValues["servers"].size(), 2);
        EXPECT_EQ(systemValues["servers"][mainServerId]["availability"]["status"], "Online");
        EXPECT_GT(systemValues["servers"][mainServerId]["availability"]["uptimeS"].toDouble(), 0);
        EXPECT_GT(systemValues["servers"][mainServerId]["serverLoad"]["transactionsPerSecond1m"].toDouble(), 0);
        EXPECT_EQ(systemValues["servers"][secondServerId]["availability"]["status"], "Offline");
        EXPECT_FALSE(systemValues["servers"][secondServerId]["availability"].count("uptimeS"));
        EXPECT_FALSE(systemValues["servers"][secondServerId]["serverLoad"].count("transactionsPerSecond1m"));
    }
}

} // nx::test
