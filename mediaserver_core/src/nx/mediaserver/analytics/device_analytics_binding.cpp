#include "device_analytics_binding.h"

#include <plugins/plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>

#include <nx/analytics/descriptor_list_manager.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/common.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/plugin.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/analytics/consuming_device_agent.h>

#include <nx/mediaserver/analytics/data_packet_adapter.h>
#include <nx/mediaserver/analytics/debug_helpers.h>
#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/traits.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/interactive_settings/json_engine.h>

namespace nx::mediaserver::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::sdk::analytics;

namespace {

static const int kMaxQueueSize = 100;

QSet<QString> supportedObjectTypes(const DeviceAgentManifest& manifest)
{
    auto result = manifest.supportedEventTypeIds.toSet();
    for (const auto& eventType: manifest.eventTypes)
        result.insert(eventType.id);

    return result;
}

QSet<QString> supportedEventTypes(const DeviceAgentManifest& manifest)
{
    auto result = manifest.supportedObjectTypeIds.toSet();
    for (const auto& objectType : manifest.objectTypes)
        result.insert(objectType.id);

    return result;
}

} // namespace

DeviceAnalyticsBinding::DeviceAnalyticsBinding(
    QnMediaServerModule* serverModule,
    QnVirtualCameraResourcePtr device,
    resource::AnalyticsEngineResourcePtr engine)
    :
    base_type(kMaxQueueSize),
    nx::mediaserver::ServerModuleAware(serverModule),
    m_device(std::move(device)),
    m_engine(std::move(engine))
{
}

DeviceAnalyticsBinding::~DeviceAnalyticsBinding()
{
    stop();
    stopAnalytics();
}

bool DeviceAnalyticsBinding::startAnalytics(const QVariantMap& settings)
{
    QnMutexLocker lock(&m_mutex);
    return startAnalyticsUnsafe(settings);
}

void DeviceAnalyticsBinding::stopAnalytics()
{
    QnMutexLocker lock(&m_mutex);
    stopAnalyticsUnsafe();
}

bool DeviceAnalyticsBinding::restartAnalytics(const QVariantMap& settings)
{
    QnMutexLocker lock(&m_mutex);
    stopAnalyticsUnsafe();
    m_sdkDeviceAgent.reset();
    return startAnalyticsUnsafe(settings);
}

bool DeviceAnalyticsBinding::startAnalyticsUnsafe(const QVariantMap& settings)
{
    if (!m_sdkDeviceAgent)
    {
        m_sdkDeviceAgent = createDeviceAgent();
        if (!m_sdkDeviceAgent)
        {
            NX_ERROR(this, "Device agent creation failed, device %1 (%2)",
                m_device->getUserDefinedName(), m_device->getId());
            return false;
        }

        auto manifest = loadDeviceAgentManifest(m_sdkDeviceAgent);
        if (!manifest)
        {
            NX_ERROR(this, lm("Cannot load device agent manifest, device %1 (%2)")
                .args(m_device->getUserDefinedName(), m_device->getId()));
            return false;
        }

        if (!updateDescriptorsWithManifest(*manifest))
            return false;

        m_metadataHandler = createMetadataHandler();
        m_sdkDeviceAgent->setMetadataHandler(m_metadataHandler.get());
        updateDeviceWithManifest(*manifest);
    }

    if (!m_sdkDeviceAgent)
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

    if (pluginsIni().analyticsDeviceAgentSettingsPath[0] != 0)
    {
        NX_WARNING(
            this,
            "Passing settings from file to device agent. "
            "Device: %1 (%2), engine: %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        debug_helpers::setDeviceAgentSettings(m_sdkDeviceAgent, m_device);
    }
    else if (m_currentSettings != settings)
    {
        const auto sdkSettings = sdk_support::toSdkSettings(settings);
        m_sdkDeviceAgent->setSettings(sdkSettings.get());
        m_currentSettings = settings;
    }

    if (!m_started)
    {
        // TODO: #dmishin Pass event list (or remove it from SDK API).
        auto error = m_sdkDeviceAgent->startFetchingMetadata(
            /*typeList*/ nullptr,
            /*typeListSize*/ 0);
        m_started = error == nx::sdk::Error::noError;
    }

    return m_started;
}

void DeviceAnalyticsBinding::stopAnalyticsUnsafe()
{
    m_started = false;
    if (!m_sdkDeviceAgent)
        return;
    m_sdkDeviceAgent->stopFetchingMetadata();
}

QVariantMap DeviceAnalyticsBinding::getSettings() const
{
    decltype(m_sdkDeviceAgent) deviceAgent;
    {
        QnMutexLocker lock(&m_mutex);
        deviceAgent = m_sdkDeviceAgent;
    }

    if (!deviceAgent)
    {
        NX_WARNING(this, "Can't access device agent for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return QVariantMap();
    }

    const auto engineManifest = m_engine->manifest();
    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.load(engineManifest.deviceAgentSettingsModel);

    const auto settingsFromProperty = m_device->deviceAgentSettingsValues(m_engine->getId());
    jsonEngine.applyValues(settingsFromProperty);

    sdk_support::UniquePtr<nx::sdk::Settings> settings(deviceAgent->settings());
    if (!settings)
    {
        NX_DEBUG(this, "Got null device agent settings for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return QVariantMap();
    }

    QVariantMap result;
    const auto count = settings->count();
    for (auto i = 0; i < count; ++i)
        result.insert(settings->key(i), settings->value(i));

    jsonEngine.applyValues(result);
    return jsonEngine.values();
}

void DeviceAnalyticsBinding::setSettings(const QVariantMap& settings)
{
    decltype(m_sdkDeviceAgent) deviceAgent;
    {
        QnMutexLocker lock(&m_mutex);
        deviceAgent = m_sdkDeviceAgent;
    }

    if (!deviceAgent)
    {
        NX_WARNING(this, "Can't access device agent for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return;
    }

    const auto engineManifest = m_engine->manifest();
    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.load(engineManifest.deviceAgentSettingsModel);

    const auto settingsFromProperty = m_device->deviceAgentSettingsValues(m_engine->getId());
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(settings);

    const auto settingsValues = jsonEngine.values();
    auto sdkSettings = sdk_support::toSdkSettings(settingsValues);
    deviceAgent->setSettings(sdkSettings.get());
    m_device->setDeviceAgentSettingsValues(m_engine->getId(), settingsValues);
}

void DeviceAnalyticsBinding::setMetadataSink(QnAbstractDataReceptorPtr metadataSink)
{
    QnMutexLocker lock(&m_mutex);
    m_metadataSink = std::move(metadataSink);
    if (m_metadataHandler)
        m_metadataHandler->setMetadataSink(m_metadataSink.get());
}

bool DeviceAnalyticsBinding::isStreamConsumer() const
{
    return m_isStreamConsumer;
}

std::optional<EngineManifest> DeviceAnalyticsBinding::engineManifest() const
{
    QnMutexLocker lock(&m_mutex);
    if (!m_engine)
    {
        NX_ASSERT(this, lm("Can't access engine"));
        return std::nullopt;
    }

    auto sdkEngine = m_engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_WARNING(this, "Can't access SDK engine object for engine %1", m_engine->getName());
        return std::nullopt;
    }

    return sdk_support::manifest<EngineManifest>(sdkEngine);
}

sdk_support::SharedPtr<DeviceAnalyticsBinding::DeviceAgent>
    DeviceAnalyticsBinding::createDeviceAgent()
{
    if (!m_device)
    {
        NX_ASSERT(false, "Device is empty");
        return nullptr;
    }

    if (!m_engine)
    {
        NX_ASSERT(false, "Engine is empty");
        return nullptr;
    }

    NX_DEBUG(
        this,
        lm("Creating DeviceAgent for device %1 (%2).")
            .args(m_device->getUserDefinedName(), m_device->getId()));

    nx::sdk::DeviceInfo deviceInfo;
    bool success = sdk_support::deviceInfoFromResource(m_device, &deviceInfo);
    if (!success)
    {
        NX_WARNING(this, lm("Cannot create device info from device %1 (%2)")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    NX_DEBUG(this, lm("Device info for device %1 (%2): %3")
        .args(m_device->getUserDefinedName(), m_device->getId(), deviceInfo));

    auto sdkEngine = m_engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_WARNING(this, lm("Can't access SDK engine object for engine %1 (%2)")
            .args(m_engine->getName(), m_engine->getId()));
        return nullptr;
    }

    nx::sdk::Error error = nx::sdk::Error::noError;
    sdk_support::SharedPtr<DeviceAgent> deviceAgent(
        sdkEngine->obtainDeviceAgent(&deviceInfo, &error));

    if (!deviceAgent)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned null.")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    sdk_support::UniquePtr<ConsumingDeviceAgent> streamConsumer(
        sdk_support::queryInterface<ConsumingDeviceAgent>(
            deviceAgent,
            IID_ConsumingDeviceAgent));

    m_isStreamConsumer = !!streamConsumer;

    if (error != nx::sdk::Error::noError)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned error %3.")
            .args(m_device->getUserDefinedName(), m_device->getId()), error);
        return nullptr;
    }

    return deviceAgent;
}

std::shared_ptr<MetadataHandler> DeviceAnalyticsBinding::createMetadataHandler()
{
    if (!m_engine)
    {
        NX_ASSERT(false, "No analytics engine is set");
        return nullptr;
    }

    const auto descriptorListManager = serverModule()
        ->commonModule()
        ->analyticsDescriptorListManager();

    auto eventDescriptors = descriptorListManager
        ->deviceDescriptors<EventTypeDescriptor>(m_device);

    auto handler = std::make_shared<MetadataHandler>(m_engine->serverModule());
    handler->setResource(m_device);
    handler->setPluginId(m_engine->plugin()->manifest().id);
    handler->setEventTypeDescriptors(eventDescriptors);
    handler->setMetadataSink(m_metadataSink.get());

    return handler;
}

std::optional<DeviceAgentManifest> DeviceAnalyticsBinding::loadDeviceAgentManifest(
    const sdk_support::SharedPtr<DeviceAgent>& deviceAgent)
{
    if (!deviceAgent)
    {
        NX_ASSERT(false, "Invalid device agent");
        return std::nullopt;
    }

    if (!m_device)
    {
        NX_ASSERT(false, "Invalid device");
        return std::nullopt;
    }

    nx::sdk::Error error = nx::sdk::Error::noError;
    auto deleter = [deviceAgent](const char* ptr) { deviceAgent->freeManifest(ptr); };
    std::unique_ptr<const char, decltype(deleter)> deviceAgentManifest{
        deviceAgent->manifest(&error), deleter};

    if (error != nx::sdk::Error::noError)
    {
        NX_ERROR(
            this,
            lm("Can not fetch manifest for device %1 (%2), plugin returned error.")
                .args(m_device->getUserDefinedName(), m_device->getId()));

        return std::nullopt;
    }

    if (!deviceAgentManifest)
    {
        NX_ERROR(this) << lm("Received null DeviceAgent manifest for plugin, device")
            .args(m_device->getUserDefinedName(), m_device->getId());

        return std::nullopt;
    }

    if (pluginsIni().analyticsManifestOutputPath[0])
    {
        sdk_support::saveManifestToFile(
            nx::utils::log::Tag(typeid(this)),
            deviceAgentManifest.get(),
            "DeviceAgent",
            m_engine->getName(),
            lit("_device_agent"));
    }

    return sdk_support::manifest<DeviceAgentManifest>(deviceAgentManifest.get());
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
    auto analyticsDescriptorListManager = sdk_support::getDescriptorListManager(serverModule());
    if (!analyticsDescriptorListManager)
    {
        NX_ERROR(this, "Can't access analytics descriptor list manager, device %1 (%2)",
            m_device->getUserDefinedGroupName(), m_device->getId());
        return false;
    }

    const auto parentPlugin = m_engine->plugin();
    if (!parentPlugin)
    {
        NX_ERROR(this, "Can't access parent analytics plugin, device %1 (%2)",
            m_device->getUserDefinedName(), m_device->getId());
        return false;
    }

    const auto pluginManifest = parentPlugin->manifest();

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<EventTypeDescriptor>(
            pluginManifest.id, manifest.eventTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<ObjectTypeDescriptor>(
            pluginManifest.id, manifest.objectTypes));

    analyticsDescriptorListManager->addDescriptors(
        sdk_support::descriptorsFromItemList<GroupDescriptor>(
            pluginManifest.id, manifest.groups));

    m_device->setSupportedAnalyticsEventTypeIds(m_engine->getId(), supportedEventTypes(manifest));
    m_device->setSupportedAnalyticsObjectTypeIds(m_engine->getId(), supportedObjectTypes(manifest));
    m_device->saveParams();

    return true;
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
    if (!m_sdkDeviceAgent)
    {
        NX_WARNING(this, lm("Device agent is not created for device %1 (%2) and engine %3")
            .args(m_device->getUserDefinedName(), m_device->getId(), m_engine->getName()));

        return true;
    }
    // Returning true means the data has been processed.
    sdk_support::UniquePtr<nx::sdk::analytics::ConsumingDeviceAgent> consumingDeviceAgent(
        sdk_support::queryInterface<nx::sdk::analytics::ConsumingDeviceAgent>(
            m_sdkDeviceAgent,
            nx::sdk::analytics::IID_ConsumingDeviceAgent));

    NX_ASSERT(consumingDeviceAgent);
    if (!consumingDeviceAgent)
        return true;

    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    NX_ASSERT(packetAdapter);
    if (!packetAdapter)
        return true;

    const nx::sdk::Error error = consumingDeviceAgent->pushDataPacket(packetAdapter->packet());
    if (error != nx::sdk::Error::noError)
    {
        NX_VERBOSE(this, "Plugin %1 has rejected video data with error %2",
            m_engine->getName(), error);
    }

    return true;
}

} // namespace nx::mediaserver::analytics
