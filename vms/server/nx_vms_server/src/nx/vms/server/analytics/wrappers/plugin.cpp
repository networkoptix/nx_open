#include "plugin.h"

#include <media_server/media_server_module.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/i_engine.h>

#include <nx/vms/server/sdk_support/error.h>
#include <nx/vms/server/analytics/wrappers/engine.h>

#include <plugins/vms_server_plugins_ini.h>

namespace nx::vms::server::analytics::wrappers {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::server::sdk_support;

Plugin::Plugin(
    QnMediaServerModule* serverModule,
    QWeakPointer<resource::AnalyticsPluginResource> pluginResource,
    sdk::Ptr<sdk::analytics::IPlugin> sdkPlugin,
    QString libraryName)
    :
    base_type(serverModule, sdkPlugin, libraryName),
    m_pluginResource(pluginResource)
{
}

Plugin::Plugin(
    QnMediaServerModule* serverModule,
    sdk::Ptr<sdk::analytics::IPlugin> sdkPlugin,
    QString libraryName)
    :
    base_type(serverModule, sdkPlugin, libraryName),
    m_libraryName(libraryName)
{
}

wrappers::EnginePtr Plugin::createEngine(const resource::AnalyticsEngineResourcePtr engineResource)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    sdk_support::TimedGuard guard = makeTimedGuard(SdkMethod::createEngine);

    const auto sdkPlugin = sdkObject();
    if (!NX_ASSERT(sdkPlugin))
        return nullptr;

    const ResultHolder<IEngine*> result = sdkPlugin->createEngine();
    if (!result.isOk())
    {
        return handleError(
            SdkMethod::createEngine,
            Error::fromResultHolder(result),
            /*returnValue*/ nullptr);
    }

    const Ptr<sdk::analytics::IEngine> sdkEngine = result.value();
    if (!sdkEngine)
    {
        return handleViolation(
            SdkMethod::createEngine,
            {ViolationType::nullEngine, /*violationDetails*/ QString()},
            /*returnValue*/ nullptr);
    }

    return std::make_shared<wrappers::Engine>(
        serverModule(),
        engineResource,
        sdkEngine,
        libName());
}

DebugSettings Plugin::makeManifestProcessorDebugSettings() const
{
    DebugSettings settings;
    settings.outputPath = pluginsIni().analyticsManifestOutputPath;
    settings.substitutePath = pluginsIni().analyticsManifestSubstitutePath;
    settings.logTag = nx::utils::log::Tag(typeid(this));

    return settings;
}

SdkObjectDescription Plugin::sdkObjectDescription() const
{
    if (!m_libraryName.isEmpty())
        return SdkObjectDescription(m_libraryName);

    const auto pluginResource = this->pluginResource();
    if (!NX_ASSERT(pluginResource))
        return SdkObjectDescription();

    return SdkObjectDescription(
        pluginResource,
        resource::AnalyticsEngineResourcePtr(),
        resource::CameraPtr());
}

resource::AnalyticsPluginResourcePtr Plugin::pluginResource() const
{
    return m_pluginResource.toStrongRef();
}

} // namespace nx::vms::server::analytics::wrappers
