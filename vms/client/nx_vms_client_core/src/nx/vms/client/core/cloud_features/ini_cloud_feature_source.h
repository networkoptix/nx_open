// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string_view>

#include <nx/utils/impl_ptr.h>

#include "abstract_cloud_feature_source.h"

namespace nx::vms::client::core {

/** Disables the features listed in the forcefullyDisabledFeatures INI parameter. */
class NX_VMS_CLIENT_CORE_API IniCloudFeatureSource: public AbstractCloudFeatureSource
{
    Q_OBJECT

public:
    explicit IniCloudFeatureSource(QObject* parent = nullptr);

    /** Explicit token list separated by space, comma or semicolon; for tests. */
    explicit IniCloudFeatureSource(std::string_view disabledFeatures, QObject* parent = nullptr);

    virtual ~IniCloudFeatureSource() override;

    virtual CloudFeatures enabledFeatures() const override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
