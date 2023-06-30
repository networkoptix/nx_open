// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/singleton.h>
#include <nx/vms/client/core/media/analytics_metadata_provider_factory.h>

namespace nx::vms::client::core::analytics {

class NX_VMS_CLIENT_CORE_API AnalyticsMetadataProviderFactory:
    public core::AnalyticsMetadataProviderFactory,
    public Singleton<AnalyticsMetadataProviderFactory>
{
public:
    void registerMetadataProviders();
};

} // namespace nx::vms::client::desktop
