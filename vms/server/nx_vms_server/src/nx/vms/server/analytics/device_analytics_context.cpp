#include "device_analytics_context.h"

#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <plugins/vms_server_plugins_ini.h>
#include <utils/common/synctime.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/analytics/device_analytics_binding.h>
#include <nx/vms/server/analytics/frame_converter.h>
#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/interactive_settings/json_engine.h>
#include <nx/vms/server/event/event_connector.h>

#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/sdk/helpers/to_string.h>

#include <nx/utils/log/log.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::server;

static const std::chrono::seconds kMinSkippedFramesReportingInterval(30);

static bool isAliveStatus(Qn::ResourceStatus status)
{
    return status == Qn::ResourceStatus::Online || status == Qn::ResourceStatus::Recording;
}

static QVariantMap mergeWithDbAndDefaultSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const QVariantMap& settingsFromUser)
{
    const auto engineManifest = engine->manifest();
    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(engineManifest.deviceAgentSettingsModel);

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

bool DeviceAnalyticsContext::isEngineAlreadyBound(
    const QnUuid& engineId) const
{
    return m_bindings.find(engineId) != m_bindings.cend();
}

bool DeviceAnalyticsContext::isEngineAlreadyBound(
    const resource::AnalyticsEngineResourcePtr& engine) const
{
    return isEngineAlreadyBound(engine->getId());
}

void DeviceAnalyticsContext::setEnabledAnalyticsEngines(
    const resource::AnalyticsEngineResourceList& engines)
{
    std::set<QnUuid> engineIds;
    for (const auto& engine: engines)
        engineIds.insert(engine->getId());

    // Remove bindings that aren't on the list.
    for (auto itr = m_bindings.begin(); itr != m_bindings.cend();)
    {
        if (engineIds.find(itr->first) == engineIds.cend())
            itr = m_bindings.erase(itr);
        else
            ++itr;
    }

    const bool deviceIsAlive = isDeviceAlive();
    for (const auto& engine: engines)
    {
        if (isEngineAlreadyBound(engine))
            continue;

        auto binding = std::make_shared<DeviceAnalyticsBinding>(serverModule(), m_device, engine);
        binding->setMetadataSink(m_metadataSink);
        m_bindings.emplace(engine->getId(), binding);

        if (deviceIsAlive)
        {
            const auto engineId = engine->getId();
            binding->startAnalytics(
                prepareSettings(engineId, m_device->deviceAgentSettingsValues(engineId)));
        }
    }

    updateSupportedFrameTypes();
}

void DeviceAnalyticsContext::addEngine(const resource::AnalyticsEngineResourcePtr& engine)
{
    auto binding = std::make_shared<DeviceAnalyticsBinding>(serverModule(), m_device, engine);
    binding->setMetadataSink(m_metadataSink);
    m_bindings.emplace(engine->getId(), binding);
    updateSupportedFrameTypes();
}

void DeviceAnalyticsContext::removeEngine(const resource::AnalyticsEngineResourcePtr& engine)
{
    m_bindings.erase(engine->getId());
    updateSupportedFrameTypes();
}

void DeviceAnalyticsContext::setMetadataSink(QWeakPointer<QnAbstractDataReceptor> metadataSink)
{
    m_metadataSink = std::move(metadataSink);
    for (auto& entry: m_bindings)
    {
        auto& binding = entry.second;
        binding->setMetadataSink(m_metadataSink);
    }
}

void DeviceAnalyticsContext::setSettings(const QString& engineId, const QVariantMap& settings)
{
    QnUuid analyticsEngineId(engineId);
    const QVariantMap effectiveSettings = prepareSettings(analyticsEngineId, settings);
    m_device->setDeviceAgentSettingsValues(analyticsEngineId, effectiveSettings);

    std::shared_ptr<DeviceAnalyticsBinding> binding;
    {
        QnMutexLocker lock(&m_mutex);
        binding = analyticsBinding(analyticsEngineId);
    }
    if (!binding)
        return;

    binding->setSettings(effectiveSettings);
}

QVariantMap DeviceAnalyticsContext::prepareSettings(
    const QnUuid& engineId,
    const QVariantMap& settings)
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

    std::optional<QVariantMap> effectiveSettings;
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

std::optional<QVariantMap> DeviceAnalyticsContext::loadSettingsFromFile(
    const resource::AnalyticsEngineResourcePtr& engine) const
{
    using namespace nx::vms::server::sdk_support;

    std::optional<QVariantMap> result = loadSettingsFromSpecificFile(
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

std::optional<QVariantMap> DeviceAnalyticsContext::loadSettingsFromSpecificFile(
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

    return sdk_support::toQVariantMap(*settingsString);
}

QVariantMap DeviceAnalyticsContext::getSettings(const QString& engineId) const
{
    std::shared_ptr<DeviceAnalyticsBinding> binding;
    QnUuid analyticsEngineId(engineId);
    {
        QnMutexLocker lock(&m_mutex);
        binding = analyticsBinding(analyticsEngineId);
    }

    const auto engine = serverModule()
        ->resourcePool()
        ->getResourceById<resource::AnalyticsEngineResource>(analyticsEngineId);

    if (!engine)
    {
        NX_WARNING(this,
            "Unable to access the Engine %1 while getting a DeviceAgent settings",
            analyticsEngineId);

        return QVariantMap();
    }

    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(engine->manifest().deviceAgentSettingsModel);
    jsonEngine.applyValues(m_device->deviceAgentSettingsValues(analyticsEngineId));

    if (!binding)
    {
        NX_DEBUG(this, "No DeviceAnalyticsBinding for the Device %1 and the Engine %2",
            m_device, engineId);

        return jsonEngine.values();
    }

    const QVariantMap pluginSideSettings = binding->getSettings();
    jsonEngine.applyValues(pluginSideSettings);
    return jsonEngine.values();
}

bool DeviceAnalyticsContext::needsCompressedFrames() const
{
    return m_cachedNeedCompressedFrames;
}

AbstractVideoDataReceptor::NeededUncompressedPixelFormats
    DeviceAnalyticsContext::neededUncompressedPixelFormats() const
{
    return m_cachedUncompressedPixelFormats;
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

void DeviceAnalyticsContext::putFrame(
    const QnConstCompressedVideoDataPtr& compressedFrame,
    const CLConstVideoDecoderOutputPtr& uncompressedFrame)
{
    if (!NX_ASSERT(compressedFrame))
        return;

    NX_VERBOSE(this, "putVideoFrame(\"%1\", compressedFrame, %2)", m_device->getId(),
        uncompressedFrame ? "uncompressedFrame" : "/*uncompressedFrame*/ nullptr");

    FrameConverter frameConverter(
        compressedFrame, uncompressedFrame, &m_missingUncompressedFrameWarningIssued);

    for (auto& entry: m_bindings)
    {
        const QnUuid engineId = entry.first;
        if (m_skippedFrameCountByEngine[engineId] > 0)
        {
            if (m_throwPluginEvent(m_skippedFrameCountByEngine[engineId], engineId))
                m_skippedFrameCountByEngine[engineId] = 0;
        }

        auto& binding = entry.second;
        if (!binding->isStreamConsumer())
            continue;

        bool gotPixelFormat = false;
        const auto pixelFormat = pixelFormatForEngine(
            serverModule(), engineId, binding, &gotPixelFormat);
        if (!gotPixelFormat)
            continue;

        if (const auto dataPacket = frameConverter.getDataPacket(pixelFormat))
        {
            if (binding->canAcceptData()
                && NX_ASSERT(dataPacket->timestampUs() >= 0))
            {
                binding->putData(std::make_shared<DataPacketAdapter>(dataPacket));
            }
            else
            {
                // TODO: Skip frames until a next key frame (if case of using compressed frames).
                ++m_skippedFrameCountByEngine[engineId];
            }
        }
        else
        {
            NX_VERBOSE(this, "Video frame not sent to DeviceAgent: see the log above.");
        }
    }
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

    const auto currentDeviceStatus = device->getStatus();
    const auto previousDeviceStatus = m_previousDeviceStatus;
    m_previousDeviceStatus = currentDeviceStatus;
    if (isAliveStatus(currentDeviceStatus) == isAliveStatus(previousDeviceStatus))
        return;

    at_deviceUpdated(resource);
}

void DeviceAnalyticsContext::at_deviceUpdated(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device, "Invalid Device"))
        return;

    const auto isAlive = isDeviceAlive();
    for (auto& entry: m_bindings)
    {
        auto engineId = entry.first;
        auto binding = entry.second;
        if (isAlive)
        {
            binding->restartAnalytics(
                prepareSettings(engineId, m_device->deviceAgentSettingsValues(engineId)));
        }
        else
        {
            binding->stopAnalytics();
        }

        updateSupportedFrameTypes();
    }
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

    for (auto& entry: m_bindings)
    {
        auto& binding = entry.second;
        binding->updateNeededMetadataTypes();
    }
}

void DeviceAnalyticsContext::subscribeToDeviceChanges()
{
    connect(
        m_device, &QnResource::statusChanged,
        this, &DeviceAnalyticsContext::at_deviceStatusChanged);

    connect(
        m_device, &QnResource::urlChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated);

    connect(
        m_device, &QnVirtualCameraResource::logicalIdChanged,
        this, &DeviceAnalyticsContext::at_deviceUpdated);

    connect(
        m_device, &QnResource::propertyChanged,
        this, &DeviceAnalyticsContext::at_devicePropertyChanged);
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

void DeviceAnalyticsContext::updateSupportedFrameTypes()
{
    AbstractVideoDataReceptor::NeededUncompressedPixelFormats neededUncompressedPixelFormats;
    bool needsCompressedFrames = false;
    for (auto& entry: m_bindings)
    {
        const QnUuid engineId = entry.first;

        auto binding = entry.second;
        if (!binding->isStreamConsumer())
            continue;

        bool gotPixelFormat = false;
        const auto pixelFormat = pixelFormatForEngine(
            serverModule(), engineId, binding, &gotPixelFormat);
        if (!gotPixelFormat)
            continue;
        if (!pixelFormat)
            needsCompressedFrames = true;
        else
            neededUncompressedPixelFormats.insert(*pixelFormat);
    }

    m_cachedNeedCompressedFrames = needsCompressedFrames;
    m_cachedUncompressedPixelFormats = std::move(neededUncompressedPixelFormats);
}

std::shared_ptr<DeviceAnalyticsBinding> DeviceAnalyticsContext::analyticsBinding(
    const QnUuid& engineId) const
{
    auto itr = m_bindings.find(engineId);
    if (itr == m_bindings.cend())
        return nullptr;

    return itr->second;
}

} // namespace nx::vms::server::analytics
