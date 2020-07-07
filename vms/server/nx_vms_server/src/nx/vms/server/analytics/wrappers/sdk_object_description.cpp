#include "sdk_object_description.h"

#include <nx/kit/utils.h>

#include <nx/vms/server/sdk_support/file_utils.h>

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

namespace nx::vms::server::analytics::wrappers {

SdkObjectDescription::SdkObjectDescription(
    resource::AnalyticsPluginResourcePtr plugin,
    resource::AnalyticsEngineResourcePtr engine,
    resource::CameraPtr device)
    :
    m_plugin(plugin),
    m_engine(engine),
    m_device(device)
{
    NX_ASSERT(plugin);
    if (device)
        NX_ASSERT(engine);
    if (engine)
        NX_ASSERT(plugin == engine->plugin());
}

SdkObjectDescription::SdkObjectDescription(QString libraryName):
    m_libraryName(libraryName)
{
}

resource::AnalyticsPluginResourcePtr SdkObjectDescription::plugin() const
{
    return m_plugin;
}

resource::AnalyticsEngineResourcePtr SdkObjectDescription::engine() const
{
    return m_engine;
}

resource::CameraPtr SdkObjectDescription::device() const
{
    return m_device;
}

SdkObjectType SdkObjectDescription::sdkObjectType() const
{
    if (!m_libraryName.isEmpty())
        return SdkObjectType::plugin;

    if (m_plugin && !m_engine && !m_device)
        return SdkObjectType::plugin;
    if (m_plugin && m_engine && !m_device)
        return SdkObjectType::engine;
    if (m_plugin && m_engine && m_device)
        return SdkObjectType::deviceAgent;

    NX_ASSERT(false);
    return SdkObjectType::undefined;
}

QString SdkObjectDescription::engineCaption() const
{
    if (!NX_ASSERT(m_plugin))
        return "Unknown Engine of unknown Plugin";

    if (!NX_ASSERT(m_engine))
        return "Unknown Engine of " + m_plugin->getName();

    if (m_plugin->getName() == m_engine->getName())
        return lm("Engine of %1").args(m_plugin->getName());

    return lm("Engine %1 of %2").args(
        nx::kit::utils::toString(m_engine->getName()), m_plugin->getName());
}

QString SdkObjectDescription::descriptionString() const
{
    if (!m_libraryName.isEmpty())
        return m_libraryName;

    switch (sdkObjectType())
    {
        case SdkObjectType::plugin:
            return lm("%1").args(m_plugin->getName());
        case SdkObjectType::engine:
            return engineCaption();
        case SdkObjectType::deviceAgent:
            return lm("DeviceAgent for %1 of %2").args(
                m_device->getUserDefinedName(), engineCaption());
        case SdkObjectType::undefined:
            return "Unknown Plugin/Engine/DeviceAgent"; //< Fallback after an assertion failure.
    }
    NX_ASSERT(false);
    return "";
}

QString SdkObjectDescription::baseInputOutputFilename() const
{
    if (!m_libraryName.isEmpty())
        return m_libraryName;

    return sdk_support::baseNameOfFileToDumpOrLoadData(m_plugin, m_engine, m_device);
}

} // namespace nx::vms::server::analytics:wrappers
