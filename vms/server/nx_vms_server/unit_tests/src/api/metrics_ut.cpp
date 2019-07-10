#include <gtest/gtest.h>

#include <nx/vms/api/metrics.h>
#include <nx/network/rest/result.h>

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
        nx::network::rest::JsonResult result;
        [&]() { NX_TEST_API_GET(&launcher, api, &result); }();
        EXPECT_EQ(result.error, nx::network::rest::Result::NoError);
        
        bool isOk = false;
        auto value = result.deserialized<T>(&isOk);
        EXPECT_TRUE(isOk);
        return value;
    }

    template<typename Values, typename... Args>
    void expectCounts(const Values& values, const QString& type, size_t count, Args... args)
    {
        const auto it = values.find(type);
        const auto actualCount = (it != values.end()) ? it->second.size() : size_t(0);
        EXPECT_EQ(actualCount, count) << type.toStdString();
        if constexpr (sizeof...(Args) > 0)
            expectCounts(values, std::forward<Args>(args)...);
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
    const auto rules = get<SystemRules>("/api/metrics/rules");
    expectCounts(rules, "systems", 3, "servers", 2, "cameras", 5, "storages", 2);
    expectEq(rules, get<SystemRules>("/ec2/metrics/rules"));

    const auto manifest = get<SystemManifest>("/api/metrics/manifest");
    expectCounts(manifest, "systems", 7, "servers", 14, "cameras", 12, "storages", 6, "networks", 3);
    expectEq(manifest, get<SystemManifest>("/ec2/metrics/manifest"));

    const auto rawManifest = get<SystemManifest>("/api/metrics/manifest?noRules");
    expectCounts(rawManifest, "systems", 7, "servers", 14, "cameras", 11, "storages", 6, "networks", 3);
    expectEq(rawManifest, get<SystemManifest>("/ec2/metrics/manifest?noRules"));

    // TODO: Generate more resources for better values check.
    // TODO: Merge with more servers to test aggreagation.

    const auto values = get<SystemValues>("/api/metrics/values");
    expectCounts(values, "systems", 1, "servers", 1);
    expectEq(values, get<SystemValues>("/ec2/metrics/values"));

    const auto rawValues = get<SystemValues>("/api/metrics/values?noRules");
    expectCounts(rawValues, "systems", 1, "servers", 1);
    expectEq(rawValues, get<SystemValues>("/ec2/metrics/values?noRules"));

    const auto timelineValues = get<SystemValues>("/api/metrics/values?timeline");
    expectCounts(timelineValues, "systems", 0, "servers", 1);
    expectEq(timelineValues, get<SystemValues>("/ec2/metrics/values?timeline"));
}

} // nx::test