// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <nx/vms/client/core/cloud_features/abstract_cloud_feature_source.h>
#include <nx/vms/client/core/cloud_features/cloud_feature.h>
#include <nx/vms/client/core/cloud_features/cloud_feature_watcher.h>

#include "test_cloud_features.h"

namespace nx::vms::client::core::test {

namespace {

using Sources = std::vector<std::unique_ptr<AbstractCloudFeatureSource>>;
using Changes = std::vector<CloudFeatures>;

class FakeSource: public AbstractCloudFeatureSource
{
public:
    virtual CloudFeatures enabledFeatures() const override { return m_enabledFeatures; }

    void setEnabledFeatures(CloudFeatures features)
    {
        m_enabledFeatures = features;
        emit featuresChanged();
    }

private:
    CloudFeatures m_enabledFeatures = kAllFeatures;
};

CloudFeatures featuresOf(const CloudFeatureWatcher& watcher)
{
    CloudFeatures result;
    for (const auto feature: nx::reflect::enumeration::allEnumValues<CloudFeature>())
        result.setFlag(feature, watcher.hasFeature(feature));
    return result;
}

class ChangeRecorder: public QObject
{
public:
    explicit ChangeRecorder(const CloudFeatureWatcher* watcher)
    {
        connect(watcher,
            &CloudFeatureWatcher::featuresChanged,
            this,
            [this, watcher](CloudFeatures changedFeatures)
            {
                changes.push_back(changedFeatures);
                featuresOnLastChange = featuresOf(*watcher);
            });
    }

    Changes changes;
    CloudFeatures featuresOnLastChange; //< As a slot sees them.
};

} // namespace

TEST(CloudFeatureWatcher, hasAllFeaturesWithoutSources)
{
    const CloudFeatureWatcher watcher{Sources()};
    EXPECT_EQ(featuresOf(watcher), kAllFeatures);
}

TEST(CloudFeatureWatcher, intersectsSources)
{
    auto first = std::make_unique<FakeSource>();
    auto second = std::make_unique<FakeSource>();
    auto* const firstSource = first.get();
    auto* const secondSource = second.get();
    firstSource->setEnabledFeatures(kLayouts | kPush | kCrashReporting);
    secondSource->setEnabledFeatures(kPush | kCrashReporting | kQrCode);

    Sources sources;
    sources.push_back(std::move(first));
    sources.push_back(std::move(second));
    CloudFeatureWatcher watcher(std::move(sources));
    EXPECT_EQ(featuresOf(watcher), kPush | kCrashReporting);

    firstSource->setEnabledFeatures(kAllFeatures);
    EXPECT_EQ(featuresOf(watcher), kPush | kCrashReporting | kQrCode);

    secondSource->setEnabledFeatures(kAllFeatures);
    EXPECT_EQ(featuresOf(watcher), kAllFeatures);

    secondSource->setEnabledFeatures(kLayouts);
    EXPECT_EQ(featuresOf(watcher), kLayouts);
}

TEST(CloudFeatureWatcher, reportsExactlyChangedFeatures)
{
    auto source = std::make_unique<FakeSource>();
    auto* const fakeSource = source.get();
    Sources sources;
    sources.push_back(std::move(source));
    CloudFeatureWatcher watcher(std::move(sources));
    ChangeRecorder recorder(&watcher);

    fakeSource->setEnabledFeatures(kPush | kCrossSite | kQrCode);
    EXPECT_EQ(recorder.changes, Changes{kLayouts | kCrashReporting});
    EXPECT_EQ(recorder.featuresOnLastChange, kPush | kCrossSite | kQrCode);

    // Both the enabled and the disabled features are reported, the untouched ones are not.
    fakeSource->setEnabledFeatures(kLayouts | kPush | kCrossSite);
    EXPECT_EQ(recorder.changes, (Changes{kLayouts | kCrashReporting, kLayouts | kQrCode}));
    EXPECT_EQ(recorder.featuresOnLastChange, kLayouts | kPush | kCrossSite);
}

TEST(CloudFeatureWatcher, doesNotReportWhenResultStaysTheSame)
{
    const CloudFeatures withoutLayouts = kPush | kCrossSite | kCrashReporting | kQrCode;

    auto first = std::make_unique<FakeSource>();
    auto second = std::make_unique<FakeSource>();
    auto* const firstSource = first.get();
    auto* const secondSource = second.get();
    firstSource->setEnabledFeatures(withoutLayouts);

    Sources sources;
    sources.push_back(std::move(first));
    sources.push_back(std::move(second));
    CloudFeatureWatcher watcher(std::move(sources));
    ChangeRecorder recorder(&watcher);

    // The same value again.
    firstSource->setEnabledFeatures(withoutLayouts);
    // Layouts are disabled by the other source already.
    secondSource->setEnabledFeatures(withoutLayouts);
    // Layouts are still disabled by the other source.
    firstSource->setEnabledFeatures(kAllFeatures);
    EXPECT_EQ(featuresOf(watcher), withoutLayouts);
    EXPECT_EQ(recorder.changes, Changes());

    secondSource->setEnabledFeatures(kAllFeatures);
    EXPECT_EQ(recorder.changes, Changes{kLayouts});
}

} // namespace nx::vms::client::core::test
