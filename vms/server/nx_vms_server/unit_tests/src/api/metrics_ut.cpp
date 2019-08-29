#include <gtest/gtest.h>

#include <nx/vms/api/metrics.h>
#include <rest/server/json_rest_result.h>

#include "test_api_requests.h"

namespace nx::test {

using namespace nx::vms::api::metrics;

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

TEST_F(MetricsApi, Api)
{
    const auto manifest = get<SystemManifest>("/api/metrics/manifest");
    expectEq(manifest, get<SystemManifest>("/ec2/metrics/manifest"));
    expectCounts(
        "manifest", manifest,
        "systems", 1, "servers", 1, "cameras", 1, "storages", 2, "networkInterfaces", 2);

    const auto values = get<SystemValues>("/api/metrics/values");
    expectEq(values, get<SystemValues>("/ec2/metrics/values"));
    expectCounts(
        "values", values,
        "systems", 1, "servers", 1, "cameras", 0, "storages", 0);
}

} // nx::test
