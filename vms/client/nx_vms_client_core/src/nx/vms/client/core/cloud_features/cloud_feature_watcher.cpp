// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_feature_watcher.h"

#include "abstract_cloud_feature_source.h"

namespace nx::vms::client::core {

namespace {

using Sources = std::vector<std::unique_ptr<AbstractCloudFeatureSource>>;

} // namespace

struct CloudFeatureWatcher::Private
{
    CloudFeatureWatcher* const q;
    const Sources sources;
    CloudFeatures features;

    CloudFeatures enabledByAllSources() const
    {
        CloudFeatures result = allCloudFeatures();
        for (const auto& source: sources)
            result &= source->enabledFeatures();
        return result;
    }

    void update()
    {
        const CloudFeatures newFeatures = enabledByAllSources();
        const CloudFeatures changedFeatures = features ^ newFeatures;
        if (!changedFeatures)
            return;

        features = newFeatures;
        emit q->featuresChanged(changedFeatures);
    }
};

CloudFeatureWatcher::CloudFeatureWatcher(
    std::vector<std::unique_ptr<AbstractCloudFeatureSource>> sources, QObject* parent):
    QObject(parent),
    d(new Private{.q = this, .sources = std::move(sources)})
{
    for (const auto& source: d->sources)
    {
        connect(source.get(),
            &AbstractCloudFeatureSource::featuresChanged,
            this,
            [this]() { d->update(); });
    }
    d->features = d->enabledByAllSources();
}

CloudFeatureWatcher::~CloudFeatureWatcher()
{
}

bool CloudFeatureWatcher::hasFeature(CloudFeature feature) const
{
    return d->features.testFlag(feature);
}

} // namespace nx::vms::client::core
