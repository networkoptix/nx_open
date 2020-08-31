#include "device_analytics_binding.h"

#include <plugins/plugin_manager.h>
#include <plugins/vms_server_plugins_ini.h>

#include <core/resource/camera_resource.h>
#include <media_server/media_server_module.h>

#include <nx/utils/log/log.h>
#include <nx/utils/concurrent.h>

#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>
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
#include <nx/vms/server/analytics/settings_engine_wrapper.h>
#include <nx/vms/server/event/server_runtime_event_manager.h>
#include <nx/vms/server/resource/camera.h>

#include <nx/analytics/analytics_logging_ini.h>
#include <nx/analytics/frame_info.h>
#include <nx/vms/server/put_in_order_data_provider.h>
#include <nx/utils/std/algorithm.h>

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
    resource::CameraPtr device,
    resource::AnalyticsEngineResourcePtr engine)
    :
    base_type(kMaxQueueSize),
    nx::vms::server::ServerModuleAware(serverModule),
    m_device(std::move(device)),
    m_engine(std::move(engine)),
    m_incomingFrameLogger("incoming_frame_", m_device->getId(), m_engine->getId()),
    m_incomingInbandMetadataLogger(
        "incoming_inband_metadata_",
        m_device->getId(),
        m_engine->getId())
{
    const std::chrono::milliseconds kMaxPipelineDelay(500);
    setMaxQueueDuration(AbstractDataReorderer::kMaxQueueDuration - kMaxPipelineDelay);
}

DeviceAnalyticsBinding::~DeviceAnalyticsBinding()
{
    stop();
    stopAnalytics();
}

bool DeviceAnalyticsBinding::startAnalytics()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return startAnalyticsUnsafe();
}

void DeviceAnalyticsBinding::stopAnalytics()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    stopAnalyticsUnsafe();
}

bool DeviceAnalyticsBinding::restartAnalytics()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    stopAnalyticsUnsafe();
    m_deviceAgentContext = DeviceAgentContext();
    return startAnalyticsUnsafe();
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

bool DeviceAnalyticsBinding::startAnalyticsUnsafe()
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

        initializeSettingsContext();

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

    SetSettingsRequest setSettingsRequest{
        m_deviceAgentContext.settingsContext.modelId,
        m_deviceAgentContext.settingsContext.values};

    setSettingsInternal(setSettingsRequest, /*isInitialSettings*/ true);

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
        notifySettingsHaveBeenChanged();

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

SettingsResponse DeviceAnalyticsBinding::getSettings() const
{
    DeviceAgentContext deviceAgentContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        deviceAgentContext = m_deviceAgentContext;
    }

    if (!deviceAgentContext.deviceAgent)
    {
        NX_WARNING(this, "Unable to access DeviceAgent, Device: %1, Engine: %3",
            m_device, m_engine);

        return SettingsResponse(
            SettingsResponse::Error::Code::sdkObjectIsNotAccessible,
            "DeviceAgent is not accessible");
    }

    const std::optional<sdk_support::SdkSettingsResponse> sdkSettingsResponse =
        deviceAgentContext.deviceAgent->pluginSideSettings();

    if (!sdkSettingsResponse)
    {
        NX_DEBUG(this,
            "Getting settings, unable to obtain an SDK settings response, Device: %1, Engine: %2",
            m_device, m_engine);

        return SettingsResponse(SettingsResponse::Error::Code::unableToObtainSettingsResponse);
    }

    NX_MUTEX_LOCKER lock(&m_mutex);
    bool settingsHaveBeenChanged = false;

    const SettingsContext settingsContext = updateSettingsContext(
        api::analytics::SettingsValues(),
        *sdkSettingsResponse,
        &settingsHaveBeenChanged);

    if (settingsHaveBeenChanged)
        notifySettingsHaveBeenChanged();

    return prepareSettingsResponse(settingsContext, *sdkSettingsResponse);
}

SettingsResponse DeviceAnalyticsBinding::setSettings(
    const SetSettingsRequest& settingsRequest)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return setSettingsInternal(settingsRequest, /*isInitialSettings*/ false);
}

SettingsResponse DeviceAnalyticsBinding::setSettingsInternal(
    const SetSettingsRequest& settingsRequest,
    bool isInitialSettings)
{
    NX_DEBUG(this,
        "Setting settings, settings values: %1, settings model id: %2, isInitialSettings: %3, "
        "Device: %4, Engine: %5",
        settingsRequest.values, settingsRequest.modelId, isInitialSettings, m_device, m_engine);

    if (!m_deviceAgentContext.deviceAgent)
    {
        NX_DEBUG(this, "Can't access DeviceAgent for the Device %1 (%2) and the Engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return SettingsResponse::Error(
            SettingsResponse::Error::Code::sdkObjectIsNotAccessible,
            "DeviceAgent is not accessible");
    }

    const bool isAppropriateModel =
        m_deviceAgentContext.settingsContext.modelId == settingsRequest.modelId;

    if (!isAppropriateModel && !isInitialSettings)
    {
        NX_DEBUG(this,
            "Setting settings, settings request model id (%1) doesn't match current settings "
            "model id (%2), Device: %3, Engine %4",
            settingsRequest.modelId, m_deviceAgentContext.settingsContext.modelId,
            m_device, m_engine);

        return SettingsResponse::Error(SettingsResponse::Error::Code::wrongSettingsModel);
    }

    const std::optional<sdk_support::SdkSettingsResponse> sdkSettingsResponse =
        m_deviceAgentContext.deviceAgent->setSettings(
            isInitialSettings
                ? settingsRequest.values
                : prepareSettings(m_deviceAgentContext.settingsContext, settingsRequest.values));

    if (!sdkSettingsResponse)
    {
        NX_DEBUG(this,
            "Settings setttings, unable to obtain an SDK settings response, Device %1, Engine %2",
            m_device, m_engine);

        return SettingsResponse(SettingsResponse::Error::Code::unableToObtainSettingsResponse);
    }

    bool settingsHaveBeenChanged = false;
    updateSettingsContext(settingsRequest.values, *sdkSettingsResponse, &settingsHaveBeenChanged);

    if (settingsHaveBeenChanged)
        notifySettingsHaveBeenChanged();

    return prepareSettingsResponse(m_deviceAgentContext.settingsContext, *sdkSettingsResponse);
}

void DeviceAnalyticsBinding::logIncomingDataPacket(Ptr<IDataPacket> frame)
{
    if (!nx::analytics::loggingIni().isLoggingEnabled())
        return;

    m_incomingFrameLogger.pushFrameInfo({std::chrono::microseconds(frame->timestampUs())});

    if (const auto customMetadata = frame->queryInterface<ICustomMetadataPacket>())
        m_incomingInbandMetadataLogger.pushCustomMetadata(customMetadata);
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

void DeviceAnalyticsBinding::setMetadataSinks(MetadataSinkSet metadataSinks)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_metadataSinks = std::move(metadataSinks);
    if (m_deviceAgentContext.handler)
        m_deviceAgentContext.handler->setMetadataSinks(m_metadataSinks);
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

void DeviceAnalyticsBinding::notifySettingsHaveBeenChanged() const
{
    serverModule()->serverRuntimeEventManager()->triggerDeviceAgentSettingsMaybeChangedEvent(
        m_device->getId(),
        m_engine->getId(),
        makeSettingsData());
}

void DeviceAnalyticsBinding::initializeSettingsContext() const
{
    const std::optional<api::analytics::SettingsModel> modelFromManifests =
        m_device->deviceAgentSettingsModel(m_engine->getId());

    NX_DEBUG(this,
        "Initializing settings context, "
        "the model from the Device and Engine manfiests: %1. Device: %2, Engine: %3",
        (modelFromManifests ? toString(*modelFromManifests): "<null>"), m_device, m_engine);

    m_deviceAgentContext.settingsContext.model = modelFromManifests
        ? *modelFromManifests
        : api::analytics::SettingsModel();

    QJsonObject storedSettings = m_device->deviceAgentSettingsValues(m_engine->getId());
    if (storedSettings.isEmpty())
    {
        // If there are no Settings in the database we use default Settings from the Model.
        // Otherwise we pass the Settings from the database without pulling them through the Model.
        SettingsEngineWrapper settingsEngine(serverModule()->eventConnector(), m_engine, m_device);
        settingsEngine.loadModelFromJsonObject(m_deviceAgentContext.settingsContext.model);
        storedSettings = settingsEngine.values();
    }

    m_deviceAgentContext.settingsContext.modelId =
        calculateModelId(m_deviceAgentContext.settingsContext.model);
    m_deviceAgentContext.settingsContext.values = storedSettings;

    const std::optional<DeviceAgentManifest> deviceAgentManifest =
        m_device->deviceAgentManifest(m_engine->getId());

    if (!NX_ASSERT(deviceAgentManifest, "Device %1, Engine %2", m_device, m_engine))
        return;

    m_deviceAgentContext.settingsContext.saveSettingsValuesToProperty =
        !deviceAgentManifest->capabilities.testFlag(
            DeviceAgentManifest::Capability::doNotSaveSettingsValuesToProperty);

    m_deviceAgentContext.additionalSettingsData.sequenceId = 0;
    m_deviceAgentContext.additionalSettingsData.sessionId = QnUuid::createUuid();

    NX_DEBUG(this,
        "Settings context has been initialized, model: %1, model id: %2, values: %3,"
        "saveSettingsValuesToProperty: %4, Device: %5, Engine: %6",
        m_deviceAgentContext.settingsContext.model,
        m_deviceAgentContext.settingsContext.modelId,
        m_deviceAgentContext.settingsContext.values,
        m_deviceAgentContext.settingsContext.saveSettingsValuesToProperty);
}

SettingsContext DeviceAnalyticsBinding::updateSettingsContext(
    const api::analytics::SettingsValues& requestValues,
    const sdk_support::SdkSettingsResponse& sdkSettingsResponse,
    bool* outSettingsHaveBeenChanged) const
{
    if (!NX_ASSERT(outSettingsHaveBeenChanged))
        return m_deviceAgentContext.settingsContext;

    NX_DEBUG(this, "Updating settings context, current settings model: %1, "
        "current settings model id: %2, settings values from the request: %3, "
        "settings model from the SDK response: %4, settings values from the SDK response: %5 "
        "Device: %6, Engine: %7",
        m_deviceAgentContext.settingsContext.model, m_deviceAgentContext.settingsContext.modelId,
        requestValues,
        (sdkSettingsResponse.model ? toString(*sdkSettingsResponse.model) : "<null>"),
        (sdkSettingsResponse.values ? toString(*sdkSettingsResponse.values) : "<null>"),
        m_device, m_engine);

    if (sdkSettingsResponse.model)
    {
        NX_DEBUG(this,
            "Updating settings context, updating current settings model from the SDK response, "
            "Device: %1, Engine: %2",
            m_device, m_engine);

        if (m_deviceAgentContext.settingsContext.model != *sdkSettingsResponse.model)
        {
            *outSettingsHaveBeenChanged = true;
            m_deviceAgentContext.settingsContext.model = *sdkSettingsResponse.model;
        }
    }

    if (*outSettingsHaveBeenChanged)
    {
        m_deviceAgentContext.settingsContext.modelId =
            calculateModelId(m_deviceAgentContext.settingsContext.model);
    }

    SettingsEngineWrapper settingsEngine(serverModule()->eventConnector(), m_engine, m_device);
    settingsEngine.loadModelFromJsonObject(m_deviceAgentContext.settingsContext.model);

    // Applying current settings values.
    settingsEngine.applyValues(m_deviceAgentContext.settingsContext.values);

    // Applying values from the settings request.
    settingsEngine.applyValues(requestValues);

    // Overriding values with ones from the SDK response (if any).
    if (sdkSettingsResponse.values)
    {
        NX_DEBUG(this,
            "Updating settings context, applying settings values from the SDK response, "
            "Device: %1, Engine: %2",
            m_device, m_engine);

        settingsEngine.applyStringValues(*sdkSettingsResponse.values);
    }

    nx::vms::api::analytics::SettingsValues newValues = settingsEngine.values();
    if (newValues != m_deviceAgentContext.settingsContext.values)
    {
        *outSettingsHaveBeenChanged = true;
        m_deviceAgentContext.settingsContext.values = std::move(newValues);
    }

    NX_DEBUG(this, "Updating settings context, resulting settings values: %1, "
        "Device: %2, Engine: %3",
        m_deviceAgentContext.settingsContext.values, m_device, m_engine);

    if (*outSettingsHaveBeenChanged)
        ++m_deviceAgentContext.additionalSettingsData.sequenceId;

    if (m_deviceAgentContext.settingsContext.saveSettingsValuesToProperty)
    {
        NX_DEBUG(this, "Saving settings values to property, Device: %1, Engine %2",
            m_device, m_engine);

        m_device->setDeviceAgentSettingsValues(
            m_engine->getId(),
            m_deviceAgentContext.settingsContext.values);

        m_device->saveProperties();
    }

    return m_deviceAgentContext.settingsContext;
}

api::analytics::SettingsValues DeviceAnalyticsBinding::prepareSettings(
    const SettingsContext& settingsContext,
    const api::analytics::SettingsValues& settingsValues) const
{
    NX_DEBUG(this,
        "Preparing settings, settings values: %1, settings values from "
        "the current settings context %2, settings model from the current settings context: %3, "
        "Device %4, Engine %5",
        settingsValues, settingsContext.values, settingsContext.model,  m_device, m_engine);

    SettingsEngineWrapper settingsEngine(serverModule()->eventConnector(), m_engine, m_device);
    settingsEngine.loadModelFromJsonObject(settingsContext.model);
    settingsEngine.applyValues(settingsContext.values);
    settingsEngine.applyValues(settingsValues);

    const api::analytics::SettingsValues result = settingsEngine.values();
    NX_DEBUG(this, "Preparing settings, resulting values: %1, Device: %2, Engine: %3",
        result, m_device, m_engine);

    return result;
}

SettingsResponse DeviceAnalyticsBinding::prepareSettingsResponse(
    const SettingsContext& settingsContext,
    const sdk_support::SdkSettingsResponse& sdkSettingsResponse) const
{
    SettingsResponse result;

    SettingsEngineWrapper settingsEngine(serverModule()->eventConnector(), m_engine, m_device);
    settingsEngine.loadModelFromJsonObject(settingsContext.model);

    result.model = settingsEngine.serializeModel();
    result.modelId = settingsContext.modelId;
    result.values = settingsContext.values;
    result.errors = sdkSettingsResponse.errors;

    if (!sdkSettingsResponse.sdkError.isOk())
    {
        NX_DEBUG(this,
            "The SDK response contains an SDK error: %1, message: %2, Device: %3, Engine: %4",
            sdkSettingsResponse.sdkError.errorCode, sdkSettingsResponse.sdkError.errorMessage,
            m_device, m_engine);

        result.error = SettingsResponse::Error(
            SettingsResponse::Error::Code::sdkError,
            NX_FMT("[%1] %2",
                sdkSettingsResponse.sdkError.errorCode,
                sdkSettingsResponse.sdkError.errorMessage));
    }

    NX_DEBUG(this,
        "Resulting settings response, settings model: %1, settings model id: %2, "
        "settings values: %3, settings errors: %4, Device: %5, Engine: %6",
        result.model, result.modelId, result.values, result.errors, m_device, m_engine);

    return result;
}

nx::vms::api::SettingsData DeviceAnalyticsBinding::makeSettingsData() const
{
    nx::vms::api::SettingsData result;
    result.sessionId = m_deviceAgentContext.additionalSettingsData.sessionId;
    result.sequenceNumber = m_deviceAgentContext.additionalSettingsData.sequenceId;
    result.modelId = m_deviceAgentContext.settingsContext.modelId;
    result.model = m_deviceAgentContext.settingsContext.model;
    result.values = m_deviceAgentContext.settingsContext.values;

    return result;
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
        serverModule(),
        m_engine,
        m_device,
        [this](const DeviceAgentManifest& manifest)
        {
            nx::utils::concurrent::run(
                serverModule()->analyticsThreadPool(),
                [this, manifest, bindingWeakRef = weak_from_this()]()
                {
                    const auto bindingStrongRef = bindingWeakRef.lock();
                    if (!bindingStrongRef)
                        return;

                    NX_MUTEX_LOCKER lock(&m_mutex);

                    updateDescriptorsWithManifest(manifest);
                    updateDeviceWithManifest(manifest);
                    m_device->saveProperties();
                });
        });

    handler->setMetadataSinks(m_metadataSinks);

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

    const std::set<QString> neededEventTypes = ruleWatcher->watchedEventsForDevice(m_device);
    NX_DEBUG(this, "Needed event types for the Device %1 (%2) from RuleWatcher: %3",
        m_device->getUserDefinedName(), m_device->getId(), containerString(neededEventTypes));

    const auto manager = serverModule()->commonModule()->analyticsEventTypeDescriptorManager();
    auto isEventTypeUsed =
        [&manager, &neededEventTypes](const QString& eventTypeId)
        {
            if (neededEventTypes.count(eventTypeId) > 0)
                return true;
            const auto& descriptor = manager->descriptor(eventTypeId);
            if (!descriptor)
                return false;
            return std::any_of(descriptor->scopes.begin(), descriptor->scopes.end(),
                [&neededEventTypes](const auto& scope)
                {
                    return neededEventTypes.count(scope.groupId) > 0;
                });
        };

    for (auto it = result.eventTypeIds.begin(); it != result.eventTypeIds.end();)
    {
        if (isEventTypeUsed(*it))
            ++it;
        else
            it = result.eventTypeIds.erase(it);
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

    logIncomingDataPacket(packetAdapter->packet());
    deviceAgentContext.deviceAgent->pushDataPacket(packetAdapter->packet());
    return true;
}

} // namespace nx::vms::server::analytics
