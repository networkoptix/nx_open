#include <gtest/gtest.h>

#include <nx/vms/api/metrics.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"
#include <nx/mediaserver/camera_mock.h>
#include <core/resource_management/resource_pool.h>

namespace nx::test {

using namespace nx::vms::api::metrics;
using namespace nx::vms::server;

class DataProviderStub: public QnAbstractStreamDataProvider
{
public:
    using QnAbstractStreamDataProvider::QnAbstractStreamDataProvider;
};

class MetricsApi: public ::testing::Test
{
public:
    std::unique_ptr<DataProviderStub> m_dataProviderStub;
    QnSharedResourcePointer<resource::test::CameraMock> m_camera;

    MetricsApi()
    {
        EXPECT_TRUE(launcher.start());

        m_camera.reset(
            new resource::test::CameraMock(launcher.serverModule()));
        m_camera->setPhysicalId("testCamera 1");
        m_camera->setParentId(launcher.commonModule()->moduleGUID());
        launcher.commonModule()->resourcePool()->addResource(m_camera);
        m_dataProviderStub.reset(new DataProviderStub(m_camera));

        DeviceFileCatalog catalog(
            launcher.serverModule(),
            m_camera->getUniqueId(), QnServer::HiQualityCatalog, QnServer::StoragePool::Normal);

        nx::vms::server::Chunk chunk1;
        chunk1.startTimeMs = 1400000000LL * 1000;
        chunk1.durationMs = 45 * 1000;
        chunk1.setFileSize(100);
        catalog.addRecord(chunk1);

        nx::vms::server::Chunk chunk2;
        chunk2.startTimeMs = 1400000000LL * 1000;
        chunk2.durationMs = 45 * 1000;
        chunk2.setFileSize(200);
        catalog.addRecord(chunk2);
    }

    template<typename T>
    T get(const QString& api)
    {
        QnJsonRestResult result;
        [&]() { NX_TEST_API_GET(&launcher, api, &result); }();
        EXPECT_EQ(result.error, QnJsonRestResult::NoError);
        return result.deserialized<T>();
    }

    template<typename Values, typename... Args>
    void expectCounts(
        const QString& label, const Values& values,
        const QString& type, size_t expectedCount, Args... args)
    {
        const auto it = values.find(type);
        const auto actualCount = (it != values.end()) ? it->second.size() : size_t(0);
        EXPECT_EQ(actualCount, expectedCount) << (label + "." + type).toStdString();
        if constexpr (sizeof...(Args) > 0)
            expectCounts(label, values, std::forward<Args>(args)...);
    }

    template<typename T>
    void expectEq(const T& expected, const T& actual)
    {
        EXPECT_EQ(QJson::serialized(expected), QJson::serialized(actual));
    }

protected:
    MediaServerLauncher launcher;
};

// TODO: Check for actual data returned.
TEST_F(MetricsApi, Api)
{
    const auto rules = get<SystemRules>("/ec2/metrics/rules");
    expectCounts("rules", rules, "systems", 1, "servers", 3, "cameras", 5, "storages", 4);

    const auto manifest = get<SystemManifest>("/ec2/metrics/manifest");
    expectCounts("manifest", manifest, "systems", 1, "servers", 3, "cameras", 5, "storages", 4);

    const auto localValues = get<SystemValues>("/api/metrics/values");
    expectCounts("localValues", localValues,"systems", 0, "servers", 1, "cameras", 1, "storages", 0);

    const auto systemValues = get<SystemValues>("/ec2/metrics/values");
    expectCounts("systemValues", systemValues,"systems", 1, "servers", 1, "cameras", 1, "storages", 0);

    const auto localAlarms = get<Alarms>("/api/metrics/alarms");
    EXPECT_EQ(localAlarms.size(), 0);

    const auto systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    EXPECT_EQ(systemAlarms.size(), 0);
}

TEST_F(MetricsApi, offlineEvents)
{
    {
        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
        ASSERT_EQ(5, cameraData.size());
        int offlineEvents = cameraData["availability"]["offlineEvents"].toInt();
        ASSERT_EQ(1, offlineEvents);
    }

    const int kOfflineIterations = 4;
    for (int i = 0; i < kOfflineIterations; ++i)
    {
        m_camera->setStatus(Qn::Online);
        m_camera->setStatus(Qn::Offline);
    }

    {
        auto systemValues = get<SystemValues>("/ec2/metrics/values");
        auto cameraData = systemValues["cameras"][m_camera->getId().toSimpleString()];
        int offlineEvents = cameraData["availability"]["offlineEvents"].toInt();
        ASSERT_EQ(1 + kOfflineIterations, offlineEvents);
    }

    auto systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_EQ(2, systemAlarms.size());
    ASSERT_EQ("availability.offlineEvents", systemAlarms[0].parameter);
    ASSERT_EQ("availability.status", systemAlarms[1].parameter);

    m_camera->setStatus(Qn::Online);
    systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    ASSERT_EQ(1, systemAlarms.size());
}

} // nx::test
