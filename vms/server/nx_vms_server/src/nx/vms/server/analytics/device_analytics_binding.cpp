#include "device_analytics_binding.h"

#include <plugins/vms_server_plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>

#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/analytics/utils.h>

#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/analytics/device_agent_handler.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
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

bool DeviceAnalyticsBinding::startAnalytics(const Ptr<IStringMap>& settings)
{
    QnMutexLocker lock(&m_mutex);
    return startAnalyticsUnsafe(settings);
}

void DeviceAnalyticsBinding::stopAnalytics()
{
    QnMutexLocker lock(&m_mutex);
    stopAnalyticsUnsafe();
}

bool DeviceAnalyticsBinding::restartAnalytics(const Ptr<IStringMap>& settings)
{
    QnMutexLocker lock(&m_mutex);
    stopAnalyticsUnsafe();
    m_deviceAgent.reset();
    return startAnalyticsUnsafe(settings);
}

bool DeviceAnalyticsBinding::updateNeededMetadataTypes()
{
    QnMutexLocker lock(&m_mutex);
    if (!m_deviceAgent)
    {
        NX_DEBUG(
            this,
            "There is no SDK device agent for device %1 (%2), and engine %3 (%4)",
            m_device->getUserDefinedName(), m_device->getId(),
            m_engine->getName(), m_engine->getId());

        return true;
    }

    const auto metadataTypes = neededMetadataTypes();
    const ResultHolder<void> result = m_deviceAgent->setNeededMetadataTypes(metadataTypes.get());

    return result.isOk();
}

bool DeviceAnalyticsBinding::startAnalyticsUnsafe(const Ptr<IStringMap>& settings)
{
    if (!m_deviceAgent)
    {
        m_deviceAgent = createDeviceAgent();
        if (!m_deviceAgent)
        {
            NX_ERROR(this, "Device agent creation failed, device %1 (%2)",
                m_device->getUserDefinedName(), m_device->getId());
            return false;
        }

        const auto manifest = loadDeviceAgentManifest(m_deviceAgent);
        if (!manifest)
        {
            NX_ERROR(this, lm("Cannot load device agent manifest, device %1 (%2)")
                .args(m_device->getUserDefinedName(), m_device->getId()));
            return false;
        }

        if (!updateDescriptorsWithManifest(*manifest))
            return false;

        m_handler = createHandler();
        m_deviceAgent->setHandler(m_handler.get());
        updateDeviceWithManifest(*manifest);
        m_device->saveProperties();
    }

    if (!m_deviceAgent)
    {
        NX_ERROR(
            this,
            "No device agent exitsts for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());
        return false;
    }

    setSettingsInternal(settings);
    if (!m_started)
    {
        const auto metadataTypes = neededMetadataTypes();
        const ResultHolder<void> result =
            m_deviceAgent->setNeededMetadataTypes(metadataTypes.get());

        m_started = result.isOk();
    }

    return m_started;
}

void DeviceAnalyticsBinding::stopAnalyticsUnsafe()
{
    m_started = false;
    if (!m_deviceAgent)
        return;

    const ResultHolder<void> result =
        m_deviceAgent->setNeededMetadataTypes(makePtr<MetadataTypes>().get());
}

QVariantMap DeviceAnalyticsBinding::getSettings() const
{
    decltype(m_deviceAgent) deviceAgent;
    {
        QnMutexLocker lock(&m_mutex);
        deviceAgent = m_deviceAgent;
    }

    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(m_engine->manifest().deviceAgentSettingsModel);
    jsonEngine.applyValues(m_device->deviceAgentSettingsValues(m_engine->getId()));

    if (!deviceAgent)
    {
        NX_WARNING(this, "Can't access device agent for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return jsonEngine.values();
    }

    const ResultHolder<const ISettingsResponse*> result = deviceAgent->pluginSideSettings();
    if (!result.isOk())
    {
        const auto errorMessage = result.errorMessage();
        NX_DEBUG(this,
            "Got an error: '%5' while obtaining device agent settings for device %1 (%2) "
            "and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId(),
            sdk_support::toErrorString(result));

        return jsonEngine.values();
    }
    else
    {
        QVariantMap settingsMap;
        const auto settingsResponse = result.value();
        if (!settingsResponse)
        {
            NX_DEBUG(this,
                "Got a null settings response while obtaining device agent settings "
                "for device %1 (%2) and engine %3 (%4)",
                m_device->getUserDefinedName(),
                m_device->getId(),
                m_engine->getName(),
                m_engine->getId());

            return jsonEngine.values();
        }

        const auto values = toPtr(settingsResponse->values());
        if (!values)
        {
            NX_DEBUG(this,
                "Got a null settings value map while obtaining device agent settings "
                "for device %1 (%2) and engine %3 (%4)",
                m_device->getUserDefinedName(),
                m_device->getId(),
                m_engine->getName(),
                m_engine->getId());

            return jsonEngine.values();
        }

        const auto count = values->count();
        for (auto i = 0; i < count; ++i)
            settingsMap.insert(values->key(i), values->value(i));

        jsonEngine.applyValues(settingsMap);
        return jsonEngine.values();
    }
}

void DeviceAnalyticsBinding::setSettings(const Ptr<IStringMap>& settings)
{
    QnMutexLocker lock(&m_mutex);
    setSettingsInternal(settings);
}

void DeviceAnalyticsBinding::setSettingsInternal(const Ptr<IStringMap>& settings)
{
    if (!m_deviceAgent)
    {
        NX_DEBUG(this, "Can't access device agent for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return;
    }

    const ResultHolder<const IStringMap*> result = m_deviceAgent->setSettings(settings.get());
    if (!result.isOk())
    {
        NX_WARNING(this, "Unable to set device agent stttings for device %1, error: '%2'",
            m_device, sdk_support::Error::fromResultHolder(result));
    }
}

void DeviceAnalyticsBinding::logIncomingFrame(nx::sdk::analytics::IDataPacket* frame)
{
    if (!nx::analytics::loggingIni().isLoggingEnabled())
        return;

    m_incomingFrameLogger.pushFrameInfo({std::chrono::microseconds(frame->timestampUs())});
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
    return m_isStreamConsumer;
}

std::optional<EngineManifest> DeviceAnalyticsBinding::engineManifest() const
{
    QnMutexLocker lock(&m_mutex);
    if (!NX_ASSERT(m_engine))
        return std::nullopt;

    return m_engine->manifest();
}

Ptr<DeviceAnalyticsBinding::DeviceAgent> DeviceAnalyticsBinding::createDeviceAgent()
{
    if (!NX_ASSERT(m_device, "Device is empty"))
        return nullptr;

    if (!NX_ASSERT(m_engine, "Engine is empty"))
        return nullptr;

    NX_DEBUG(
        this,
        lm("Creating DeviceAgent for device %1 (%2).")
            .args(m_device->getUserDefinedName(), m_device->getId()));

    auto deviceInfo = sdk_support::deviceInfoFromResource(m_device);
    if (!deviceInfo)
    {
        NX_WARNING(this, lm("Cannot create device info from device %1 (%2)")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    NX_DEBUG(this, lm("Device info for device %1 (%2): %3")
        .args(m_device->getUserDefinedName(), m_device->getId(), deviceInfo));

    const auto sdkEngine = m_engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_WARNING(this, lm("Can't access SDK engine object for engine %1 (%2)")
            .args(m_engine->getName(), m_engine->getId()));
        return nullptr;
    }

    const ResultHolder<IDeviceAgent*> result = sdkEngine->obtainDeviceAgent(deviceInfo.get());
    if (!result.isOk())
    {
        NX_ERROR(this, "Cannot obtain DeviceAgent %1 (%2), Engine returned an error: '%3'",
            m_device->getUserDefinedName(), m_device->getId(), sdk_support::toErrorString(result));
        return nullptr;
    }

    const auto deviceAgent = result.value();
    if (!deviceAgent)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned null")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    const auto streamConsumer = queryInterfacePtr<IConsumingDeviceAgent>(deviceAgent);

    m_isStreamConsumer = streamConsumer != nullptr;
    return deviceAgent;
}

nx::sdk::Ptr<DeviceAgentHandler> DeviceAnalyticsBinding::createHandler()
{
    if (!NX_ASSERT(m_engine, "No analytics engine is set"))
        return nullptr;

    auto handler = nx::sdk::makePtr<DeviceAgentHandler>(
        serverModule(), m_engine->getId(), m_device);
    handler->setMetadataSink(m_metadataSink.get());

    return handler;
}

std::optional<DeviceAgentManifest> DeviceAnalyticsBinding::loadDeviceAgentManifest(
    const Ptr<DeviceAgent>& deviceAgent)
{
    if (!NX_ASSERT(deviceAgent, "Invalid device agent"))
        return std::nullopt;

    if (!NX_ASSERT(m_device, "Invalid device"))
        return std::nullopt;

    const auto deviceAgentManifest = sdk_support::manifest<DeviceAgentManifest>(
        deviceAgent,
        m_device,
        m_engine,
        m_engine->plugin().dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>(),
        makeLogger("DeviceAgent"));

    if (!deviceAgentManifest)
    {
        NX_ERROR(this) << lm("Received null DeviceAgent manifest for plugin, device")
            .args(m_device->getUserDefinedName(), m_device->getId());

        return std::nullopt;
    }

    return deviceAgentManifest;
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
        NX_ERROR(this, "Can't access parent analytics plugin, device %1 (%2)",
            m_device->getUserDefinedName(), m_device->getId());
        return false;
    }

    nx::analytics::DescriptorManager descriptorManager(serverModule()->commonModule());
    descriptorManager.updateFromDeviceAgentManifest(
        m_device->getId(),
        m_engine->getId(),
        manifest);

    return true;
}

Ptr<MetadataTypes> DeviceAnalyticsBinding::neededMetadataTypes() const
{
    const auto deviceAgentManifest = sdk_support::manifest<DeviceAgentManifest>(
        m_deviceAgent,
        m_device,
        m_engine,
        m_engine->plugin().dynamicCast<nx::vms::server::resource::AnalyticsPluginResource>(),
        makeLogger("DeviceAgent"));

    if (!NX_ASSERT(deviceAgentManifest, "Got invlaid device agent manifest"))
        return Ptr<MetadataTypes>();

    using namespace nx::analytics;

    const auto eventTypes = supportedEventTypeIdsFromManifest(*deviceAgentManifest);
    const auto objectTypes = supportedObjectTypeIdsFromManifest(*deviceAgentManifest);

    const auto ruleWatcher = serverModule()->analyticsEventRuleWatcher();
    if (!NX_ASSERT(ruleWatcher, "Can't access analytics rule watcher"))
        return Ptr<MetadataTypes>();

    auto neededEventTypes = ruleWatcher->watchedEventsForResource(m_device->getId());
    for (auto it = neededEventTypes.begin(); it != neededEventTypes.end();)
    {
        if (eventTypes.find(*it) == eventTypes.cend())
            it = neededEventTypes.erase(it);
        else
            ++it;
    }

    const auto result = makePtr<MetadataTypes>();
    for (const auto& eventTypeId: neededEventTypes)
        result->addEventTypeId(eventTypeId.toStdString());

    for (const auto& objectTypeId: objectTypes)
        result->addObjectTypeId(objectTypeId.toStdString());

    return result;
}

std::unique_ptr<sdk_support::AbstractManifestLogger> DeviceAnalyticsBinding::makeLogger(
    const QString& manifestType) const
{
    const auto messageTemplate = QString(
        "Error occurred while fetching %1 manifest for device {:device} "
        "and engine {:engine}: {:error}").arg(manifestType);

    return std::make_unique<sdk_support::ManifestLogger>(
        typeid(*this), //< Using the same tag for all instances.
        messageTemplate,
        m_device,
        m_engine);
}

QVariantMap DeviceAnalyticsBinding::mergeWithDbAndDefaultSettings(
    const QVariantMap& settingsFromUser) const
{
    const auto engineManifest = m_engine->manifest();
    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(engineManifest.deviceAgentSettingsModel);

    const auto settingsFromProperty = m_device->deviceAgentSettingsValues(m_engine->getId());
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(settingsFromUser);

    return jsonEngine.values();
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
        NX_WARNING(this, lm("Device agent is not created for device %1 (%2) and engine %3")
            .args(m_device->getUserDefinedName(), m_device->getId(), m_engine->getName()));

        return true;
    }

    const auto consumingDeviceAgent = queryInterfacePtr<IConsumingDeviceAgent>(m_deviceAgent);
    if (!NX_ASSERT(consumingDeviceAgent))
        return true;

    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    if (!NX_ASSERT(packetAdapter))
        return true;

    logIncomingFrame(packetAdapter->packet());

    const ResultHolder<void> result =
        consumingDeviceAgent->pushDataPacket(packetAdapter->packet());

    if (!result.isOk())
    {
        NX_VERBOSE(this, "Plugin %1 has rejected video data with error: %2",
            m_engine->getName(), sdk_support::toErrorString(result));
    }

    return true;
}

} // namespace nx::vms::server::analytics
