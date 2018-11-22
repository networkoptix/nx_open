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
#include <nx/sdk/analytics/common_metadata_types.h>

#include <nx/mediaserver/analytics/data_packet_adapter.h>
#include <nx/mediaserver/analytics/debug_helpers.h>
#include <nx/mediaserver/analytics/device_agent_handler.h>
#include <nx/mediaserver/analytics/event_rule_watcher.h>
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
    auto result = manifest.supportedObjectTypeIds.toSet();
    for (const auto& objectType : manifest.objectTypes)
        result.insert(objectType.id);

    return result;
}

QSet<QString> supportedEventTypes(const DeviceAgentManifest& manifest)
{
    auto result = manifest.supportedEventTypeIds.toSet();
    for (const auto& eventType: manifest.eventTypes)
        result.insert(eventType.id);

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

bool DeviceAnalyticsBinding::updateNeededMetadataTypes()
{
    QnMutexLocker lock(&m_mutex);
    if (!m_sdkDeviceAgent)
    {
        NX_DEBUG(
            this,
            "There is no SDK device agent for device %1 (%2), and engine %3 (%4)",
            m_device->getUserDefinedName(), m_device->getId(),
            m_engine->getName(), m_engine->getId());

        return true;
    }

    const auto metadataTypes = neededMetadataTypes();
    const auto error = m_sdkDeviceAgent->setNeededMetadataTypes(metadataTypes.get());
    return error == nx::sdk::Error::noError;
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

        m_handler = createHandler();
        m_sdkDeviceAgent->setHandler(m_handler.get());
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

    sdk_support::UniquePtr<nx::sdk::Settings> sdkSettings;
    if (pluginsIni().analyticsDeviceAgentSettingsPath[0] != '\0')
    {
        NX_WARNING(
            this,
            "Trying to load settings for the DeviceAgent from the file. "
            "Device: %1 (%2), Engine: %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        sdkSettings = debug_helpers::loadDeviceAgentSettingsFromFile(m_device, m_engine);
    }

    if (!sdkSettings)
    {
        const auto settingValues = augmentedSettings(settings);
        sdkSettings = sdk_support::toSdkSettings(settingValues);
    }

    if (!sdkSettings)
    {
        NX_ERROR(
            this,
            "Unable to get settings for DeviceAgent. Device: %1 (%2), Engine: %3 %4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());
        return false;
    }

    m_sdkDeviceAgent->setSettings(sdkSettings.get());

    if (!m_started)
    {
        const auto metadataTypes = neededMetadataTypes();
        const auto error = m_sdkDeviceAgent->setNeededMetadataTypes(metadataTypes.get());
        m_started = error == nx::sdk::Error::noError;
    }

    return m_started;
}

void DeviceAnalyticsBinding::stopAnalyticsUnsafe()
{
    m_started = false;
    if (!m_sdkDeviceAgent)
        return;

    sdk_support::UniquePtr<nx::sdk::analytics::IMetadataTypes> metadataTypes(
        new nx::sdk::analytics::CommonMetadataTypes());
    m_sdkDeviceAgent->setNeededMetadataTypes(metadataTypes.get());
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
    jsonEngine.loadModelFromJsonObject(engineManifest.deviceAgentSettingsModel);

    const auto settingsFromProperty = m_device->deviceAgentSettingsValues(m_engine->getId());
    jsonEngine.applyValues(settingsFromProperty);

    sdk_support::UniquePtr<nx::sdk::Settings> pluginSideSettings(
        deviceAgent->pluginSideSettings());
    if (!pluginSideSettings)
    {
        NX_DEBUG(this, "Got null device agent settings for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return QVariantMap();
    }

    QVariantMap result;
    const auto count = pluginSideSettings->count();
    for (auto i = 0; i < count; ++i)
        result.insert(pluginSideSettings->key(i), pluginSideSettings->value(i));

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

    sdk_support::UniquePtr<nx::sdk::Settings> sdkSettings;
    QVariantMap settingsValues;
    if (pluginsIni().analyticsDeviceAgentSettingsPath[0] != '\0')
    {
        NX_WARNING(
            this,
            "Trying to load settings for the DeviceAgent from the file. "
            "Device: %1 (%2), Engine: %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        sdkSettings = debug_helpers::loadDeviceAgentSettingsFromFile(m_device, m_engine);
    }

    if (!sdkSettings)
    {
        settingsValues = augmentedSettings(settings);
        sdkSettings = sdk_support::toSdkSettings(settingsValues);
    }

    if (!sdkSettings)
    {
        NX_ERROR(
            this,
            "Unable to get settings for DeviceAgent. Device: %1 (%2), Engine: %3 %4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());
        return;
    }

    deviceAgent->setSettings(sdkSettings.get());
    m_device->setDeviceAgentSettingsValues(m_engine->getId(), settingsValues);
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
    if (!m_engine)
    {
        NX_ASSERT(this, lm("Can't access engine"));
        return std::nullopt;
    }

    return m_engine->manifest();
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

std::unique_ptr<DeviceAgentHandler> DeviceAnalyticsBinding::createHandler()
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

    auto handler = std::make_unique<DeviceAgentHandler>(serverModule(), m_engine, m_device);
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

    const auto deviceAgentManifest = sdk_support::manifest<DeviceAgentManifest>(
        deviceAgent, makeLogger("DeviceAgent"));

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
    m_device->setSupportedAnalyticsObjectTypeIds(
        m_engine->getId(),
        supportedObjectTypes(manifest));

    m_device->saveParams();

    return true;
}

sdk_support::UniquePtr<CommonMetadataTypes> DeviceAnalyticsBinding::neededMetadataTypes() const
{
    const auto deviceAgentManifest = sdk_support::manifest<DeviceAgentManifest>(
        m_sdkDeviceAgent, makeLogger("DeviceAgent"));

    if (!deviceAgentManifest)
    {
        NX_ASSERT(false, "Got invlaid device agent manifest");
        return sdk_support::UniquePtr<CommonMetadataTypes>();
    }

    const auto eventTypes = supportedEventTypes(*deviceAgentManifest);
    const auto objectTypes = supportedObjectTypes(*deviceAgentManifest);

    const auto ruleWatcher = serverModule()->analyticsEventRuleWatcher();
    if (!ruleWatcher)
    {
        NX_ASSERT(false, "Can't access analytics rule watcher");
        return sdk_support::UniquePtr<CommonMetadataTypes>();
    }

    auto neededEventTypes = ruleWatcher->watchedEventsForResource(m_device->getId());
    neededEventTypes.intersect(eventTypes);

    sdk_support::UniquePtr<CommonMetadataTypes> result(new CommonMetadataTypes());
    for (const auto& eventTypeId: neededEventTypes)
        result->addEventType(eventTypeId.toStdString());

    for (const auto& objectTypeId: objectTypes)
        result->addObjectType(objectTypeId.toStdString());

    return result;
}

std::unique_ptr<sdk_support::AbstractManifestLogger> DeviceAnalyticsBinding::makeLogger(
    const QString& manifestType) const
{
    const auto messageTemplate = QString(
        "Error occurred while fetching %1 manifest for device {:device} "
        "and engine {:engine}: {:error}").arg(manifestType);

    return std::make_unique<sdk_support::ManifestLogger>(
        typeid(this), //< Using the same tag for all instances.
        messageTemplate,
        m_device,
        m_engine);
}
QVariantMap DeviceAnalyticsBinding::augmentedSettings(
    const QVariantMap& settings) const
{
    const auto engineManifest = m_engine->manifest();
    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(engineManifest.deviceAgentSettingsModel);

    const auto settingsFromProperty = m_device->deviceAgentSettingsValues(m_engine->getId());
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(settings);

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
