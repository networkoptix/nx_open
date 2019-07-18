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
    expectCounts(values, "systems", 0, "servers", 1);

    const auto systemValues = get<SystemValues>("/ec2/metrics/values");
    expectCounts(systemValues, "systems", 1, "servers", 1);
    expectEq(values.find("servers")->second, systemValues.find("servers")->second);

    const auto rawValues = get<SystemValues>("/api/metrics/values?noRules");
    expectCounts(rawValues, "systems", 0, "servers", 1);

    const auto rawSystemValues = get<SystemValues>("/ec2/metrics/values?noRules");
    expectCounts(rawSystemValues, "systems", 1, "servers", 1);
    expectEq(rawValues.find("servers")->second, rawSystemValues.find("servers")->second);

    const auto timelineValues = get<SystemValues>("/api/metrics/values?timeline");
    expectCounts(timelineValues, "systems", 0, "servers", 1);

    const auto timelineSystemValues = get<SystemValues>("/ec2/metrics/values?timeline");
    expectCounts(timelineSystemValues, "systems", 0, "servers", 1);
    expectEq(timelineValues.find("servers")->second, timelineSystemValues.find("servers")->second);
}

} // nx::test
