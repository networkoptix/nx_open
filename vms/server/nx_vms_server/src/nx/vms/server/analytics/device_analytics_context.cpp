﻿#include "device_analytics_context.h"

#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <plugins/vms_server_plugins_ini.h>
#include <utils/common/synctime.h>

#include <nx/utils/log/log.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/vms/event/events/plugin_diagnostic_event.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/camera.h>

#include <nx/vms/server/analytics/device_analytics_binding.h>
#include <nx/vms/server/analytics/data_converter.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/analytics/stream_converter.h>
#include <nx/vms/server/analytics/wrappers/sdk_object_description.h>

#include <nx/vms/server/sdk_support/conversion_utils.h>
#include <nx/vms/server/sdk_support/utils.h>

#include <nx/vms/server/event/event_connector.h>

#include <nx/vms/server/analytics/wrappers/plugin_diagnostic_message_builder.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::server;

using StreamType = nx::vms::api::analytics::StreamType;
using StreamTypes = nx::vms::api::analytics::StreamTypes;
using StreamIndex = nx::vms::api::StreamIndex;

static const std::chrono::seconds kMinSkippedFramesReportingInterval(30);

static bool isAliveStatus(Qn::ResourceStatus status)
{
    return status == Qn::ResourceStatus::Online || status == Qn::ResourceStatus::Recording;
}

DeviceAnalyticsContext::DeviceAnalyticsContext(
    QnMediaServerModule* serverModule,
    const resource::CameraPtr& device)
    :
    base_type(serverModule),
    m_device(device),
    m_reportSkippedFrames(
        [this](int framesSkipped, QnUuid engineId)
        {
            reportSkippedFrames(framesSkipped, engineId);
        },
        kMinSkippedFramesReportingInterval),
    m_streamConverter(std::make_unique<StreamConverter>())
{
    connect(this, &DeviceAnalyticsContext::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);

    subscribeToDeviceChanges();
    subscribeToRulesChanges();
}

DeviceAnalyticsContext::~DeviceAnalyticsContext()
{
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
            binding->setMetadataSinks(m_metadataSinks);
            m_bindings.emplace(engine->getId(), binding);
        }

        if (deviceIsAlive)
            binding->startAnalytics();
    }

    updateStreamProviderRequirements();
}

void DeviceAnalyticsContext::removeEngine(const resource::AnalyticsEngineResourcePtr& engine)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_bindings.erase(engine->getId());
    }

    updateStreamProviderRequirements();
}

void DeviceAnalyticsContext::setMetadataSinks(MetadataSinkSet metadataSinks)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_metadataSinks = std::move(metadataSinks);

    for (auto& [_, binding]: m_bindings)
        binding->setMetadataSinks(m_metadataSinks);
}

SettingsResponse DeviceAnalyticsContext::setSettings(
    const QString& engineId, const SetSettingsRequest& settings)
{
    const std::shared_ptr<DeviceAnalyticsBinding> binding =
        analyticsBindingSafe(QnUuid(engineId));

    if (!binding)
    {
        NX_DEBUG(this,
            "Setting DeviceAgent settings: DeviceAgent is not available for the "
            "Device %1 and the Engine %2",
            m_device, engineId);

        return SettingsResponse(
            SettingsResponse::Error::Code::sdkObjectIsNotAccessible,
            "Unable to find DeviceAgent");
    }

    return binding->setSettings(settings);
}

SettingsResponse DeviceAnalyticsContext::getSettings(const QString& engineId) const
{
    const std::shared_ptr<DeviceAnalyticsBinding> binding =
        analyticsBindingSafe(QnUuid(engineId));

    if (!binding)
    {
        NX_DEBUG(this,
            "Getting DeviceAgent settings: DeviceAgent is not available for the "
            "Device %1 and the Engine %2",
            m_device, engineId);

        return SettingsResponse(
            SettingsResponse::Error::Code::sdkObjectIsNotAccessible,
            "Unable to find DeviceAgent");
    }

    return binding->getSettings();
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

void DeviceAnalyticsContext::putData(const QnAbstractDataPacketPtr& data)
{
    NX_VERBOSE(this, "Processing data, timestamp %1 us", data->timestamp);

    if (!NX_ASSERT(data))
        return;

    m_streamConverter->updateRotation(
        m_device->getProperty(QnMediaResource::rotationKey()).toInt());

    if (!m_streamConverter->pushData(data))
    {
        NX_VERBOSE(this,
            "Stream converter won't be able to produce data from the current packet,"
            "no additional processing will be done");
        return;
    }

    struct DataConversionContext
    {
        StreamRequirements requirements;
        QnAbstractDataPacketPtr convertedData;
    };

    std::map</*engineId*/ QnUuid, DataConversionContext> dataConversionContextByEngineId;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for (const auto& [engineId, binding]: m_bindings)
        {
            if (m_skippedPacketCountByEngine[engineId] > 0)
            {
                // Report skipped frames.
                if (m_reportSkippedFrames(m_skippedPacketCountByEngine[engineId], engineId))
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

    for (const auto& [engineId, context]: dataConversionContextByEngineId)
    {
        dataConversionContextByEngineId[engineId].convertedData =
            m_streamConverter->getData(context.requirements);
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

StreamProviderRequirements DeviceAnalyticsContext::providerRequirements(
    StreamIndex streamIndex) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (const auto it = m_cachedStreamProviderRequirements.find(streamIndex);
        it != m_cachedStreamProviderRequirements.cend())
    {
        return it->second;
    }

    return {};
}

void DeviceAnalyticsContext::registerStream(nx::vms::api::StreamIndex streamIndex)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_registeredStreamIndexes.insert(streamIndex);
    }

    updateStreamProviderRequirements();
}

void DeviceAnalyticsContext::reportSkippedFrames(int framesSkipped, QnUuid engineId) const
{
    const std::shared_ptr<DeviceAnalyticsBinding> binding =
        analyticsBindingUnsafe(engineId);

    if (!NX_ASSERT(binding,
        "Unable to report %1 skipped frames for Device %2 %3: Binding not found.",
        framesSkipped, m_device->getUserDefinedName(), m_device->getId()))
    {
        return;
    }

    const wrappers::PluginDiagnosticMessageBuilder messageBuilder(
        wrappers::SdkObjectDescription(
            binding->engine()->plugin().dynamicCast<resource::AnalyticsPluginResource>(),
            binding->engine(),
            m_device),
        "Plugin is processing incoming frames too slowly.",
        NX_FMT("Frame queue overflow, skipped %1 frame(s)", framesSkipped));

    NX_INFO(this, messageBuilder.buildLogString());

    const nx::vms::event::PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new nx::vms::event::PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            engineId,
            messageBuilder.buildPluginDiagnosticEventCaption(),
            messageBuilder.buildPluginDiagnosticEventDescription(),
            nx::vms::api::EventLevel::WarningEventLevel,
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

void DeviceAnalyticsContext::at_deviceStatusChanged(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device, "Invalid Device"))
        return;

    NX_DEBUG(this, "Received \"Device status changed\" signal, Device: %1", device);

    const Qn::ResourceStatus currentDeviceStatus = device->getStatus();
    const Qn::ResourceStatus previousDeviceStatus =
        m_previousDeviceStatus.exchange(currentDeviceStatus);

    if (isAliveStatus(currentDeviceStatus) == isAliveStatus(previousDeviceStatus))
        return;

    NX_DEBUG(this,
        "Device status changed, Device: %1, previous status: %2, current Device status: %3",
        device, previousDeviceStatus, currentDeviceStatus);

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
            NX_DEBUG(this, "Restarting Analytics, Device %1, Engine id: %2", device, engineId);
            binding->restartAnalytics();
        }
        else
        {
            NX_DEBUG(this, "Stopping Analytics, Device %1, Engine id: %3", device, engineId);
            binding->stopAnalytics();
        }
    }

    updateStreamProviderRequirements();
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
                .args(device->getUserDefinedName(), device->getId()));

        at_deviceUpdated(device);
    }

    if (propertyName == QnVirtualCameraResource::kAnalyzedStreamIndexes)
    {
        NX_DEBUG(this, "Analyzed stream indexes have been changed, Device: %1 (%2)",
            device->getUserDefinedName(), device->getId());

        updateAnalyzedStreamIndexes();
    }
}

void DeviceAnalyticsContext::at_rulesUpdated()
{
    NX_DEBUG(this, "Rules have been updated, Device: %1 (%2)",
        m_device->getUserDefinedName(), m_device->getId());
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
        ruleWatcher, &EventRuleWatcher::watchedEventTypesChanged,
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

void DeviceAnalyticsContext::updateAnalyzedStreamIndexes()
{
    const BindingMap bindings = analyticsBindingsSafe();

    for (const auto& [engineId, binding]: bindings)
        binding->recalculateStreamRequirements();

    updateStreamProviderRequirements();
}

void DeviceAnalyticsContext::updateStreamProviderRequirements()
{
    StreamProviderRequirementsMap providerRequirementsMap = getTotalProviderRequirements();
    providerRequirementsMap = setUpMotionAnalysisPolicy(std::move(providerRequirementsMap));

    NX_MUTEX_LOCKER lock(&m_mutex);
    providerRequirementsMap = adjustProviderRequirementsByProviderCountUnsafe(
        std::move(providerRequirementsMap));

    m_cachedStreamProviderRequirements = providerRequirementsMap;
}

DeviceAnalyticsContext::StreamProviderRequirementsMap
    DeviceAnalyticsContext::getTotalProviderRequirements() const
{
    BindingMap bindings = analyticsBindingsSafe();
    StreamProviderRequirementsMap providerRequirementsMap;
    for (const auto& [engineId, binding]: bindings)
    {
        if (!binding->isStreamConsumer())
            continue;

        const std::optional<StreamRequirements> requirements = binding->streamRequirements();
        if (!requirements)
            continue;

        StreamIndex preferredStreamIndex = requirements->preferredStreamIndex;
        if (preferredStreamIndex == StreamIndex::undefined)
            preferredStreamIndex = QnVirtualCameraResource::kDefaultAnalyzedStreamIndex;

        providerRequirementsMap[preferredStreamIndex].requiredStreamTypes |=
            requirements->requiredStreamTypes;
    }

    return providerRequirementsMap;
}

DeviceAnalyticsContext::StreamProviderRequirementsMap
    DeviceAnalyticsContext::setUpMotionAnalysisPolicy(
        StreamProviderRequirementsMap requirements) const
{
    const bool needMotion =
        requirements[StreamIndex::primary].requiredStreamTypes.testFlag(StreamType::motion)
        || requirements[StreamIndex::secondary].requiredStreamTypes.testFlag(StreamType::motion);

    if (!needMotion)
    {
        for (const StreamIndex streamIndex: {StreamIndex::primary, StreamIndex::secondary})
            requirements[streamIndex].motionAnalysisPolicy = MotionAnalysisPolicy::automatic;

        return requirements;
    }

    const auto setMotionStream =
        [](StreamIndex streamIndex, StreamProviderRequirementsMap requirements)
        {
            const StreamIndex oppositeStreamIndex = nx::vms::api::oppositeStreamIndex(streamIndex);

            requirements[streamIndex].motionAnalysisPolicy = MotionAnalysisPolicy::forced;
            requirements[oppositeStreamIndex].motionAnalysisPolicy =
                MotionAnalysisPolicy::disabled;

            requirements[streamIndex].requiredStreamTypes.setFlag(StreamType::motion, true);
            requirements[oppositeStreamIndex].requiredStreamTypes.setFlag(
                StreamType::motion, false);

            return requirements;
        };

    const bool needUncompressedFramesFromPrimaryStream =
        requirements[StreamIndex::primary].requiredStreamTypes.testFlag(
            StreamType::uncompressedVideo);

    const bool needUncompressedFramesFromSecondaryStream =
        requirements[StreamIndex::secondary].requiredStreamTypes.testFlag(
            StreamType::uncompressedVideo);

    if (!needUncompressedFramesFromPrimaryStream)
        requirements = setMotionStream(StreamIndex::secondary, std::move(requirements));
    else if (!needUncompressedFramesFromSecondaryStream)
        requirements = setMotionStream(StreamIndex::primary, std::move(requirements));

    return requirements;
}

DeviceAnalyticsContext::StreamProviderRequirementsMap
    DeviceAnalyticsContext::adjustProviderRequirementsByProviderCountUnsafe(
        StreamProviderRequirementsMap requirements) const
{
    if (m_registeredStreamIndexes.size() != 1)
        return requirements;

    const StreamIndex registeredStreamIndex = *m_registeredStreamIndexes.begin();
    const StreamIndex oppositeStreamIndex =
        nx::vms::api::oppositeStreamIndex(registeredStreamIndex);

    requirements[registeredStreamIndex].requiredStreamTypes |=
        requirements[oppositeStreamIndex].requiredStreamTypes;

    requirements[registeredStreamIndex].motionAnalysisPolicy =
        requirements[registeredStreamIndex].requiredStreamTypes.testFlag(StreamType::motion)
            ? MotionAnalysisPolicy::forced
            : MotionAnalysisPolicy::automatic;

    requirements[oppositeStreamIndex] = StreamProviderRequirements{};

    return requirements;
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
