#include "manager.h"

#include <nx/kit/debug.h>
#include <nx/utils/file_system.h>

#include <common/common_module.h>
#include <media_server/media_server_module.h>

#include <plugins/plugin_manager.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/dataconsumer/abstract_data_receptor.h>

#include <nx/vms/server/analytics/metadata_handler.h>
#include <nx/vms/server/analytics/wrappers/engine.h>
#include <nx/vms/server/analytics/device_analytics_context.h>
#include <nx/vms/server/analytics/proxy_stream_data_receptor.h>
#include <nx/vms/server/metrics/common_plugin_resource_binding_info_holder.h>

#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/camera.h>
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
using namespace nx::vms::server::metrics;

static bool isEngineCompatibleWithDevice(
    const wrappers::EnginePtr& engine,
    const QnVirtualCameraResourcePtr& device)
{
    if (!NX_ASSERT(engine))
        return false;

    if (!NX_ASSERT(device))
        return false;

    if (device->hasCameraCapabilities(Qn::CameraCapability::noAnalytics))
        return false;

    return engine->isCompatible(device);
}

Manager::Manager(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    m_thread(new QThread(this))
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
    NX_INFO(this, "Initializing");
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
    NX_INFO(this, "Initializing existing resources");
    const auto mediaServer = resourcePool()->getResourceById<QnMediaServerResource>(moduleGUID());
    const auto devices = resourcePool()->getAllCameras(
        /*any server*/QnResourcePtr(),
        /*ignoreDesktopCamera*/ true);

    for (const auto& device: devices)
    {
        NX_DEBUG(this, "Initializing existing device on the server startup: %1 (%2)",
            device->getUserDefinedName(), device->getId());
        at_deviceAdded(device);
    }

    const auto engines = resourcePool()->getResources<AnalyticsEngineResource>();
    for (const auto& engine: engines)
    {
        if (!engine)
        {
            NX_WARNING(this, "Engine is null");
            continue;
        }

        NX_DEBUG(this, "Initializing existing engine on the server startup: %1 (%2)",
            engine->getName(), engine->getId());

        at_engineAdded(engine);
    }
}

QSharedPointer<DeviceAnalyticsContext> Manager::deviceAnalyticsContextUnsafe(
    const QnUuid& deviceId) const
{
    auto contextItr = m_deviceAnalyticsContexts.find(deviceId);
    if (contextItr == m_deviceAnalyticsContexts.cend())
        return nullptr;

    return contextItr->second;
}

QSharedPointer<DeviceAnalyticsContext> Manager::deviceAnalyticsContextUnsafe(
    const QnVirtualCameraResourcePtr& device) const
{
    return deviceAnalyticsContextUnsafe(device->getId());
}

void Manager::at_resourceAdded(const QnResourcePtr& resource)
{
    const auto analyticsEngine = resource.dynamicCast<AnalyticsEngineResource>();
    if (analyticsEngine)
    {
        NX_DEBUG(
            this,
            lm("The Analytics Engine %1 (%2) has been added.")
               .args(analyticsEngine->getName(), analyticsEngine->getId()));

        at_engineAdded(analyticsEngine);
        return;
    }

    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (device && !device->hasFlags(Qn::desktop_camera))
    {
        NX_DEBUG(
            this,
            lm("The Device %1 (%2) has been added.").args(resource->getName(), resource->getId()));

        at_deviceAdded(device);
    }
}

void Manager::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (!resource)
    {
        NX_DEBUG(this, "Receieved a null Resource.");
        return;
    }

    NX_DEBUG(this, "The Resource %1 (%2) has been removed.",
        resource->getName(), resource->getId());

    resource->disconnect(this);

    const auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (device)
    {
        NX_DEBUG(this, "The Device %1 (%2) has been removed",
            device->getUserDefinedName(), device->getId());
        at_deviceRemoved(device);
        return;
    }

    const auto engine = resource.dynamicCast<AnalyticsEngineResource>();
    if (!engine)
    {
        NX_DEBUG(this,
            "The resource %1 (%2) is neither an Analytics Engine nor a Device. Skipping",
            resource->getName(), resource->getId());
        return;
    }

    NX_DEBUG(this, "Engine %1 (%2) has been removed", engine->getName(), engine->getId());
    at_engineRemoved(engine);
}

void Manager::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device))
        return;

    at_deviceParentIdChanged(device);
}

void Manager::at_resourcePropertyChanged(
    const QnResourcePtr& resource,
    const QString& propertyName)
{
    auto engine = resource.dynamicCast<nx::vms::server::resource::AnalyticsEngineResource>();
    if (!NX_ASSERT(engine))
        return;

    at_enginePropertyChanged(engine, propertyName);
}

void Manager::at_deviceAdded(const QnVirtualCameraResourcePtr& device)
{
    NX_DEBUG(this, "Handling new device, %1 (%2)", device->getUserDefinedName(), device->getId());
    connect(
        device, &QnResource::parentIdChanged,
        this, &Manager::at_resourceParentIdChanged);

    connect(
        device, &QnResource::statusChanged,
        this, &Manager::at_deviceStatusChanged);

    if (isLocalDevice(device))
        handleDeviceArrivalToServer(device);
}

void Manager::at_deviceRemoved(const QnVirtualCameraResourcePtr& device)
{
    NX_DEBUG(this, "Handling device removal, %1 (%2)",
        device->getUserDefinedName(), device->getId());

    handleDeviceRemovalFromServer(device);
}

void Manager::at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device)
{
    if (isLocalDevice(device))
    {
        NX_DEBUG(this, "The Device %1 (%2) has been moved to the current Server",
            device->getUserDefinedName(), device->getId());
        updateCompatibilityWithEngines(device);
        handleDeviceArrivalToServer(device);
    }
    else
    {
        NX_DEBUG(this, "The Device %1 (%2) has been moved to another Server",
            device->getUserDefinedName(), device->getId());
        handleDeviceRemovalFromServer(device);
    }
}

void Manager::at_deviceUserEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& device)
{
    updateEnabledAnalyticsEngines(device);
}

void Manager::at_deviceStatusChanged(const QnResourcePtr& deviceResource)
{
    const auto device = deviceResource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device))
        return;

    if (device->isOnline())
    {
        updateCompatibilityWithEngines(device);
        updateEnabledAnalyticsEngines(device);
    }
}

void Manager::handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device)
{
    auto camera = device.objectCast<resource::Camera>();
    if (!NX_ASSERT(camera))
        return;

    updateCompatibilityWithEngines(camera);
    connect(
        camera, &QnVirtualCameraResource::userEnabledAnalyticsEnginesChanged,
        this, &Manager::at_deviceUserEnabledAnalyticsEnginesChanged);

    auto context = QSharedPointer<DeviceAnalyticsContext>::create(serverModule(), camera);
    context->setEnabledAnalyticsEngines(
        camera->enabledAnalyticsEngineResources().filtered<resource::AnalyticsEngineResource>());
    context->setMetadataSinks(metadataSinksUnsafe(camera->getId()));

    if (auto source = mediaSourceUnsafe(camera->getId()).toStrongRef())
        source->setProxiedReceptor(context);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_deviceAnalyticsContexts.emplace(camera->getId(), context);
}

void Manager::handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    const QnUuid deviceId = device->getId();

    m_deviceAnalyticsContexts.erase(deviceId);
    m_metadataSinks.erase(deviceId);
    m_mediaSources.erase(deviceId);
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
    for (const auto& device: localDevices())
        updateEnabledAnalyticsEngines(device);
}

void Manager::at_engineRemoved(const AnalyticsEngineResourcePtr& engine)
{
    NX_VERBOSE(
        this,
        lm("The Engine %1 (%2) has been removed").args(engine->getName(), engine->getId()));

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

std::unique_ptr<metrics::PluginResourceBindingInfoHolder> Manager::bindingInfoHolder() const
{
    NX_DEBUG(this, "Collecting Engine binding info");
    metrics::CommonPluginResourceBindingInfoHolder::EngineBindingInfoMap engineBindingInfoMap;

    for (const resource::AnalyticsEngineResourcePtr& engine: localEngines())
    {
        engineBindingInfoMap[engine].id = engine->getId().toString();
        engineBindingInfoMap[engine].name = engine->getName();
    }

    for (const auto& [deviceId, context]: m_deviceAnalyticsContexts)
    {
        const auto device = resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);
        if (!device || !isLocalDevice(device))
        {
            NX_DEBUG(this,
                "Unable to find the Device with id %1 (but its Analytics context exists)",
                deviceId);
            continue;
        }

        if (!isLocalDevice(device))
        {
            NX_DEBUG(this,
                "Skipping Device %1 (%2) because it is bound to another Server "
                "(but its analytics context exists on the current one)",
                device->getUserDefinedName(), device->getId());
        }

        NX_VERBOSE(this, "Collecting Engine binding info for Device %1 (%2)",
            device->getUserDefinedName(), device->getId());

        for (const auto& [engineId, hasAliveDeviceAgent]: context->bindingStatuses())
        {
            const auto engineResource =
                resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);

            if (!NX_ASSERT(engineResource))
                continue;

            NX_VERBOSE(this,
                "Device %1 (%2) is bound to the Engine %3 (%4), active Device Agent exists: %5",
                device->getUserDefinedName(), device->getId(),
                engineResource->getName(), engineResource->getId(),
                hasAliveDeviceAgent);

            ++engineBindingInfoMap[engineResource].boundResourceCount;
            if (hasAliveDeviceAgent)
                ++engineBindingInfoMap[engineResource].onlineBoundResourceCount;
        }
    }

    return std::make_unique<metrics::CommonPluginResourceBindingInfoHolder>(
        std::move(engineBindingInfoMap));
}

void Manager::registerMetadataSink(
    const QnResourcePtr& deviceResource,
    MetadataSinkPtr metadataSink)
{
    auto device = deviceResource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ERROR(this,
            "Can't register metadata sink for the Resource %1 (%2), the resource is not a Device",
            deviceResource->getName(), deviceResource->getId());
        return;
    }

    NX_DEBUG(this, "Registering metadata sink for Device %1", device);

    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    MetadataSinkSet metadataSinks;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_metadataSinks[deviceResource->getId()].insert(std::move(metadataSink));

        metadataSinks = m_metadataSinks[deviceResource->getId()];
        analyticsContext = deviceAnalyticsContextUnsafe(device);
    }

    if (analyticsContext)
        analyticsContext->setMetadataSinks(metadataSinks);
}

QWeakPointer<StreamDataReceptor> Manager::registerMediaSource(
    const QnUuid& deviceId,
    nx::vms::api::StreamIndex streamIndex)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QSharedPointer<ProxyStreamDataReceptor> proxySource;
    if (auto it = m_mediaSources.find(deviceId); it != m_mediaSources.cend())
        proxySource = it->second;
    else
        proxySource = QSharedPointer<ProxyStreamDataReceptor>::create();

    NX_DEBUG(this, "Registering media source for Device %1, stream index: %2",
        deviceId, streamIndex);

    proxySource->registerStream(streamIndex);

    auto analyticsContext = deviceAnalyticsContextUnsafe(deviceId);
    if (analyticsContext)
        proxySource->setProxiedReceptor(analyticsContext);

    m_mediaSources[deviceId] = proxySource;
    return proxySource;
}

void Manager::setSettings(
    const QString& deviceId,
    const QString& engineId,
    const QJsonObject& deviceAgentSettings)
{
    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        analyticsContext = deviceAnalyticsContextUnsafe(QnUuid(deviceId));
    }

    if (!NX_ASSERT(analyticsContext, lm("Device %1").arg(deviceId)))
        return;

    analyticsContext->setSettingsValues(engineId, deviceAgentSettings);
}

std::optional<Settings> Manager::getSettings(
    const QString& deviceId, const QString& engineId) const
{
    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        analyticsContext = deviceAnalyticsContextUnsafe(QnUuid(deviceId));
    }

    if (!analyticsContext)
        return {};

    return analyticsContext->getSettings(engineId);
}

void Manager::setSettings(const QString& engineId, const QJsonObject& engineSettings)
{
    auto engine = sdk_support::find<AnalyticsEngineResource>(serverModule(), engineId);

    if (!NX_ASSERT(engine, lm("Engine %1").arg(engineId)))
        return;

    NX_DEBUG(this, "Setting settings for the Engine %1 (%2)", engine->getName(), engine->getId());
    engine->setSettingsValues(engineSettings);
}

QJsonObject Manager::getSettings(const QString& engineId) const
{
    auto engine = sdk_support::find<AnalyticsEngineResource>(serverModule(), engineId);

    if (!NX_ASSERT(engine, lm("Engine %1").arg(engineId)))
        return {};

    NX_DEBUG(this, "Getting settings for the Engine %1 (%2)", engine->getName(), engine->getId());
    return engine->settingsValues();
}

MetadataSinkSet Manager::metadataSinksUnsafe(
    const QnUuid& deviceId) const
{
    auto it = m_metadataSinks.find(deviceId);
    if (it == m_metadataSinks.cend())
        return {};

    return it->second;
}

QWeakPointer<ProxyStreamDataReceptor> Manager::mediaSourceUnsafe(const QnUuid& deviceId) const
{
    auto it = m_mediaSources.find(deviceId);
    if (it == m_mediaSources.cend())
        return QWeakPointer<ProxyStreamDataReceptor>();

    return it->second;
}

nx::vms::server::resource::AnalyticsEngineResourceList Manager::localEngines() const
{
    using namespace nx::vms::server::resource;
    return resourcePool()->getResources<AnalyticsEngineResource>(
        [](const AnalyticsEngineResourcePtr& engine) { return !!engine->sdkEngine(); });
}

QnVirtualCameraResourceList Manager::localDevices() const
{
    return resourcePool()->getAllCameras(
        serverModule()->commonModule()->currentServer(),
        /*ignoreDesktopCamera*/ true);
}

bool Manager::isLocalDevice(const QnVirtualCameraResourcePtr& device) const
{
    return device->getParentId() == moduleGUID();
}

QSet<QnUuid> Manager::compatibleEngineIds(const QnVirtualCameraResourcePtr& device) const
{
    QSet<QnUuid> result;
    if (!NX_ASSERT(device))
        return result;

    for (auto& engine: localEngines())
    {
        auto sdkEngine = engine->sdkEngine();
        if (!NX_ASSERT(sdkEngine))
            continue;

        if (isEngineCompatibleWithDevice(sdkEngine, device))
            result.insert(engine->getId());
    }

    return result;
}

void Manager::updateCompatibilityWithEngines(const QnVirtualCameraResourcePtr& device)
{
    device->setCompatibleAnalyticsEngines(compatibleEngineIds(device));
    device->savePropertiesAsync();
}

void Manager::updateCompatibilityWithDevices(const AnalyticsEngineResourcePtr& engine)
{
    const auto sdkEngine = engine->sdkEngine();
    if (!sdkEngine || !sdkEngine->isValid())
    {
        NX_DEBUG(this, "The Engine Resource %1 (%2) has no assigned SDK Engine",
            engine->getName(), engine->getId());
        return;
    }

    const auto engineId = engine->getId();
    auto devices = resourcePool()->getAllCameras(
        /*any server*/ QnResourcePtr(),
        /*ignoreDesktopCamera*/ true);

    for (auto& device: devices)
    {
        auto compatibleEngineIds = device->compatibleAnalyticsEngines();
        if (isEngineCompatibleWithDevice(sdkEngine, device))
            compatibleEngineIds.insert(engineId);
        else
            compatibleEngineIds.remove(engineId);

        device->setCompatibleAnalyticsEngines(compatibleEngineIds);
        device->savePropertiesAsync();
    }
}

void Manager::updateEnabledAnalyticsEngines(const QnVirtualCameraResourcePtr& device)
{
    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        analyticsContext = deviceAnalyticsContextUnsafe(device);
    }

    if (!analyticsContext)
    {
        NX_DEBUG(this, "Can't find a DeviceAnalyticsContext for the Device %1 (%2)",
            device->getUserDefinedName(), device->getId());
        return;
    }

    analyticsContext->setEnabledAnalyticsEngines(
        device->enabledAnalyticsEngineResources().filtered<resource::AnalyticsEngineResource>());
}

} // namespace nx::vms::server::analytics
