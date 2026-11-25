// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "cloud_feature.h"

namespace nx::vms::client::core {

/**
 * Emits featuresChanged() synchronously, right after the value of enabledFeatures() changed;
 * emitting it without an actual change is allowed.
 */
class NX_VMS_CLIENT_CORE_API AbstractCloudFeatureSource: public QObject
{
    Q_OBJECT

public:
    explicit AbstractCloudFeatureSource(QObject* parent = nullptr);
    virtual ~AbstractCloudFeatureSource() override;

    /** The features this source does not disable. */
    virtual CloudFeatures enabledFeatures() const = 0;

signals:
    void featuresChanged();
};

} // namespace nx::vms::client::core
