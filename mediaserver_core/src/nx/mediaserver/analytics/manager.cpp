#include "manager.h"

#include <memory>
#include <tuple>

#include <libavutil/pixfmt.h>

#include <nx/kit/debug.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <nx/utils/file_system.h>

#include <plugins/plugin_manager.h>
#include <plugins/plugin_tools.h>
#include <nx/mediaserver_plugins/utils/uuid.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/mediaserver/analytics/metadata_handler.h>
#include <nx/mediaserver/analytics/event_rule_watcher.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/plugins/settings.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/sdk/analytics/consuming_device_agent.h>
#include <nx/debugging/visual_metadata_debugger_factory.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/log/log_main.h>
#include <mediaserver_ini.h>
#include <plugins/plugins_ini.h>
#include <nx/sdk/analytics/pixel_format.h>
#include <nx/sdk/analytics/plugin.h>

#include "yuv420_uncompressed_video_frame.h"
#include "generic_uncompressed_video_frame.h"
#include "wrapping_compressed_video_packet.h"
#include "data_packet_adapter.h"
#include "frame_converter.h"

namespace nx {
namespace mediaserver {
namespace analytics {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::debugging;
using namespace nx::vms::common;

using PixelFormat = UncompressedVideoFrame::PixelFormat;

Manager::Manager(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule),
    m_thread(new QThread(this)),
    m_visualMetadataDebugger(
        VisualMetadataDebuggerFactory::makeDebugger(DebuggerType::analyticsManager))
{
    m_thread->setObjectName(lit("AnalyticsManager"));
    moveToThread(m_thread);
    m_thread->start();
}

Manager::~Manager()
{
    NX_DEBUG(this, lit("Destroying AnalyticsManager."));
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
    NX_DEBUG(this, lit("Initializing AnalyticsManager."));

    connect(
        resourcePool(), &QnResourcePool::resourceAdded,
        this, &Manager::at_resourceAdded);

    connect(
        resourcePool(), &QnResourcePool::resourceRemoved,
        this, &Manager::at_resourceRemoved);

    connect(
        serverModule()->analyticsEventRuleWatcher(), &EventRuleWatcher::rulesUpdated,
        this, &Manager::at_rulesUpdated);

    QMetaObject::invokeMethod(this, "initExistingResources");
}

void Manager::initExistingResources()
{
    const auto mediaServer = resourcePool()->getResourceById<QnMediaServerResource>(moduleGUID());
    const auto devices = resourcePool()->getAllCameras(mediaServer, /*ignoreDesktopCamera*/ true);
    for (const auto& device: devices)
        at_deviceAdded(device);
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
    auto analyticsEngine = resource.dynamicCast<resource::AnalyticsEngineResource>();
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
    if (!device)
    {
        NX_VERBOSE(
            this,
            lm("Resource %1 (%2) is neither analytics engine nor device. Skipping.")
                .args(resource->getName(), resource->getId()));
        return;
    }

    NX_VERBOSE(
        this,
        lm("Device %1 (%2) has been added.")
            .args(resource->getName(), resource->getId()));

    at_deviceAdded(device);
}

void Manager::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (!resource)
    {
        NX_VERBOSE(this, "Receieved null resource");
        return;
    }

    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been removed.")
            .args(resource->getName(), resource->getId()));

    resource->disconnect(this);

    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (device)
    {
        NX_VERBOSE(
            this,
            lm("Device %1 (%2) has been removed")
                .args(device->getUserDefinedName(), device->getId()));
        at_deviceRemoved(device);
        return;
    }

    auto engine = resource.dynamicCast<nx::mediaserver::resource::AnalyticsEngineResource>();
    if (!engine)
    {
        NX_VERBOSE(
            this,
            lm("Resource %1 (%2) is neither analytics engine nor device. Skipping.")
                .args(resource->getName(), resource->getId()));
        return;
    }

    NX_VERBOSE(
        this,
        lm("Engine %1 (%2) has been removed.")
            .args(engine->getName(), engine->getId()));

    at_engineRemoved(engine);
}

void Manager::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
#if 0
    NX_VERBOSE(
        this,
        lm("Rules have been updated. Affected resources: %1").arg(affectedResources));

    for (const auto& resourceId: affectedResources)
    {
        auto resource = resourcePool()->getResourceById(resourceId);
        if (!resource)
            releaseDeviceAgentsUnsafe(resourceId);
        else
            handleResourceChanges(resource);
    }
#endif
}

void Manager::at_resourceParentIdChanged(const QnResourcePtr& resource)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_WARNING(this, "Resource is not a device.");
        return;
    }

    at_deviceParentIdChanged(device);
}

void Manager::at_resourcePropertyChanged(const QnResourcePtr& resource, const QString& propertyName)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (device)
    {
        at_devicePropertyChanged(device, propertyName);
        return;
    }

    auto engine = resource.dynamicCast<resource::AnalyticsEngineResource>();
    if (!engine)
    {
        NX_WARNING(this, "Resource is not a device or engine");
        return;
    }

    at_enginePropertyChanged(engine, propertyName);
}

void Manager::at_deviceAdded(const QnVirtualCameraResourcePtr& device)
{
    connect(
        device, &QnResource::parentIdChanged,
        this, &Manager::at_resourceParentIdChanged);

    if (isLocalDevice(device))
        handleDeviceArrivalToServer(device);
}

void Manager::at_deviceRemoved(const QnVirtualCameraResourcePtr& device)
{
    handleDeviceRemovalFromServer(device);
}

void Manager::at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device)
{
    if (isLocalDevice(device))
    {
        NX_VERBOSE(this, lm("Device %1 (%2) has been moved to the current server"));
        handleDeviceArrivalToServer(device);
    }
    else
    {
        NX_VERBOSE(this, lm("Device %1 (%2) has been moved to another server"));
        handleDeviceRemovalFromServer(device);
    }
}

void Manager::at_devicePropertyChanged(
    const QnVirtualCameraResourcePtr& device,
    const QString& propertyName)
{
    if (propertyName == QnVirtualCameraResource::kEnabledAnalyticsEnginesProperty)
    {
        auto analyticsContext = context(device);
        if (!analyticsContext)
        {
            NX_DEBUG(this, lm("Can't find analytics context for device %1 (%2)")
                .args(device->getUserDefinedName(), device->getId()));
        }

        analyticsContext->setEnabledAnalyticsEngines(
            sdk_support::toServerEngineList(device->enabledAnalyticsEngineResources()));
    }
}

void Manager::handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device)
{
    connect(
        device, &QnResource::propertyChanged,
        this, &Manager::at_resourcePropertyChanged);

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

void Manager::at_engineAdded(const resource::AnalyticsEngineResourcePtr& engine)
{
    connect(
        engine, &QnResource::propertyChanged,
        this, &Manager::at_resourcePropertyChanged);
}

void Manager::at_engineRemoved(const resource::AnalyticsEngineResourcePtr& engine)
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
    const resource::AnalyticsEngineResourcePtr& engine,
    const QString& propertyName)
{
    if (!engine)
    {
        NX_WARNING(this, "Got empty analytics engine resource, skipping");
        return;
    }

    if (propertyName == nx::vms::common::AnalyticsEngineResource::kSettingsValuesProperty)
        updateEngineSettings(engine);
}

void Manager::updateEngineSettings(const resource::AnalyticsEngineResourcePtr& engine)
{
    auto sdkEngine = engine->sdkEngine();
    if (!sdkEngine)
    {
        NX_ERROR(this, "Can't access underlying SDK analytics engine object for %1 (%2)",
            engine->getName(), engine->getId());
        return;
    }

    NX_DEBUG(this, "Updating engine %1 (%2) with settings",
        engine->getName(), engine->getId());

    auto settings = engine->settingsValues();
    auto settingsHolder = sdk_support::toSettingsHolder(settings);
    sdkEngine->setSettings(settingsHolder->array(), settingsHolder->size());
}

void Manager::registerMetadataSink(
    const QnResourcePtr& resource,
    QWeakPointer<QnAbstractDataReceptor> metadataSink)
{
    auto device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!device)
    {
        NX_ERROR(this, "Can't register metadata sink for resource %1 (%2)",
            resource->getName(), resource->getId());
        return;
    }

    m_metadataSinks[resource->getId()] = metadataSink;
    auto analyticsContext = context(device);
    if (analyticsContext)
        analyticsContext->setMetadataSink(metadataSink);
}

QWeakPointer<AbstractVideoDataReceptor> Manager::registerMediaSource(const QnUuid& resourceId)
{
    auto proxySource = QSharedPointer<ProxyVideoDataReceptor>::create();
    auto& analyticsContext = context(resourceId);

    if (analyticsContext)
        proxySource->setProxiedReceptor(analyticsContext);

    m_mediaSources[resourceId] = proxySource;
    return proxySource;
}

QWeakPointer<QnAbstractDataReceptor> Manager::metadataSink(
    const QnVirtualCameraResourcePtr& device) const
{
    return metadataSink(device->getId());
}

QWeakPointer<QnAbstractDataReceptor> Manager::metadataSink(const QnUuid& deviceId) const
{
    auto itr = m_metadataSinks.find(deviceId);
    if (itr == m_metadataSinks.cend())
        return nullptr;

    return itr->second;
}

QWeakPointer<ProxyVideoDataReceptor> Manager::mediaSource(
    const QnVirtualCameraResourcePtr& device) const
{
    return mediaSource(device->getId());
}

QWeakPointer<ProxyVideoDataReceptor> Manager::mediaSource(const QnUuid& deviceId) const
{
    auto itr = m_mediaSources.find(deviceId);
    if (itr == m_mediaSources.cend())
        return nullptr;

    return itr->second;
}

#if 0

/** @return Empty settings if the file does not exist, or on error. */
std::unique_ptr<const nx::plugins::SettingsHolder> Manager::loadSettingsFromFile(
    const QString& fileDescription, const QString& filename)
{
    using nx::utils::log::Level;
    auto log = //< Can be used to return empty settings: return log(...)
        [&](Level level, const QString& message)
    {
        NX_UTILS_LOG(level, this) << lm("Metadata %1 settings: %2: [%3]")
            .args(fileDescription, message, filename);
        return std::make_unique<const nx::plugins::SettingsHolder>();
    };

    if (!QFileInfo::exists(filename))
        return log(Level::info, lit("File does not exist"));

    log(Level::info, lit("Loading from file"));

    QFile f(filename);
    if (!f.open(QFile::ReadOnly))
        return log(Level::error, lit("Unable to open file"));

    const QString& settingsJson = f.readAll();
    if (settingsJson.isEmpty())
        return log(Level::error, lit("Unable to read from file"));

    auto settings = std::make_unique<const nx::plugins::SettingsHolder>(settingsJson);
    if (!settings->isValid())
        return log(Level::error, lit("Invalid JSON in file"));

    return settings;
}

/** If path is empty, the path to ini_config .ini files is used. */
static QString settingsFilename(
    const char* const path, const QString& pluginLibName, const QString& extraSuffix = "")
{
    QString dir = QDir::cleanPath( //< Normalize to use forward slashes, as required by QFile.
        QString::fromUtf8(path[0] ? path : nx::kit::IniConfig::iniFilesDir()));
    if (!dir.isEmpty() && dir.at(dir.size() - 1) != '/')
        dir.append('/');
    return dir + pluginLibName + extraSuffix + lit(".json");
}

void Manager::setDeviceAgentSettings(
    DeviceAgent* deviceAgent,
    const QnVirtualCameraResourcePtr& camera)
{
    // TODO: Stub. Implement storing the settings in the database.
    const auto settings = loadSettingsFromFile(lit("DeviceAgent"), settingsFilename(
        pluginsIni().analyticsDeviceAgentSettingsPath,
        deviceAgent->engine()->plugin()->name(),
        lit("_device_agent_for_") + camera->getPhysicalId()));
    deviceAgent->setSettings(settings->array(), settings->size());
}

void Manager::setEngineSettings(Engine* engine)
{
    // TODO: Stub. Implement storing the settings in the database.
    const auto settings = loadSettingsFromFile(lit("Engine"), settingsFilename(
        pluginsIni().analyticsEngineSettingsPath, engine->plugin()->name()));
    engine->setSettings(settings->array(), settings->size());
}

#endif

} // namespace analytics
} // namespace mediaserver
} // namespace nx


