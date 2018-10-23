#pragma once

#include <nx/vms/common/resource/analytics_engine_resource.h>

#include <nx/sdk/analytics/engine.h>
#include <nx/mediaserver/sdk_support/pointers.h>

#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/server_module_aware.h>

namespace nx::mediaserver::resource {

class AnalyticsEngineResource:
    public nx::vms::common::AnalyticsEngineResource,
    public nx::mediaserver::ServerModuleAware
{
    using base_type = nx::vms::common::AnalyticsEngineResource;

public:
    AnalyticsEngineResource(QnMediaServerModule* serverModule);

    void setSdkEngine(sdk_support::SharedPtr<nx::sdk::analytics::Engine> sdkEngine);

    sdk_support::SharedPtr<nx::sdk::analytics::Engine> sdkEngine() const;

protected:
    virtual CameraDiagnostics::Result initInternal() override;

private:
    sdk_support::SharedPtr<nx::sdk::analytics::Engine> m_sdkEngine;
};

} // namespace nx::mediaserver::resource
