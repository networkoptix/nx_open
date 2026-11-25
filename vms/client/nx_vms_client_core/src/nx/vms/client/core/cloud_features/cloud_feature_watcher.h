// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

#include "cloud_feature.h"

namespace nx::vms::client::core {

class AbstractCloudFeatureSource;

/**
 * Cloud feature availability. Every feature is available unless some source disables it: the
 * sources can only take features away, so their order does not matter.
 */
class NX_VMS_CLIENT_CORE_API CloudFeatureWatcher: public QObject
{
    Q_OBJECT

public:
    explicit CloudFeatureWatcher(std::vector<std::unique_ptr<AbstractCloudFeatureSource>> sources,
        QObject* parent = nullptr);

    virtual ~CloudFeatureWatcher() override;

    bool hasFeature(CloudFeature feature) const;

signals:
    /** @param changedFeatures The features whose availability has just changed. */
    void featuresChanged(CloudFeatures changedFeatures);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
