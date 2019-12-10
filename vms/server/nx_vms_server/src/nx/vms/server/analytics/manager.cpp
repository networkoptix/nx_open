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
#include <nx/vms/server/analytics/wrappers/engine.h>
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
using namespace nx::vms::server::metrics;

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
    updateCompatibilityWithEngines(device);
    connect(
        device, &QnVirtualCameraResource::userEnabledAnalyticsEnginesChanged,
        this, &Manager::at_deviceUserEnabledAnalyticsEnginesChanged);

    auto context = QSharedPointer<DeviceAnalyticsContext>::create(serverModule(), device);
    context->setEnabledAnalyticsEngines(
        device->enabledAnalyticsEngineResources().filtered<resource::AnalyticsEngineResource>());
    context->setMetadataSink(metadataSinkUnsafe(device->getId()));

    if (auto source = mediaSourceUnsafe(device->getId()).toStrongRef())
        source->setProxiedReceptor(context);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_deviceAnalyticsContexts.emplace(device->getId(), context);
}

void Manager::handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
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

std::vector<PluginMetrics> Manager::metrics() const
{
    std::map</*Engine id*/ QnUuid, PluginMetrics> engineMetrics;
    for (const auto& [deviceId, context]: m_deviceAnalyticsContexts)
    {
        const auto device = resourcePool()->getResourceById<QnVirtualCameraResource>(deviceId);
        if (!device || !isLocalDevice(device))
            continue;

        const auto bindingStatuses = context->bindingStatuses();
        for (const auto& [engineId, hasAliveDeviceAgent]: bindingStatuses)
        {
            ++engineMetrics[engineId].numberOfBoundResources;
            if (hasAliveDeviceAgent)
                ++engineMetrics[engineId].numberOfAliveBoundResources;

            if (engineMetrics[engineId].name.isEmpty())
            {
                if (const auto engineResource = resourcePool()->getResourceById(engineId))
                    engineMetrics[engineId].name = engineResource->getName();
            }
        }
    }

    std::vector<PluginMetrics> result;
    for (auto& [engineId, metrics]: engineMetrics)
        result.push_back(std::move(metrics));

    return result;
}

void Manager::registerMetadataSink(
    const QnResourcePtr& deviceResource,
    QWeakPointer<QnAbstractDataReceptor> metadataSink)
{
    auto device = deviceResource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ERROR(this,
            "Can't register metadata sink for the Resource %1 (%2), the resource is not a Device",
            deviceResource->getName(), deviceResource->getId());
        return;
    }

    QSharedPointer<DeviceAnalyticsContext> analyticsContext;
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_metadataSinks[deviceResource->getId()] = metadataSink;
        analyticsContext = deviceAnalyticsContextUnsafe(device);
    }

    if (analyticsContext)
        analyticsContext->setMetadataSink(metadataSink);
}

QWeakPointer<IStreamDataReceptor> Manager::registerMediaSource(const QnUuid& deviceId)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto proxySource = QSharedPointer<ProxyStreamDataReceptor>::create();
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

    analyticsContext->setSettings(engineId, deviceAgentSettings);
}

QJsonObject Manager::getSettings(const QString& deviceId, const QString& engineId) const
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

QWeakPointer<QnAbstractDataReceptor> Manager::metadataSinkUnsafe(const QnUuid& deviceId) const
{
    auto it = m_metadataSinks.find(deviceId);
    if (it == m_metadataSinks.cend())
        return QWeakPointer<QnAbstractDataReceptor>();

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

        if (sdkEngine->isCompatible(device))
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
        if (sdkEngine->isCompatible(device))
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
