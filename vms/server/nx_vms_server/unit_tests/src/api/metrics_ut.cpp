#include <gtest/gtest.h>

#include <nx/vms/api/metrics.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"
#include <nx/mediaserver/camera_mock.h>
#include <core/resource_management/resource_pool.h>

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
        EXPECT_TRUE(launcher.start());
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
    expectCounts("localValues", localValues, "systems", 0, "servers", 1, "cameras", 0, "storages", 0);

    const auto systemValues = get<SystemValues>("/ec2/metrics/values");
    expectCounts("systemValues", systemValues, "systems", 1, "servers", 1, "cameras", 0, "storages", 0);

    const auto localAlarms = get<Alarms>("/api/metrics/alarms");
    EXPECT_EQ(localAlarms.size(), 0);

    const auto systemAlarms = get<Alarms>("/ec2/metrics/alarms");
    EXPECT_EQ(systemAlarms.size(), 0);
}

} // nx::test
