#include "device_analytics_binding.h"

#include <plugins/plugin_manager.h>
#include <plugins/vms_server_plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/helpers/to_string.h>

#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/ptr.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/analytics/utils.h>

#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/device_agent_handler.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/analytics/wrappers/plugin.h>
#include <nx/vms/server/analytics/wrappers/engine.h>
#include <nx/vms/server/analytics/wrappers/device_agent.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/interactive_settings/json_engine.h>

#include <nx/analytics/analytics_logging_ini.h>
#include <nx/analytics/frame_info.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

template<typename T>
using ResultHolder = nx::vms::server::sdk_support::ResultHolder<T>;

static const int kMaxQueueSize = 100;

DeviceAnalyticsBinding::DeviceAnalyticsBinding(
    QnMediaServerModule* serverModule,
    QnVirtualCameraResourcePtr device,
    resource::AnalyticsEngineResourcePtr engine)
    :
    base_type(kMaxQueueSize),
    nx::vms::server::ServerModuleAware(serverModule),
    m_device(std::move(device)),
    m_engine(std::move(engine)),
    m_incomingFrameLogger("incoming_frame_", m_device->getId(), m_engine->getId())
{
}

DeviceAnalyticsBinding::~DeviceAnalyticsBinding()
{
    stop();
    stopAnalytics();
}

bool DeviceAnalyticsBinding::startAnalytics(const QJsonObject& settings)
{
    QnMutexLocker lock(&m_mutex);
    return startAnalyticsUnsafe(settings);
}

void DeviceAnalyticsBinding::stopAnalytics()
{
    QnMutexLocker lock(&m_mutex);
    stopAnalyticsUnsafe();
}

bool DeviceAnalyticsBinding::restartAnalytics(const QJsonObject& settings)
{
    QnMutexLocker lock(&m_mutex);
    stopAnalyticsUnsafe();
    m_deviceAgent.reset();
    return startAnalyticsUnsafe(settings);
}

bool DeviceAnalyticsBinding::updateNeededMetadataTypes()
{
    QnMutexLocker lock(&m_mutex);
    NX_DEBUG(this, "Updating needed metadata types for the Device %1 (%2) and Engine %3 (%4)",
        m_device->getUserDefinedName(), m_device->getId(), m_engine->getName(), m_engine->getId());

    if (!m_deviceAgent)
    {
        NX_DEBUG(
            this,
            "There is no SDK DeviceAgent for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(), m_device->getId(),
            m_engine->getName(), m_engine->getId());

        return true;
    }

    auto neededMetadataTypes = this->neededMetadataTypes();
    if (m_lastNeededMetadataTypes && (neededMetadataTypes == *m_lastNeededMetadataTypes))
    {
        NX_DEBUG(this,
            "Last needed metadata types are equal to the new ones, doing nothing. "
            "Device %1 (%2), Engine %3 (%4)",
            m_device->getUserDefinedName(), m_device->getId(),
            m_engine->getName(), m_engine->getId());
        return true;
    }

    NX_DEBUG(this,
        "Passing new needed metadata types to the DeviceAgent. Device %1 (%2), Engine %3 (%4)",
        m_device->getUserDefinedName(), m_device->getId(),
        m_engine->getName(), m_engine->getId());

    const bool result = m_deviceAgent->setNeededMetadataTypes(neededMetadataTypes);
    if (result)
        m_lastNeededMetadataTypes = std::move(neededMetadataTypes);

    return result;
}

bool DeviceAnalyticsBinding::hasAliveDeviceAgent() const
{
    return m_started.load();
}

bool DeviceAnalyticsBinding::startAnalyticsUnsafe(const QJsonObject& settings)
{
    if (!m_deviceAgent)
    {
        const auto deviceAgent = createDeviceAgent();
        if (!deviceAgent)
        {
            NX_ERROR(this,
                "DeviceAgent creation failed for the Engine %1 (%2) and the Device %3 (%4)",
                m_engine->getName(), m_engine->getId(),
                m_device->getUserDefinedName(), m_device->getId());
            return false;
        }

        const auto manifest = deviceAgent->manifest();
        if (!manifest)
            return false;

        if (!updateDescriptorsWithManifest(*manifest))
            return false;

        m_handler = createHandler();
        deviceAgent->setHandler(m_handler);
        if (!updatePluginInfo())
            return false;

        updateDeviceWithManifest(*manifest);
        m_deviceAgent = deviceAgent;
        m_device->saveProperties();
    }

    if (!m_deviceAgent)
    {
        NX_ERROR(
            this,
            "No DeviceAgent exitsts for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());
        return false;
    }

    setSettingsInternal(settings);

    // TODO: #dmishin: Investigate why this condition is checked here; should it be NX_ASSERT()?
    if (!m_started)
    {
        m_lastNeededMetadataTypes.reset();
        auto neededMetadataTypes = this->neededMetadataTypes();
        m_started = m_deviceAgent->setNeededMetadataTypes(neededMetadataTypes);
        if (m_started)
            m_lastNeededMetadataTypes = std::move(neededMetadataTypes);
    }

    return m_started;
}

void DeviceAnalyticsBinding::stopAnalyticsUnsafe()
{
    m_started = false;
    if (!m_deviceAgent)
        return;

    m_lastNeededMetadataTypes.reset();
    m_deviceAgent->setNeededMetadataTypes(sdk_support::MetadataTypes());
}

QJsonObject DeviceAnalyticsBinding::getSettings() const
{
    decltype(m_deviceAgent) deviceAgent;
    {
        QnMutexLocker lock(&m_mutex);
        deviceAgent = m_deviceAgent;
    }

    if (!deviceAgent)
    {
        NX_WARNING(this, "Can't access DeviceAgent for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return {};
    }

    const auto settingsResponse = deviceAgent->pluginSideSettings();
    return settingsResponse ? settingsResponse->settingValues : QJsonObject();
}

void DeviceAnalyticsBinding::setSettings(const QJsonObject& settings)
{
    QnMutexLocker lock(&m_mutex);
    setSettingsInternal(settings);
}

void DeviceAnalyticsBinding::setSettingsInternal(const QJsonObject& settings)
{
    if (!m_deviceAgent)
    {
        NX_DEBUG(this, "Can't access DeviceAgent for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return;
    }

    m_deviceAgent->setSettings(settings);
}

void DeviceAnalyticsBinding::logIncomingFrame(Ptr<IDataPacket> frame)
{
    if (!nx::analytics::loggingIni().isLoggingEnabled())
        return;

    m_incomingFrameLogger.pushFrameInfo({std::chrono::microseconds(frame->timestampUs())});
}

bool DeviceAnalyticsBinding::updatePluginInfo() const
{
    if (const auto pluginManager = serverModule()->pluginManager())
    {
        if (!NX_ASSERT(m_engine))
            return false;

        const auto plugin = m_engine->plugin();
        if (!NX_ASSERT(plugin))
            return false;

        const auto serverPlugin =
            plugin.dynamicCast<server::resource::AnalyticsPluginResource>();

        if (!NX_ASSERT(serverPlugin))
            return false;

        pluginManager->setIsActive(
            serverPlugin->sdkPlugin()->sdkObject().get(),
            /*isActive*/ true);
    }

    return true;
}

void DeviceAnalyticsBinding::setMetadataSink(QnAbstractDataReceptorPtr metadataSink)
{
    QnMutexLocker lock(&m_mutex);
    m_metadataSink = std::move(metadataSink);
    if (m_handler)
        m_handler->setMetadataSink(m_metadataSink.get());
}

bool DeviceAnalyticsBinding::isStreamConsumer() const
{
    return m_deviceAgent && m_deviceAgent->isConsumer();
}

std::optional<EngineManifest> DeviceAnalyticsBinding::engineManifest() const
{
    QnMutexLocker lock(&m_mutex);
    if (!NX_ASSERT(m_engine))
        return std::nullopt;

    return m_engine->manifest();
}

wrappers::DeviceAgentPtr DeviceAnalyticsBinding::createDeviceAgent()
{
    if (!NX_ASSERT(m_device, "Device is empty"))
        return nullptr;

    if (!NX_ASSERT(m_engine, "Engine is empty"))
        return nullptr;

    NX_DEBUG(
        this,
        lm("Creating a DeviceAgent for the Device %1 (%2)")
            .args(m_device->getUserDefinedName(), m_device->getId()));

    const auto sdkEngine = m_engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_WARNING(this, lm("Can't access an SDK Engine object for the Engine %1 (%2)")
            .args(m_engine->getName(), m_engine->getId()));
        return nullptr;
    }

    const auto deviceAgent = sdkEngine->obtainDeviceAgent(m_device);
    if (!deviceAgent || !deviceAgent->isValid())
        return nullptr;

    return deviceAgent;
}

nx::sdk::Ptr<DeviceAgentHandler> DeviceAnalyticsBinding::createHandler()
{
    if (!NX_ASSERT(m_engine, "No Analytics Engine is set"))
        return nullptr;

    auto handler = nx::sdk::makePtr<DeviceAgentHandler>(
        serverModule(), m_engine->getId(), m_device);
    handler->setMetadataSink(m_metadataSink.get());

    return handler;
}

bool DeviceAnalyticsBinding::updateDeviceWithManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    m_device->setDeviceAgentManifest(m_engine->getId(), manifest);
    return true;
}

bool DeviceAnalyticsBinding::updateDescriptorsWithManifest(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest)
{
    const auto parentPlugin = m_engine->plugin();
    if (!parentPlugin)
    {
        NX_ERROR(this,
            "Can't access the parent Analytics Plugin for the Device %1 (%2) "
            "and the Engine %3 (%4)",
            m_device->getUserDefinedName(), m_device->getId(),
            m_engine->getName(), m_engine->getId());
        return false;
    }

    nx::analytics::DescriptorManager descriptorManager(serverModule()->commonModule());
    descriptorManager.updateFromDeviceAgentManifest(
        m_device->getId(),
        m_engine->getId(),
        manifest);

    return true;
}

sdk_support::MetadataTypes DeviceAnalyticsBinding::neededMetadataTypes() const
{
    NX_DEBUG(this, "Fetching needed metadata types from RuleWatcher for the Device %1 (%2)",
        m_device->getUserDefinedName(), m_device->getId());

    const auto deviceAgentManifest = m_deviceAgent->manifest();
    if (!deviceAgentManifest)
        return {};

    const auto ruleWatcher = serverModule()->analyticsEventRuleWatcher();
    if (!NX_ASSERT(ruleWatcher, "Can't access Analytics RuleWatcher"))
        return {};

    sdk_support::MetadataTypes result;

    result.eventTypeIds = nx::analytics::supportedEventTypeIdsFromManifest(*deviceAgentManifest);
    result.objectTypeIds = nx::analytics::supportedObjectTypeIdsFromManifest(*deviceAgentManifest);

    const auto neededEventTypes = ruleWatcher->watchedEventsForResource(m_device->getId());
    NX_DEBUG(this, "Needed event types for the Device %1 (%2) from RuleWatcher: %3",
        m_device->getUserDefinedName(), m_device->getId(), neededEventTypes);

    for (auto it = result.eventTypeIds.begin(); it != result.eventTypeIds.end();)
    {
        if (!neededEventTypes.contains(*it))
            it = result.eventTypeIds.erase(it);
        else
            ++it;
    }

    // TODO: #dmishin write a normal container toString method.
    const auto containerToString =
        [](const auto& container)
        {
            QString result("{");
            for (auto itr = container.cbegin(); itr != container.cend(); ++itr)
            {
                result += *itr;
                if (std::next(itr) != container.cend())
                    result += ", ";
            }
            result += "}";
            return result;
        };

    NX_DEBUG(this, "Filtered needed event types list for resource %1 (%2): %3",
        m_device->getUserDefinedName(), m_device->getId(), containerToString(result.eventTypeIds));

    return result;
}

void DeviceAnalyticsBinding::putData(const QnAbstractDataPacketPtr& data)
{
    QnMutexLocker lock(&m_mutex);
    if (!isRunning())
        start();

    base_type::putData(data);
}

bool DeviceAnalyticsBinding::processData(const QnAbstractDataPacketPtr& data)
{
    // Returning true means the data has been processed.
    if (!m_deviceAgent)
    {
        NX_WARNING(this, lm("DeviceAgent is not created for the Device %1 (%2) and the Engine %3")
            .args(m_device->getUserDefinedName(), m_device->getId(), m_engine->getName()));

        return true;
    }

    if (!NX_ASSERT(m_deviceAgent->isConsumer()))
        return true;

    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    if (!NX_ASSERT(packetAdapter))
        return true;

    logIncomingFrame(packetAdapter->packet());
    m_deviceAgent->pushDataPacket(packetAdapter->packet());
    return true;
}

} // namespace nx::vms::server::analytics
