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
#include <nx/vms/server/event/server_runtime_event_manager.h>

#include <nx/analytics/analytics_logging_ini.h>
#include <nx/analytics/frame_info.h>
#include <nx/vms/server/put_in_order_data_provider.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

static const int kMaxQueueSize = 100;

using PixelFormat = nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat;
using StreamType = nx::vms::api::analytics::StreamType;
using StreamTypes = nx::vms::api::analytics::StreamTypes;

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
    const std::chrono::milliseconds kMaxPipelineDelay(500);
    setMaxQueueDuration(AbstractDataReorderer::kMaxQueueDuration - kMaxPipelineDelay);
}

DeviceAnalyticsBinding::~DeviceAnalyticsBinding()
{
    stop();
    stopAnalytics();
}

bool DeviceAnalyticsBinding::startAnalytics(const QJsonObject& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return startAnalyticsUnsafe(settings);
}

void DeviceAnalyticsBinding::stopAnalytics()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    stopAnalyticsUnsafe();
}

bool DeviceAnalyticsBinding::restartAnalytics(const QJsonObject& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    stopAnalyticsUnsafe();
    m_deviceAgentContext = DeviceAgentContext();
    return startAnalyticsUnsafe(settings);
}

bool DeviceAnalyticsBinding::updateNeededMetadataTypes()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_DEBUG(this, "Updating needed metadata types for the Device %1 (%2) and Engine %3 (%4)",
        m_device->getUserDefinedName(), m_device->getId(), m_engine->getName(), m_engine->getId());

    if (!m_deviceAgentContext.deviceAgent)
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

    const bool result = m_deviceAgentContext.deviceAgent->setNeededMetadataTypes(
        neededMetadataTypes);

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
    if (!m_deviceAgentContext.deviceAgent)
    {
        DeviceAgentContext deviceAgentContext;
        deviceAgentContext.deviceAgent = createDeviceAgentUnsafe();
        if (!deviceAgentContext.deviceAgent)
        {
            NX_WARNING(this,
                "DeviceAgent creation failed for the Engine %1 (%2) and the Device %3 (%4)",
                m_engine->getName(), m_engine->getId(),
                m_device->getUserDefinedName(), m_device->getId());
            return false;
        }

        const auto manifest = deviceAgentContext.deviceAgent->manifest();
        if (!manifest)
            return false;

        if (!updateDescriptorsWithManifest(*manifest))
            return false;

        deviceAgentContext.handler = createHandlerUnsafe();
        deviceAgentContext.deviceAgent->setHandler(deviceAgentContext.handler);
        if (!updatePluginInfo())
            return false;

        updateDeviceWithManifest(*manifest);
        m_deviceAgentContext = deviceAgentContext;
        m_device->saveProperties();
    }

    m_cachedStreamRequirements = calculateStreamRequirements();

    if (!m_deviceAgentContext.deviceAgent)
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
        m_started = m_deviceAgentContext.deviceAgent->setNeededMetadataTypes(neededMetadataTypes);
        if (m_started)
            m_lastNeededMetadataTypes = std::move(neededMetadataTypes);
    }

    if (m_started)
        notifySettingsMaybeChanged();

    return m_started;
}

void DeviceAnalyticsBinding::stopAnalyticsUnsafe()
{
    m_started = false;
    if (!m_deviceAgentContext.deviceAgent)
        return;

    m_lastNeededMetadataTypes.reset();
    m_deviceAgentContext.deviceAgent->setNeededMetadataTypes(sdk_support::MetadataTypes());
}

QJsonObject DeviceAnalyticsBinding::getSettings() const
{
    DeviceAgentContext deviceAgentContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        deviceAgentContext = m_deviceAgentContext;
    }

    if (!deviceAgentContext.deviceAgent)
    {
        NX_WARNING(this, "Can't access DeviceAgent for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return {};
    }

    const auto settingsResponse = deviceAgentContext.deviceAgent->pluginSideSettings();
    return settingsResponse ? settingsResponse->settingValues : QJsonObject();
}

void DeviceAnalyticsBinding::setSettings(const QJsonObject& settings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    setSettingsInternal(settings);
}

void DeviceAnalyticsBinding::setSettingsInternal(const QJsonObject& settings)
{
    if (!m_deviceAgentContext.deviceAgent)
    {
        NX_DEBUG(this, "Can't access DeviceAgent for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return;
    }

    m_deviceAgentContext.deviceAgent->setSettings(settings);
    notifySettingsMaybeChanged();
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

std::optional<StreamRequirements> DeviceAnalyticsBinding::calculateStreamRequirements()
{
    if (!isStreamConsumerUnsafe())
        return std::nullopt;

    const nx::vms::api::analytics::EngineManifest manifest = m_engine->manifest();
    AVPixelFormat avPixelFormat = AV_PIX_FMT_NONE;
    StreamTypes requiredStreamTypes = manifest.streamTypeFilter;
    const std::optional<PixelFormat> pixelFormat =
        sdk_support::pixelFormatFromEngineManifest(
            manifest,
            lm("Engine %1 (%2)").args(m_engine->getName(), m_engine->getId()));

    if (pixelFormat)
    {
        avPixelFormat = sdk_support::sdkToAvPixelFormat(*pixelFormat);
        if (!NX_ASSERT(avPixelFormat != AV_PIX_FMT_NONE,
            "Unable to convert SDK pixel format %1 to Ffmpeg pixel format", (int)*pixelFormat))
        {
            return std::nullopt;
        }

        if (!requiredStreamTypes)
        {
            NX_DEBUG(this, "Pixel format is specified but no stream type filter is defined in the "
                "manifest of the Engine %1 (%2). Adding `uncompressedVideo` stream type to "
                "requirements. Device: %3 (%4)",
                m_engine->getName(), m_engine->getId(),
                m_device->getUserDefinedName(), m_device->getId());

            requiredStreamTypes |= StreamType::uncompressedVideo;
        }

    }
    else if (requiredStreamTypes.testFlag(StreamType::uncompressedVideo))
    {
        NX_DEBUG(this, "Uncompressed video is requested via stream type filter in the manifest of "
            "the Engine %1 (%2), but no pixel format is specified. Device %3 (%4)",
            m_engine->getName(), m_engine->getId(),
            m_device->getUserDefinedName(), m_device->getId());

        return std::nullopt;
    }
    else if (!requiredStreamTypes)
    {
        NX_DEBUG(this, "No stream type filter is specified in the manifest of the Engine %1 (%2), "
            "and pixel format is not specified."
            "Adding `compressedVideo` stream type to requirements. Device: %3 (%4)",
            m_engine->getName(), m_engine->getId(),
            m_device->getUserDefinedName(), m_device->getId());

        requiredStreamTypes |= StreamType::compressedVideo;
    }

    return StreamRequirements{
        requiredStreamTypes,
        avPixelFormat,
        m_device->analyzedStreamIndex(m_engine->getId())};
}

void DeviceAnalyticsBinding::setMetadataSink(QnAbstractDataReceptorPtr metadataSink)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_metadataSink = std::move(metadataSink);
    if (m_deviceAgentContext.handler)
        m_deviceAgentContext.handler->setMetadataSink(m_metadataSink.get());
}

bool DeviceAnalyticsBinding::isStreamConsumer() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return isStreamConsumerUnsafe();
}

bool DeviceAnalyticsBinding::isStreamConsumerUnsafe() const
{
    return m_deviceAgentContext.deviceAgent && m_deviceAgentContext.deviceAgent->isConsumer();
}

void DeviceAnalyticsBinding::notifySettingsMaybeChanged() const
{
    serverModule()->serverRuntimeEventManager()->triggerDeviceAgentSettingsMaybeChangedEvent(
        m_device->getId(),
        m_engine->getId());
}

std::optional<EngineManifest> DeviceAnalyticsBinding::engineManifest() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!NX_ASSERT(m_engine))
        return std::nullopt;

    return m_engine->manifest();
}

wrappers::DeviceAgentPtr DeviceAnalyticsBinding::createDeviceAgentUnsafe()
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

nx::sdk::Ptr<DeviceAgentHandler> DeviceAnalyticsBinding::createHandlerUnsafe()
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

    const auto deviceAgentManifest = m_deviceAgentContext.deviceAgent->manifest();
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
    if (!isRunning())
        start();

    base_type::putData(data);
}

std::optional<StreamRequirements> DeviceAnalyticsBinding::streamRequirements() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_cachedStreamRequirements;
}

void DeviceAnalyticsBinding::recalculateStreamRequirements()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cachedStreamRequirements = calculateStreamRequirements();
}

bool DeviceAnalyticsBinding::canAcceptData() const
{
    auto queue = m_dataQueue.lock();
    if (queue.size() > 0 && m_maxQueueDuration.count() > 0
        && queue.last()->timestamp - queue.front()->timestamp >= m_maxQueueDuration.count())
    {
        return false;
    }
    return QnAbstractDataConsumer::canAcceptData();
}

void DeviceAnalyticsBinding::setMaxQueueDuration(std::chrono::microseconds value)
{
    m_maxQueueDuration = value;
}

bool DeviceAnalyticsBinding::processData(const QnAbstractDataPacketPtr& data)
{
    DeviceAgentContext deviceAgentContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        deviceAgentContext = m_deviceAgentContext;
    }

    // Returning true means the data has been processed.
    if (!deviceAgentContext.deviceAgent)
    {
        NX_WARNING(this, lm("DeviceAgent is not created for the Device %1 (%2) and the Engine %3")
            .args(m_device->getUserDefinedName(), m_device->getId(), m_engine->getName()));

        return true;
    }

    if (!NX_ASSERT(deviceAgentContext.deviceAgent->isConsumer()))
        return true;

    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    if (!NX_ASSERT(packetAdapter))
        return true;

    logIncomingFrame(packetAdapter->packet());
    deviceAgentContext.deviceAgent->pushDataPacket(packetAdapter->packet());
    return true;
}

} // namespace nx::vms::server::analytics
