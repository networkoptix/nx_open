#include "device_analytics_context.h"

#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <plugins/plugins_ini.h>

#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/analytics/device_analytics_binding.h>
#include <nx/vms/server/analytics/frame_converter.h>
#include <nx/vms/server/analytics/data_packet_adapter.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/analytics/event_rule_watcher.h>
#include <nx/vms/server/sdk_support/utils.h>

#include <nx/utils/log/log.h>

namespace nx::vms::server::analytics {

using namespace nx::vms::server;

DeviceAnalyticsContext::DeviceAnalyticsContext(
    QnMediaServerModule* serverModule,
    const QnVirtualCameraResourcePtr& device)
    :
    base_type(serverModule),
    m_device(device)
{
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
            binding->startAnalytics(m_device->deviceAgentSettingsValues(engine->getId()));
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
    std::shared_ptr<DeviceAnalyticsBinding> binding;
    {
        QnMutexLocker lock(&m_mutex);
        binding = analyticsBinding(QnUuid(engineId));
    }

    if (!binding)
        return;

    binding->setSettings(settings);
}

QVariantMap DeviceAnalyticsContext::getSettings(const QString& engineId) const
{
    std::shared_ptr<DeviceAnalyticsBinding> binding;
    {
        QnMutexLocker lock(&m_mutex);
        binding = analyticsBinding(QnUuid(engineId));
    }

    if (!binding)
        return QVariantMap();

    return binding->getSettings();
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
    if (!NX_ASSERT(engineResource)
        || !NX_ASSERT(engineResource->sdkEngine()))
    {
        return "unknown Analytics Engine";
    }

    if (!NX_ASSERT(engineResource->sdkEngine()->plugin())
        || !NX_ASSERT(engineResource->sdkEngine()->plugin()->name()))
    {
        return lm("Analytics Engine %1 of unknown Plugin").arg(engineResource->getName());
    }

    return lm("Analytics Engine %1 of Plugin %2").args(
        engineResource->getName(), engineResource->plugin()->manifest().id);
}

static std::optional<nx::sdk::analytics::IUncompressedVideoFrame::PixelFormat> pixelFormatForEngine(
    QnMediaServerModule* serverModule,
    QnUuid engineId,
    const std::shared_ptr<nx::vms::server::analytics::DeviceAnalyticsBinding>& binding,
    bool* outSuccess)
{
    *outSuccess = false;
    const QString engineLogLabel = getEngineLogLabel(serverModule, engineId);
    const auto engineManifest = binding->engineManifest();
    if (!NX_ASSERT(engineManifest, lm("Engine %1").arg(engineLogLabel)))
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
                binding->putData(std::make_shared<DataPacketAdapter>(dataPacket.get()));
            }
            else
            {
                NX_INFO(this, "Skipped video frame for %1 from camera %2: queue overflow.",
                    getEngineLogLabel(serverModule(), engineId), m_device->getId());
            }
        }
        else
        {
            NX_VERBOSE(this, "Video frame not sent to DeviceAgent: see the log above.");
        }
    }
}

void DeviceAnalyticsContext::at_deviceUpdated(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ASSERT(device, "Invalid device");
        return;
    }

    const auto isAlive = isDeviceAlive();
    for (auto& entry: m_bindings)
    {
        auto engineId = entry.first;
        auto binding = entry.second;
        if (isAlive)
            binding->restartAnalytics(m_device->deviceAgentSettingsValues(engineId));
        else
            binding->stopAnalytics();

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
        NX_ASSERT(false, "Invalid device");
        return;
    }

    if (propertyName == ResourcePropertyKey::kCredentials)
    {
        NX_DEBUG(
            this,
            lm("Credentials have been changed for resource %1 (%2)")
                .args(device->getName(), device->getId()));

        at_deviceUpdated(device);
    }
}

void DeviceAnalyticsContext::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
    if (!affectedResources.contains(m_device->getId()))
        return;

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
        this, &DeviceAnalyticsContext::at_deviceUpdated);

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
    return m_device->getStatus() >= Qn::Online
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
