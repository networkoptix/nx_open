#include "sdk_object_description.h"

#include <nx/vms/server/sdk_support/file_utils.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <core/resource/camera_resource.h>

namespace nx::vms::server::analytics::wrappers {

SdkObjectDescription::SdkObjectDescription(
    resource::AnalyticsPluginResourcePtr plugin,
    resource::AnalyticsEngineResourcePtr engine,
    QnVirtualCameraResourcePtr device)
    :
    m_plugin(plugin),
    m_engine(engine),
    m_device(device)
{
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

QnVirtualCameraResourcePtr SdkObjectDescription::device() const
{
    return m_device;
}

SdkObjectType SdkObjectDescription::sdkObjectType() const
{
    if (!m_libraryName.isEmpty())
        return SdkObjectType::plugin;

    if (m_plugin && !m_engine && !m_device)
        return SdkObjectType::plugin;
    else if (m_plugin && m_engine && !m_device)
        return SdkObjectType::engine;
    else if (m_plugin && m_engine && m_device)
        return SdkObjectType::deviceAgent;

    NX_ASSERT(false);
    return SdkObjectType::undefined;
}

QString SdkObjectDescription::descriptionString() const
{
    if (!m_libraryName.isEmpty())
        return m_libraryName;

    switch (sdkObjectType())
    {
        case SdkObjectType::plugin:
        {
            return lm("%1").args(m_plugin->getName());
        }
        case SdkObjectType::engine:
        {
            return lm("Engine %1, %2")
                .args(m_engine->getId(), m_plugin->getName());
        }
        case SdkObjectType::deviceAgent:
        {
            return lm("DeviceAgent: %1, Engine %2, %3")
                .args(m_device->getUserDefinedName(), m_engine->getId(), m_plugin->getName());
        }
        default:
        {
            NX_ASSERT(false);
            return QString();
        }
    }
}

QString SdkObjectDescription::baseInputOutputFilename() const
{
    if (!m_libraryName.isEmpty())
        return m_libraryName;

    return sdk_support::baseNameOfFileToDumpOrLoadData(m_plugin, m_engine, m_device);
}

} // namespace nx::vms::server::analytics:wrappers
