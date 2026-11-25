// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <map>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <QtTest/QTest>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/cloud_features/abstract_cloud_feature_source.h>
#include <nx/vms/client/core/cloud_features/cloud_feature.h>
#include <nx/vms/client/core/cloud_features/cloud_service_checker.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/system_finder/mock_cloud_status_watcher.h>

#include "test_cloud_features.h"

namespace nx::vms::client::core::test {

namespace {

constexpr int kResponseTimeoutMs = 10000;

/** What the cloud source enables until the cloud reports otherwise. */
constexpr CloudFeatures kCloudDefaults = kLayouts | kPush | kCrossSite;

bool waitForChange(AbstractCloudFeatureSource* source, int timeoutMs)
{
    bool changed = false;
    const auto connection = QObject::connect(source,
        &AbstractCloudFeatureSource::featuresChanged,
        source,
        [&changed]() { changed = true; });
    const bool result = QTest::qWaitFor([&changed]() { return changed; }, timeoutMs);
    QObject::disconnect(connection);
    return result;
}

} // namespace

class CloudServiceCheckerTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        server.registerContentProvider("/api/utils/settings",
            [this]() -> std::unique_ptr<nx::network::http::AbstractMsgBodySource>
            {
                NX_MUTEX_LOCKER lock(&mutex);
                return std::make_unique<nx::network::http::BufferSource>(
                    "application/json", nx::Buffer(response));
            });
        ASSERT_TRUE(server.bindAndListen());
        nx::network::url::Builder builder;
        builder.setScheme("http").setEndpoint(server.serverAddress());
        portalUrl = builder.toUrl();
        clearCache();
    }

    virtual void TearDown() override { clearCache(); }

    void clearCache()
    {
        appContext.coreSettings()->lastKnownCloudEnabledFeatures = Settings::CloudFeatures();
    }

    void givenCloudResponse(const std::string& body)
    {
        NX_MUTEX_LOCKER lock(&mutex);
        response = body;
    }

    std::unique_ptr<CloudServiceChecker> makeSource()
    {
        return std::make_unique<CloudServiceChecker>(
            &watcher, appContext.coreSettings(), portalUrl);
    }

    ApplicationContext appContext{
        ApplicationContext::Mode::unitTests, Qn::SerializationFormat::json};
    MockCloudStatusWatcher watcher; //< Has no credentials, the user name is empty.
    nx::Mutex mutex;
    std::string response;
    nx::network::http::TestHttpServer server; //< Reads the response, so declared after it.
    nx::Url portalUrl;
};

TEST_F(CloudServiceCheckerTest, reportsReceivedFlagsAndCachesThem)
{
    // Only the exact enumerator name with a boolean value is a flag of a feature: not another
    // letter case, not the value of the enumerator.
    givenCloudResponse(R"json({"featureFlags": {
        "vmsClientCrashReporting": true,
        "vmsClientCloudLayouts": false,
        "VmsClientQrCodeDeployment": true,
        "16": true,
        "layouts": true,
        "vmsClientPushNotifications": "no"}})json");

    {
        const auto source = makeSource();
        EXPECT_EQ(source->enabledFeatures(), kCloudDefaults);
        ASSERT_TRUE(waitForChange(source.get(), kResponseTimeoutMs));

        // The reported values, the defaults for the rest.
        EXPECT_EQ(source->enabledFeatures(), kCrashReporting | kPush | kCrossSite);
    }

    // Only the features the cloud reported enabled are cached, by the user name.
    const Settings::CloudFeatures cached{{"", "vmsClientCrashReporting"}};
    EXPECT_EQ(appContext.coreSettings()->lastKnownCloudEnabledFeatures(), cached);

    // The cache answers before the network does; the disabled feature is back to its default.
    const auto source = makeSource();
    EXPECT_EQ(source->enabledFeatures(), kCrashReporting | kCloudDefaults);
}

TEST_F(CloudServiceCheckerTest, missingServiceDisablesItsFeatures)
{
    givenCloudResponse(R"json({"featureFlags": {
        "docdb": false,
        "push_notifications": false,
        "vmsClientPushNotifications": true,
        "cloud_notifications": false,
        "vmsClientCrashReporting": true}})json");

    {
        const auto source = makeSource();
        ASSERT_TRUE(waitForChange(source.get(), kResponseTimeoutMs));

        // A missing service wins over the default and over the flag of its feature.
        EXPECT_EQ(source->enabledFeatures(), kCrashReporting);
    }

    // Neither the missing services nor the flag overridden by one of them are cached.
    const Settings::CloudFeatures cached{{"", "vmsClientCrashReporting"}};
    EXPECT_EQ(appContext.coreSettings()->lastKnownCloudEnabledFeatures(), cached);

    const auto source = makeSource();
    EXPECT_EQ(source->enabledFeatures(), kCrashReporting | kCloudDefaults);
}

TEST_F(CloudServiceCheckerTest, presentServiceLeavesItsFeaturesToDefaults)
{
    // The feature flag makes the answer observable.
    givenCloudResponse(R"json({"featureFlags": {
        "docdb": true,
        "push_notifications": "no",
        "vmsClientCrashReporting": true}})json");

    const auto source = makeSource();
    ASSERT_TRUE(waitForChange(source.get(), kResponseTimeoutMs));

    // A service flag that is not a boolean does not tell that the service is missing.
    EXPECT_EQ(source->enabledFeatures(), kCrashReporting | kCloudDefaults);
    // A present service does not report its features as enabled.
    const Settings::CloudFeatures cached{{"", "vmsClientCrashReporting"}};
    EXPECT_EQ(appContext.coreSettings()->lastKnownCloudEnabledFeatures(), cached);
}

TEST_F(CloudServiceCheckerTest, eachServiceDisablesItsOwnFeature)
{
    const std::map<std::string, CloudFeature> featureOfService{
        {"docdb", CloudFeature::vmsClientCloudLayouts},
        {"push_notifications", CloudFeature::vmsClientPushNotifications},
        {"cloud_notifications", CloudFeature::vmsClientCrossSiteNotifications}};

    for (const auto& [service, feature]: featureOfService)
    {
        SCOPED_TRACE(service);
        givenCloudResponse(R"json({"featureFlags": {")json" + service + R"json(": false}})json");
        const auto source = makeSource();
        ASSERT_TRUE(waitForChange(source.get(), kResponseTimeoutMs));
        EXPECT_EQ(source->enabledFeatures(), kCloudDefaults & ~CloudFeatures(feature));
    }
}

} // namespace nx::vms::client::core::test
