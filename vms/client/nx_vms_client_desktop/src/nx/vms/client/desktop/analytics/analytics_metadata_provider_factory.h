// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/media/analytics_metadata_provider_factory.h>
#include <nx/utils/singleton.h>

namespace nx::vms::client::desktop {

class AnalyticsMetadataProviderFactory:
    public core::AnalyticsMetadataProviderFactory,
    public Singleton<AnalyticsMetadataProviderFactory>
{
public:
    void registerMetadataProviders();
};

} // namespace nx::vms::client::desktop
