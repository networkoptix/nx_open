#include "manager.h"

#include <nx/kit/debug.h>
#include <nx/utils/file_system.h>

#include <common/common_module.h>
#include <media_server/media_server_module.h>

#include <plugins/plugin_manager.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/dataconsumer/abstract_data_receptor.h>

#include <nx/vms/server/analytics/metadata_handler.h>
#include <nx/vms/server/analytics/debug_helpers.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/sdk_support/utils.h>

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/debugging/visual_metadata_debugger_factory.h>
#include <nx/utils/log/log_main.h>
#include <nx/analytics/descriptor_manager.h>

#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/i_string_map.h>

namespace nx::vms::server::analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::debugging;
using nx::vms::server::resource::AnalyticsEngineResource;

Manager::Manager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    m_thread(new QThread(this)),
    m_visualMetadataDebugger(
        VisualMetadataDebuggerFactory::makeDebugger(DebuggerType::analyticsManager))
{
    m_thread->setObjectName(toString(this));
    moveToThread(m_thread);
    m_thread->start();
}

Manager::~Manager()
{
    NX_DEBUG(this, "Destroying");
    stop();
}

void Manager::stop()
{
    disconnect(this);
    m_thread->quit();
    m_thread->wait();
    m_deviceAnalyticsContexts.clear();
}

void Manager::init()
{
    NX_DEBUG(this, "Initializing");

    connect(
        resourcePool(), &QnResourcePool::resourceAdded,
        this, &Manager::at_resourceAdded);

    connect(
        resourcePool(), &QnResourcePool::resourceRemoved,
        this, &Manager::at_resourceRemoved);

    QMetaObject::invokeMethod(this, "initExistingResources");
}

void Manager::initExistingResources()
{
    const auto mediaServer = resourcePool()->getResourceById<QnMediaServerResource>(moduleGUID());
    const auto devices = resourcePool()->getAllCameras(mediaServer, /*ignoreDesktopCamera*/ true);
    for (const auto& device: devices)
        at_deviceAdded(device);

    const auto engines =
        resourcePool()->getResources<AnalyticsEngineResource>();
    for (const auto& engine: engines)
        at_engineAdded(engine);
}

QSharedPointer<DeviceAnalyticsContext> Manager::context(const QnUuid& deviceId) const
{
    auto contextItr = m_deviceAnalyticsContexts.find(deviceId);
    if (contextItr == m_deviceAnalyticsContexts.cend())
        return nullptr;

    return contextItr->second;
}

QSharedPointer<DeviceAnalyticsContext> Manager::context(
    const QnVirtualCameraResourcePtr& device) const
{
    return context(device->getId());
}

bool Manager::isLocalDevice(const QnVirtualCameraResourcePtr& device) const
{
    return device->getParentId() == moduleGUID();
}

void Manager::at_resourceAdded(const QnResourcePtr& resource)
{
    const auto analyticsEngine = resource.dynamicCast<AnalyticsEngineResource>();
    if (analyticsEngine)
    {
        NX_VERBOSE(
            this,
            lm("Analytics engine %1 (%2) has been added.")
               .args(analyticsEngine->getName(), analyticsEngine->getId()));

        at_engineAdded(analyticsEngine);
        return;
    }

    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (device)
    {
        NX_VERBOSE(
            this,
            lm("Device %1 (%2) has been added.")
            .args(resource->getName(), resource->getId()));

        at_deviceAdded(device);
    }
}

void Manager::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (!resource)
    {
        NX_VERBOSE(this, "Receieved null resource.");
        return;
    }

    NX_VERBOSE(this, "Resource %1 (%2) has been removed.", resource->getName(), resource->getId());

    resource->disconnect(this);

    const auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (device)
    {
        NX_VERBOSE(this, "Device %1 (%2) has been removed",
            device->getUserDefinedName(), device->getId());
        at_deviceRemoved(device);
        return;
    }

    const auto engine = resource.dynamicCast<AnalyticsEngineResource>();
    if (!engine)
    {
        NX_VERBOSE(this, "Resource %1 (%2) is neither analytics engine nor device. Skipping.",
            resource->getName(), resource->getId());
        return;
    }

    NX_VERBOSE(this, "Engine %1 (%2) has been removed.", engine->getName(), engine->getId());
    at_engineRemoved(engine);
}

void Manager::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device))
        return;

    at_deviceParentIdChanged(device);
}

void Manager::at_resourcePropertyChanged(const QnResourcePtr& resource, const QString& propertyName)
{
    auto engine = resource.dynamicCast<nx::vms::server::resource::AnalyticsEngineResource>();
    if (!NX_ASSERT(engine))
        return;

    at_enginePropertyChanged(engine, propertyName);
}

void Manager::at_deviceAdded(const QnVirtualCameraResourcePtr& device)
{
    connect(
        device, &QnResource::parentIdChanged,
        this, &Manager::at_resourceParentIdChanged);

    connect(
        device, &QnResource::statusChanged,
        this, &Manager::at_deviceStatusChanged);

    updateCompatibilityWithEngines(device);
    if (isLocalDevice(device))
        handleDeviceArrivalToServer(device);
}

void Manager::at_deviceRemoved(const QnVirtualCameraResourcePtr& device)
{
    removeDeviceDescriptor(device);
    handleDeviceRemovalFromServer(device);
}

void Manager::at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device)
{
    if (isLocalDevice(device))
    {
        NX_VERBOSE(this, "Device %1 (%2) has been moved to the current server",
            device->getUserDefinedName(), device->getId());
        handleDeviceArrivalToServer(device);
    }
    else
    {
        NX_VERBOSE(this, "Device %1 (%2) has been moved to another server",
            device->getUserDefinedName(), device->getId());
        handleDeviceRemovalFromServer(device);
    }
}

void Manager::at_deviceEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& device)
{
    auto analyticsContext = context(device);
    if (!analyticsContext)
    {
        NX_DEBUG(this, "Can't find analytics context for device %1 (%2)",
            device->getUserDefinedName(), device->getId());
        return;
    }

    analyticsContext->setEnabledAnalyticsEngines(
        sdk_support::toServerEngineList(device->enabledAnalyticsEngineResources()));
}

void Manager::at_deviceStatusChanged(const QnResourcePtr& deviceResource)
{
    const auto device = deviceResource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device))
        return;

    if (device->isOnline())
        updateCompatibilityWithEngines(device);
}

void Manager::handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device)
{
    connect(
        device, &QnVirtualCameraResource::enabledAnalyticsEnginesChanged,
        this, &Manager::at_deviceEnabledAnalyticsEnginesChanged);

    auto context = QSharedPointer<DeviceAnalyticsContext>::create(serverModule(), device);
    context->setEnabledAnalyticsEngines(
        sdk_support::toServerEngineList(device->enabledAnalyticsEngineResources()));
    context->setMetadataSink(metadataSink(device));

    if (auto source = mediaSource(device).toStrongRef())
        source->setProxiedReceptor(context);

    m_deviceAnalyticsContexts.emplace(device->getId(), context);
}

void Manager::handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device)
{
    m_deviceAnalyticsContexts.erase(device->getId());
}

void Manager::at_engineAdded(const AnalyticsEngineResourcePtr& engine)
{
    connect(
        engine, &QnResource::propertyChanged,
        this, &Manager::at_resourcePropertyChanged);

    connect(
        engine, &AnalyticsEngineResource::engineInitialized,
        this, &Manager::at_engineInitializationStateChanged);

    updateCompatibilityWithDevices(engine);
}

void Manager::at_engineRemoved(const AnalyticsEngineResourcePtr& engine)
{
    NX_VERBOSE(
        this,
        lm("Engine %1 (%2) has been removed.").args(engine->getName(), engine->getId()));

    engine->disconnect(this);
    for (auto& entry: m_deviceAnalyticsContexts)
    {
        auto& context = entry.second;
        context->removeEngine(engine);
    }
}

void Manager::at_enginePropertyChanged(
    const AnalyticsEngineResourcePtr& engine,
    const QString& propertyName)
{
    if (!NX_ASSERT(engine))
        return;

    if (propertyName == AnalyticsEngineResource::kSettingsValuesProperty)
        engine->sendSettingsToSdkEngine();
}

void Manager::at_engineInitializationStateChanged(const AnalyticsEngineResourcePtr& engine)
{
    updateCompatibilityWithDevices(engine);
}

void Manager::registerMetadataSink(
    const QnResourcePtr& deviceResource,
    QWeakPointer<QnAbstractDataReceptor> metadataSink)
{
    auto device = deviceResource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ERROR(this, "Can't register metadata sink for resource %1 (%2)",
            deviceResource->getName(), deviceResource->getId());
        return;
    }

    m_metadataSinks[deviceResource->getId()] = metadataSink;
    auto analyticsContext = context(device);
    if (analyticsContext)
        analyticsContext->setMetadataSink(metadataSink);
}

QWeakPointer<AbstractVideoDataReceptor> Manager::registerMediaSource(const QnUuid& deviceId)
{
    auto proxySource = QSharedPointer<ProxyVideoDataReceptor>::create();
    auto analyticsContext = context(deviceId);

    if (analyticsContext)
        proxySource->setProxiedReceptor(analyticsContext);

    m_mediaSources[deviceId] = proxySource;
    return proxySource;
}

void Manager::setSettings(
    const QString& deviceId,
    const QString& engineId,
    const QVariantMap& deviceAgentSettings)
{
    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    {
        QnMutexLocker lock(&m_contextMutex);
        analyticsContext = context(QnUuid(deviceId));
    }

    if (!NX_ASSERT(analyticsContext, lm("Device %1").arg(deviceId)))
        return;

    return analyticsContext->setSettings(engineId, deviceAgentSettings);
}

QVariantMap Manager::getSettings(const QString& deviceId, const QString& engineId) const
{
    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    {
        QnMutexLocker lock(&m_contextMutex);
        analyticsContext = context(QnUuid(deviceId));
    }

    if (!analyticsContext)
        return QVariantMap();

    return analyticsContext->getSettings(engineId);
}

void Manager::setSettings(const QString& engineId, const QVariantMap& engineSettings)
{
    auto engine = sdk_support::find<AnalyticsEngineResource>(serverModule(), engineId);

    if (!NX_ASSERT(engine, lm("Engine %1").arg(engineId)))
        return;

    NX_DEBUG(this, "Setting settings for engine %1 (%2)", engine->getName(), engine->getId());
    engine->setSettingsValues(engineSettings);
}

QVariantMap Manager::getSettings(const QString& engineId) const
{
    auto engine = sdk_support::find<AnalyticsEngineResource>(serverModule(), engineId);

    if (!NX_ASSERT(engine, lm("Engine %1").arg(engineId)))
        return QVariantMap();

    NX_DEBUG(this, "Getting settings for engine %1 (%2)", engine->getName(), engine->getId());
    return engine->settingsValues();
}

QWeakPointer<QnAbstractDataReceptor> Manager::metadataSink(
    const QnVirtualCameraResourcePtr& device) const
{
    return metadataSink(device->getId());
}

QWeakPointer<QnAbstractDataReceptor> Manager::metadataSink(const QnUuid& deviceId) const
{
    auto it = m_metadataSinks.find(deviceId);
    if (it == m_metadataSinks.cend())
        return QWeakPointer<QnAbstractDataReceptor>();

    return it->second;
}

QWeakPointer<ProxyVideoDataReceptor> Manager::mediaSource(
    const QnVirtualCameraResourcePtr& device) const
{
    return mediaSource(device->getId());
}

QWeakPointer<ProxyVideoDataReceptor> Manager::mediaSource(const QnUuid& deviceId) const
{
    auto it = m_mediaSources.find(deviceId);
    if (it == m_mediaSources.cend())
        return QWeakPointer<ProxyVideoDataReceptor>();

    return it->second;
}

nx::vms::server::resource::AnalyticsEngineResourceList Manager::localEngines() const
{
    using namespace nx::vms::server::resource;
    return resourcePool()->getResources<AnalyticsEngineResource>(
            [](const AnalyticsEngineResourcePtr& engine) { return engine->sdkEngine(); });
}

std::set<QnUuid> Manager::compatibleEngineIds(const QnVirtualCameraResourcePtr& device) const
{
    std::set<QnUuid> result;
    if (!NX_ASSERT(device))
        return result;

    for (auto& engine: localEngines())
    {
        auto sdkEngine = engine->sdkEngine();
        if (!NX_ASSERT(sdkEngine))
            continue;

        auto deviceInfo = sdk_support::deviceInfoFromResource(device);
        if (!deviceInfo)
        {
            NX_WARNING(this, "Unable to build device info for device %1 (%2)",
                device->getUserDefinedName(), device->getId());

            continue;
        }

        if (sdkEngine->isCompatible(deviceInfo.get()))
            result.insert(engine->getId());
    }

    return result;
}

void Manager::updateCompatibilityWithEngines(const QnVirtualCameraResourcePtr& device)
{
    nx::analytics::DeviceDescriptorManager descriptorManager(serverModule()->commonModule());
    descriptorManager.setCompatibleAnalyticsEngines(device->getId(), compatibleEngineIds(device));
}

void Manager::updateCompatibilityWithDevices(const AnalyticsEngineResourcePtr& engine)
{
    const auto sdkEngine = engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_DEBUG(this, "Engine resource %1 (%2) has no assigned SDK engine",
            engine->getName(), engine->getId());
        return;
    }

    nx::analytics::DeviceDescriptorManager descriptorManager(serverModule()->commonModule());
    auto devices = resourcePool()->getAllCameras(
        /*any server*/ QnResourcePtr(),
        /*ignoreDesktopCamera*/ true);

    for (auto& device: devices)
    {
        auto deviceInfo = sdk_support::deviceInfoFromResource(device);
        if (!deviceInfo)
        {
            NX_WARNING(this, "Unable to build device info for device %1 (%2)",
                device->getUserDefinedName(), device->getId());
            continue;
        }

        if (sdkEngine->isCompatible(deviceInfo.get()))
            descriptorManager.addCompatibleAnalyticsEngines(device->getId(), {engine->getId()});
    }
}

void Manager::removeDeviceDescriptor(const QnVirtualCameraResourcePtr& device) const
{
    nx::analytics::DeviceDescriptorManager descriptorManager(serverModule()->commonModule());
    descriptorManager.removeDeviceDescriptors({device->getId()});
}

void Manager::removeEngineFromCompatible(const AnalyticsEngineResourcePtr& engine) const
{
    nx::analytics::DeviceDescriptorManager descriptorManager(serverModule()->commonModule());
    descriptorManager.removeCompatibleAnalyticsEngines({engine->getId()});
}

} // namespace nx::vms::server::analytics
