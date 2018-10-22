#include "device_analytics_context.h"

#include <core/resource/camera_resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>

#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/analytics/device_analytics_binding.h>
#include <nx/mediaserver/analytics/frame_converter.h>
#include <nx/mediaserver/analytics/data_packet_adapter.h>
#include <nx/mediaserver/sdk_support/utils.h>

#include <nx/utils/log/log.h>

namespace nx::mediaserver::analytics {

using namespace nx::mediaserver;

DeviceAnalyticsContext::DeviceAnalyticsContext(
    QnMediaServerModule* serverModule,
    const QnVirtualCameraResourcePtr& device)
    :
    base_type(serverModule),
    m_device(device)
{
    subscribeToDeviceChanges();
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

    for (const auto& engine: engines)
    {
        if (isEngineAlreadyBound(engine))
            continue;

        auto binding = std::make_shared<DeviceAnalyticsBinding>(serverModule(), m_device, engine);
        binding->setMetadataSink(m_metadataSink);
        m_bindings.emplace(engine->getId(), binding);
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

void DeviceAnalyticsContext::putFrame(
    const QnConstCompressedVideoDataPtr& compressedFrame,
    const CLConstVideoDecoderOutputPtr& uncompressedFrame)
{
    if (!NX_ASSERT(compressedFrame))
        return;

    NX_VERBOSE(this, "putVideoFrame(\"%1\", compressedFrame, %2)", m_device->getId(),
        uncompressedFrame ? "uncompressedFrame" : "/*uncompressedFrame*/ nullptr");

    FrameConverter frameConverter(
        [&]() { return compressedFrame; },
        [&, this]()
        {
            if (!uncompressedFrame)
                issueMissingUncompressedFrameWarningOnce();
            return uncompressedFrame;
        });

    for (auto& entry: m_bindings)
    {
        auto& binding = entry.second;
        if (!binding->isStreamConsumer())
            continue;

        const auto engineManifest = binding->engineManifest();
        if (!engineManifest)
        {
            NX_ERROR(this, "Unable to fetch engine manifest for device %1 (%2)",
                m_device->getUserDefinedName(), m_device->getId());
            continue;
        }

        if (nx::sdk::analytics::DataPacket* const dataPacket = frameConverter.getDataPacket(
            sdk_support::pixelFormatFromEngineManifest(*engineManifest)))
        {
            if (binding->canAcceptData()
                && NX_ASSERT(dataPacket->timestampUsec() >= 0))
            {
                binding->putData(std::make_shared<DataPacketAdapter>(dataPacket));
            }
            else
            {
                NX_INFO(this, "Skipped video frame for %1 from camera %2: queue overflow.",
                   engineManifest->pluginName.value, m_device->getId());
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

    if (propertyName == Qn::CAMERA_CREDENTIALS_PARAM_NAME)
    {
        NX_DEBUG(
            this,
            lm("Credentials have been changed for resource %1 (%2)")
                .args(device->getName(), device->getId()));

        at_deviceUpdated(device);
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
        auto binding = entry.second;
        if (!binding->isStreamConsumer())
            continue;

        auto engineManifest = binding->engineManifest();
        if (!engineManifest)
            continue;

        const auto pixelFormat = sdk_support::pixelFormatFromEngineManifest(*engineManifest);
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

void DeviceAnalyticsContext::issueMissingUncompressedFrameWarningOnce()
{
    auto logLevel = nx::utils::log::Level::verbose;
    if (!m_uncompressedFrameWarningIssued)
    {
        m_uncompressedFrameWarningIssued = true;
        logLevel = nx::utils::log::Level::warning;
    }
    NX_UTILS_LOG(logLevel, this, "Uncompressed frame requested but not received.");
}

} // namespace nx::mediaserver::analytics
