#include "device_analytics_binding.h"

#include <plugins/plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>
#include <nx/sdk/helpers/ptr.h>

#include <nx/analytics/descriptor_manager.h>
#include <nx/analytics/utils.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/vms/server/sdk_support/to_string.h>

#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/analytics/device_agent_handler.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/interactive_settings/json_engine.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::api::analytics;
using namespace nx::sdk::analytics;

namespace {

static const int kMaxQueueSize = 100;

} // namespace

DeviceAnalyticsBinding::DeviceAnalyticsBinding(
    QnMediaServerModule* serverModule,
    QnVirtualCameraResourcePtr device,
    resource::AnalyticsEngineResourcePtr engine)
    :
    base_type(kMaxQueueSize),
    nx::vms::server::ServerModuleAware(serverModule),
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

    const auto actualSettings = setSettingsInternal(settings);
    if (!actualSettings)
        return false; //< Error is already logged.

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

    const auto neededMetadataTypes = nx::sdk::makePtr<MetadataTypes>();
    m_sdkDeviceAgent->setNeededMetadataTypes(neededMetadataTypes.get());
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

    nx::sdk::Ptr<nx::sdk::IStringMap> pluginSideSettings(
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
    QnMutexLocker lock(&m_mutex);
    if (!m_sdkDeviceAgent)
    {
        NX_WARNING(this, "Can't access device agent for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());
        return;
    }

    setSettingsInternal(settings);
}

bool DeviceAnalyticsBinding::setSettingsInternal(const QVariantMap& settingsFromUser)
{
    nx::sdk::Ptr<nx::sdk::IStringMap> effectiveSettings;
    if (pluginsIni().analyticsDeviceAgentSettingsPath[0] != '\0')
    {
        NX_WARNING(this, "Trying to load settings for the DeviceAgent from the file. "
            "Device: %1 (%2), Engine: %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        effectiveSettings = debug_helpers::loadDeviceAgentSettingsFromFile(m_device, m_engine);
    }

    if (!effectiveSettings)
    {
        effectiveSettings =
            sdk_support::toIStringMap(mergeWithDbAndDefaultSettings(settingsFromUser));
    }

    if (!NX_ASSERT(effectiveSettings, lm("Device: %1 (%2), Engine: %3 (%4)").args(
        m_device->getUserDefinedName(), m_device->getId(),
        m_engine->getName(), m_engine->getId())))
    {
        return false;
    }

    if (pluginsIni().analyticsSettingsOutputPath[0] != '\0')
    {
        debug_helpers::dumpStringToFile(
            this,
            QString::fromStdString(nx::sdk::toJsonString(effectiveSettings.get())),
            pluginsIni().analyticsSettingsOutputPath,
            debug_helpers::filename(
                m_device,
                m_engine,
                nx::vms::server::resource::AnalyticsPluginResourcePtr(),
                "_effective_settings.json"));
    }

    m_sdkDeviceAgent->setSettings(effectiveSettings.get());

    m_device->setDeviceAgentSettingsValues(
        m_engine->getId(),
        sdk_support::fromIStringMap(effectiveSettings.get()));

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
    return m_isStreamConsumer;
}

std::optional<EngineManifest> DeviceAnalyticsBinding::engineManifest() const
{
    QnMutexLocker lock(&m_mutex);
    if (!NX_ASSERT(m_engine))
        return std::nullopt;

    return m_engine->manifest();
}

nx::sdk::Ptr<DeviceAnalyticsBinding::DeviceAgent>
    DeviceAnalyticsBinding::createDeviceAgent()
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

    auto sdkEngine = m_engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_WARNING(this, lm("Can't access SDK engine object for engine %1 (%2)")
            .args(m_engine->getName(), m_engine->getId()));
        return nullptr;
    }

    nx::sdk::Error error = nx::sdk::Error::noError;
    nx::sdk::Ptr<DeviceAgent> deviceAgent(sdkEngine->obtainDeviceAgent(deviceInfo.get(), &error));

    if (!deviceAgent)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned null.")
            .args(m_device->getUserDefinedName(), m_device->getId()));
        return nullptr;
    }

    auto streamConsumer = nxpt::queryInterfacePtr<IConsumingDeviceAgent>(deviceAgent,
        IID_ConsumingDeviceAgent);

    m_isStreamConsumer = streamConsumer != nullptr;

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
    if (!NX_ASSERT(m_engine, "No analytics engine is set"))
        return nullptr;

    auto handler = std::make_unique<DeviceAgentHandler>(serverModule(), m_engine, m_device);
    handler->setMetadataSink(m_metadataSink.get());

    return handler;
}

std::optional<DeviceAgentManifest> DeviceAnalyticsBinding::loadDeviceAgentManifest(
    const nx::sdk::Ptr<DeviceAgent>& deviceAgent)
{
    if (!NX_ASSERT(deviceAgent, "Invalid device agent"))
        return std::nullopt;

    if (!NX_ASSERT(m_device, "Invalid device"))
        return std::nullopt;

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

    m_device->saveProperties();

    return true;
}

nx::sdk::Ptr<MetadataTypes> DeviceAnalyticsBinding::neededMetadataTypes() const
{
    const auto deviceAgentManifest = sdk_support::manifest<DeviceAgentManifest>(
        m_sdkDeviceAgent, makeLogger("DeviceAgent"));

    if (!NX_ASSERT(deviceAgentManifest, "Got invlaid device agent manifest"))
        return nx::sdk::Ptr<MetadataTypes>();

    using namespace nx::analytics;

    const auto eventTypes = supportedEventTypeIdsFromManifest(*deviceAgentManifest);
    const auto objectTypes = supportedObjectTypeIdsFromManifest(*deviceAgentManifest);

    const auto ruleWatcher = serverModule()->analyticsEventRuleWatcher();
    if (!NX_ASSERT(ruleWatcher, "Can't access analytics rule watcher"))
        return nx::sdk::Ptr<MetadataTypes>();

    auto neededEventTypes = ruleWatcher->watchedEventsForResource(m_device->getId());
    for (auto it = neededEventTypes.begin(); it != neededEventTypes.end();)
    {
        if (eventTypes.find(*it) == eventTypes.cend())
            it = neededEventTypes.erase(it);
        else
            ++it;
    }

    nx::sdk::Ptr<MetadataTypes> result(new MetadataTypes());
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
        typeid(this), //< Using the same tag for all instances.
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

    if (!m_sdkDeviceAgent)
    {
        NX_WARNING(this, lm("Device agent is not created for device %1 (%2) and engine %3")
            .args(m_device->getUserDefinedName(), m_device->getId(), m_engine->getName()));

        return true;
    }
    auto consumingDeviceAgent = nxpt::queryInterfacePtr<IConsumingDeviceAgent>(m_sdkDeviceAgent,
        IID_ConsumingDeviceAgent);

    if (!NX_ASSERT(consumingDeviceAgent))
        return true;

    auto packetAdapter = std::dynamic_pointer_cast<DataPacketAdapter>(data);
    if (!NX_ASSERT(packetAdapter))
        return true;

    const nx::sdk::Error error = consumingDeviceAgent->pushDataPacket(packetAdapter->packet());
    if (error != nx::sdk::Error::noError)
    {
        NX_VERBOSE(this, "Plugin %1 has rejected video data with error %2",
            m_engine->getName(), error);
    }

    return true;
}

} // namespace nx::vms::server::analytics
