#include "device_analytics_context.h"

#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <plugins/vms_server_plugins_ini.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>

#include <nx/vms/server/analytics/device_analytics_binding.h>
#include <nx/vms/server/analytics/data_converter.h>
#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>

#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/event/event_connector.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::server;

using StreamType = nx::vms::api::analytics::StreamType;
using StreamTypes = nx::vms::api::analytics::StreamTypes;

static const std::chrono::seconds kMinSkippedFramesReportingInterval(30);

static bool isAliveStatus(Qn::ResourceStatus status)
{
    return status == Qn::ResourceStatus::Online || status == Qn::ResourceStatus::Recording;
}

static std::optional<QJsonObject> mergeWithDbAndDefaultSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settingsFromUser)
{
    const auto engineManifest = engine->manifest();
    interactive_settings::JsonEngine jsonEngine;

    const std::optional<QJsonObject> deviceAgentSettingsModel =
        device->deviceAgentSettingsModel(engine->getId());

    if (!NX_ASSERT(deviceAgentSettingsModel))
        return std::nullopt;

    jsonEngine.loadModelFromJsonObject(*deviceAgentSettingsModel);

    const auto settingsFromProperty = device->deviceAgentSettingsValues(engine->getId());
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(settingsFromUser);

    return jsonEngine.values();
}

DeviceAnalyticsContext::DeviceAnalyticsContext(
    QnMediaServerModule* serverModule,
    const QnVirtualCameraResourcePtr& device)
    :
    base_type(serverModule),
    m_device(device),
    m_throwPluginEvent(
        [this](int framesSkipped, QnUuid engineId)
        {
            reportSkippedFrames(framesSkipped, engineId);
        },
        kMinSkippedFramesReportingInterval)
{
    connect(this, &DeviceAnalyticsContext::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);

    subscribeToDeviceChanges();
    subscribeToRulesChanges();
}

bool DeviceAnalyticsContext::isEngineAlreadyBoundUnsafe(const QnUuid& engineId) const
{
    return m_bindings.find(engineId) != m_bindings.cend();
}

void DeviceAnalyticsContext::setEnabledAnalyticsEngines(
    const resource::AnalyticsEngineResourceList& engines)
{
    std::set<QnUuid> engineIds;
    for (const auto& engine: engines)
        engineIds.insert(engine->getId());

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        // Remove bindings that aren't on the list.
        for (auto itr = m_bindings.begin(); itr != m_bindings.cend();)
        {
            if (engineIds.find(itr->first) == engineIds.cend())
                itr = m_bindings.erase(itr);
            else
                ++itr;
        }
    }

    const bool deviceIsAlive = isDeviceAlive();
    for (const auto& engine: engines)
    {
        std::shared_ptr<DeviceAnalyticsBinding> binding;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            if (isEngineAlreadyBoundUnsafe(engine->getId()))
                continue;

            binding = std::make_shared<DeviceAnalyticsBinding>(serverModule(), m_device, engine);
            binding->setMetadataSink(m_metadataSink);
            m_bindings.emplace(engine->getId(), binding);
        }

        if (deviceIsAlive)
        {
            const auto engineId = engine->getId();
            binding->startAnalytics(
                prepareSettings(engineId, m_device->deviceAgentSettingsValues(engineId)));
        }
    }

    updateStreamRequirements();
}

void DeviceAnalyticsContext::removeEngine(const resource::AnalyticsEngineResourcePtr& engine)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_bindings.erase(engine->getId());
    }

    updateStreamRequirements();
}

void DeviceAnalyticsContext::setMetadataSink(QWeakPointer<QnAbstractDataReceptor> metadataSink)
{
    BindingMap bindings;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_metadataSink = std::move(metadataSink);
        bindings = m_bindings;
    }

    for (auto& [_, binding]: bindings)
        binding->setMetadataSink(m_metadataSink);
}

void DeviceAnalyticsContext::setSettings(const QString& engineId, const QJsonObject& settings)
{
    QnUuid analyticsEngineId(engineId);
    const QJsonObject effectiveSettings = prepareSettings(analyticsEngineId, settings);
    m_device->setDeviceAgentSettingsValues(analyticsEngineId, effectiveSettings);

    const std::shared_ptr<DeviceAnalyticsBinding> binding =
        analyticsBindingSafe(analyticsEngineId);

    if (!binding)
        return;

    binding->setSettings(effectiveSettings);
}

QJsonObject DeviceAnalyticsContext::prepareSettings(
    const QnUuid& engineId,
    const QJsonObject& settings)
{
    using namespace nx::sdk;
    const auto engine = serverModule()
        ->resourcePool()
        ->getResourceById<nx::vms::server::resource::AnalyticsEngineResource>(engineId)
        .dynamicCast<resource::AnalyticsEngineResource>();

    if (!engine)
    {
        NX_WARNING(this,
            "Unable to access the Engine with id %1 while preparing settings", engineId);
        return {};
    }

    std::optional<QJsonObject> effectiveSettings;
    if (pluginsIni().analyticsSettingsSubstitutePath[0] != '\0')
    {
        NX_WARNING(this, "Trying to load settings for a DeviceAgent from the file. "
            "Device: %1 (%2), Engine: %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            engine->getName(),
            engine->getId());

        effectiveSettings = loadSettingsFromFile(engine);
    }

    if (!effectiveSettings)
        effectiveSettings = mergeWithDbAndDefaultSettings(m_device, engine, settings);

    if (!NX_ASSERT(effectiveSettings, "Device: %1 (%2), Engine: %3 (%4)",
        m_device->getUserDefinedName(), m_device->getId(),
        engine->getName(), engine->getId()))
    {
        return {};
    }

    return *effectiveSettings;
}

std::optional<QJsonObject> DeviceAnalyticsContext::loadSettingsFromFile(
    const resource::AnalyticsEngineResourcePtr& engine) const
{
    using namespace nx::vms::server::sdk_support;

    std::optional<QJsonObject> result = loadSettingsFromSpecificFile(
        engine,
        FilenameGenerationOption::engineSpecific | FilenameGenerationOption::deviceSpecific);

    if (result)
        return result;

    result = loadSettingsFromSpecificFile(engine, FilenameGenerationOption::deviceSpecific);
    if (result)
        return result;

    result = loadSettingsFromSpecificFile(engine, FilenameGenerationOption::engineSpecific);
    if (result)
        return result;

    return loadSettingsFromSpecificFile(engine, FilenameGenerationOptions());
}

std::optional<QJsonObject> DeviceAnalyticsContext::loadSettingsFromSpecificFile(
    const resource::AnalyticsEngineResourcePtr& engine,
    sdk_support::FilenameGenerationOptions filenameGenerationOptions) const
{
    const QString settingsFilename = sdk_support::debugFileAbsolutePath(
        pluginsIni().analyticsSettingsSubstitutePath,
        sdk_support::baseNameOfFileToDumpOrLoadData(
            engine->plugin().dynamicCast<resource::AnalyticsPluginResource>(),
            engine,
            m_device,
            filenameGenerationOptions)) + "_settings.json";

    std::optional<QString> settingsString = sdk_support::loadStringFromFile(
        nx::utils::log::Tag(typeid(this)),
        settingsFilename);

    if (!settingsString)
        return std::nullopt;

    return sdk_support::toQJsonObject(*settingsString);
}

QJsonObject DeviceAnalyticsContext::getSettings(const QString& engineId) const
{
    const QnUuid analyticsEngineId(engineId);
    const auto engine = serverModule()
        ->resourcePool()
        ->getResourceById<resource::AnalyticsEngineResource>(analyticsEngineId);

    if (!engine)
    {
        NX_WARNING(this,
            "Unable to access the Engine %1 while getting a DeviceAgent settings",
            analyticsEngineId);

        return {};
    }

    interactive_settings::JsonEngine jsonEngine;
    const std::optional<QJsonObject> deviceAgentSettingsModel = m_device->deviceAgentSettingsModel(
        QnUuid::fromStringSafe(engineId));

    if (!deviceAgentSettingsModel)
    {
        NX_WARNING(this,
            "Unable to access DeviceAgent settings model for the Device %1 (%2), Engine %1 (%2)",
            m_device->getUserDefinedName(), m_device->getId(),
            engine->getName(), engine->getId());

        return {};
    }

    jsonEngine.loadModelFromJsonObject(*deviceAgentSettingsModel);
    jsonEngine.applyValues(m_device->deviceAgentSettingsValues(analyticsEngineId));

    const std::shared_ptr<DeviceAnalyticsBinding> binding =
        analyticsBindingSafe(analyticsEngineId);

    if (!binding)
    {
        NX_DEBUG(this, "No DeviceAnalyticsBinding for the Device %1 and the Engine %2",
            m_device, engineId);

        return jsonEngine.values();
    }

    const QJsonObject pluginSideSettings = binding->getSettings();
    jsonEngine.applyValues(pluginSideSettings);
    return jsonEngine.values();
}

std::map<QnUuid, bool> DeviceAnalyticsContext::bindingStatuses() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    std::map<QnUuid, bool> result;
    for (const auto& [engineId, binding]: m_bindings)
        result[engineId] = binding->hasAliveDeviceAgent();

    return result;
}

static QString getEngineLogLabel(QnMediaServerModule* serverModule, QnUuid engineId)
{
    const auto& engineResource =
        serverModule->resourcePool()->getResourceById<resource::AnalyticsEngineResource>(
            engineId);

    if (!NX_ASSERT(engineResource))
        return "unknown Analytics Engine";

    const auto parentPlugin = engineResource->plugin();
    if (!NX_ASSERT(parentPlugin))
        return lm("Analytics Engine %1 of unknown Plugin").arg(engineResource->getId());

    return lm("Analytics Engine %1 of Plugin %2").args(
        engineResource->getId(), parentPlugin->getName());
}

static std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat>
    pixelFormatForEngine(
        QnMediaServerModule* serverModule,
        QnUuid engineId,
        const std::shared_ptr<nx::vms::server::analytics::DeviceAnalyticsBinding>& binding,
        bool* outSuccess)
{
    *outSuccess = false;
    const QString engineLogLabel = getEngineLogLabel(serverModule, engineId);

    if (!NX_ASSERT(binding, engineLogLabel))
        return std::nullopt;

    const auto engineManifest = binding->engineManifest();
    if (!NX_ASSERT(engineManifest, engineLogLabel))
        return std::nullopt;

    *outSuccess = true;
    return sdk_support::pixelFormatFromEngineManifest(*engineManifest, engineLogLabel);
}

void DeviceAnalyticsContext::putData(const QnAbstractDataPacketPtr& data)
{
    struct DataConversionContext
    {
        StreamRequirements requirements;
        QnAbstractDataPacketPtr convertedData;
    };

    NX_VERBOSE(this, "Processing data, timestamp %1 us", data->timestamp);

    if (!NX_ASSERT(data))
        return;

    std::map</*engineId*/ QnUuid, DataConversionContext> dataConversionContextByEngineId;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for (const auto& [engineId, binding]: m_bindings)
        {
            if (m_skippedPacketCountByEngine[engineId] > 0)
            {
                // Report skipped frames.
                if (m_throwPluginEvent(m_skippedPacketCountByEngine[engineId], engineId))
                    m_skippedPacketCountByEngine[engineId] = 0;
            }

            const std::optional<StreamRequirements> requirements = binding->streamRequirements();
            if (!requirements)
            {
                NX_DEBUG(this, "Unable to get stream requirements for the Engine %1", engineId);
                continue;
            }

            dataConversionContextByEngineId.emplace(
                engineId,
                DataConversionContext{*requirements, nullptr});
        }
    }

    DataConverter dataConverter(m_device->getProperty(QnMediaResource::rotationKey()).toInt());
    for (const auto& [engineId, context]: dataConversionContextByEngineId)
    {
        dataConversionContextByEngineId[engineId].convertedData =
            dataConverter.convert(data, context.requirements);
    }

    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& [engineId, conversionContext]: dataConversionContextByEngineId)
    {
        if (!conversionContext.convertedData)
        {
            NX_VERBOSE(this, "No data has been converted for the Engine %1", engineId);
            continue;
        }

        if (auto binding = analyticsBindingUnsafe(engineId))
        {
            if (binding->canAcceptData()
                && NX_ASSERT(conversionContext.convertedData->timestamp >= 0))
            {
                binding->putData(conversionContext.convertedData);
            }
            else
            {
                NX_VERBOSE(this, "The binding for the Engine %1 is unable to accept data",
                    engineId);
                ++m_skippedPacketCountByEngine[engineId];
            }
        }
    }
}

StreamTypes DeviceAnalyticsContext::requiredStreamTypes() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_cachedRequiredStreamTypes;
}

void DeviceAnalyticsContext::reportSkippedFrames(int framesSkipped, QnUuid engineId) const
{
    const QString caption = lm("Analytics: skipped %1 video frame%2").args(
        framesSkipped,
        framesSkipped > 1 ? "s" : "");

    const QString description =
        lm("Skipped %1 video frame%2 for %3 from Device %4 %5: queue overflow. "
            "Probably the Plugin is working too slow").args(
            framesSkipped,
            framesSkipped > 1 ? "s" : "",
            getEngineLogLabel(serverModule(), engineId),
            m_device->getUserDefinedName(),
            m_device->getId());

    const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new nx::vms::event::PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            engineId,
            caption,
            description,
            nx::vms::api::EventLevel::WarningEventLevel,
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
    NX_INFO(this, description);
}

void DeviceAnalyticsContext::at_deviceStatusChanged(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device, "Invalid Device"))
        return;

    const Qn::ResourceStatus currentDeviceStatus = device->getStatus();
    const Qn::ResourceStatus previousDeviceStatus =
        m_previousDeviceStatus.exchange(currentDeviceStatus);

    if (isAliveStatus(currentDeviceStatus) == isAliveStatus(previousDeviceStatus))
        return;

    at_deviceUpdated(resource);
}

void DeviceAnalyticsContext::at_deviceUpdated(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device, "Invalid Device"))
        return;

    BindingMap bindings = analyticsBindingsSafe();
    const auto isAlive = isDeviceAlive();
    for (auto& [engineId, binding]: bindings)
    {
        if (isAlive)
        {
            binding->restartAnalytics(
                prepareSettings(engineId, m_device->deviceAgentSettingsValues(engineId)));
        }
        else
        {
            binding->stopAnalytics();
        }
    }

    updateStreamRequirements();
}

void DeviceAnalyticsContext::at_devicePropertyChanged(
    const QnResourcePtr& resource,
    const QString& propertyName)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ASSERT(false, "Invalid Device");
        return;
    }

    if (propertyName == ResourcePropertyKey::kCredentials)
    {
        NX_DEBUG(
            this,
            lm("Credentials have been changed for the Device %1 (%2)")
                .args(device->getName(), device->getId()));

        at_deviceUpdated(device);
    }
}

void DeviceAnalyticsContext::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
    NX_DEBUG(this, "Rules have been updated, affected resources %1", affectedResources);
    if (!affectedResources.contains(m_device->getId()))
    {
        NX_DEBUG(this, "Device %1 (%2) is not in the list of affected resources",
            m_device->getUserDefinedName(), m_device->getId());
        return;
    }

    BindingMap bindings = analyticsBindingsSafe();
    for (auto& [_, binding]: bindings)
        binding->updateNeededMetadataTypes();
}

void DeviceAnalyticsContext::subscribeToDeviceChanges()
{
    connect(
        m_device, &QnResource::statusChanged,
        this, &DeviceAnalyticsContext::at_deviceStatusChanged,
        Qt::QueuedConnection);

    connect(
        m_device, &QnResource::urlChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated,
        Qt::QueuedConnection);

    connect(
        m_device, &QnVirtualCameraResource::logicalIdChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated,
        Qt::QueuedConnection);

    connect(
        m_device, &QnResource::propertyChanged,
        this, &DeviceAnalyticsContext::at_devicePropertyChanged,
        Qt::QueuedConnection);
}

void DeviceAnalyticsContext::subscribeToRulesChanges()
{
    auto ruleWatcher = serverModule()->analyticsEventRuleWatcher();
    connect(
        ruleWatcher, &EventRuleWatcher::rulesUpdated,
        this, &DeviceAnalyticsContext::at_rulesUpdated);
}

bool DeviceAnalyticsContext::isDeviceAlive() const
{
    if (!m_device)
        return false;

    const auto flags = m_device->flags();
    return isAliveStatus(m_device->getStatus())
        && !flags.testFlag(Qn::foreigner)
        && !flags.testFlag(Qn::desktop_camera);
}

void DeviceAnalyticsContext::updateStreamRequirements()
{
    BindingMap bindings = analyticsBindingsSafe();
    StreamTypes totalRequiredStreamTypes;
    for (const auto& [engineId, binding]: bindings)
    {
        const std::optional<StreamRequirements> requirements = binding->streamRequirements();
        if (!requirements)
            continue;

        totalRequiredStreamTypes |= requirements->requiredStreamTypes;
    }

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_cachedRequiredStreamTypes = totalRequiredStreamTypes;
}

DeviceAnalyticsContext::BindingMap DeviceAnalyticsContext::analyticsBindingsSafe() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_bindings;
}

std::shared_ptr<DeviceAnalyticsBinding> DeviceAnalyticsContext::analyticsBindingUnsafe(
    const QnUuid& engineId) const
{
    auto itr = m_bindings.find(engineId);
    if (itr == m_bindings.cend())
        return nullptr;

    return itr->second;
}

std::shared_ptr<DeviceAnalyticsBinding> DeviceAnalyticsContext::analyticsBindingSafe(
    const QnUuid& engineId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return analyticsBindingUnsafe(engineId);
}

} // namespace nx::vms::server::analytics
