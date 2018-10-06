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
#include <nx/plugins/settings.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/sdk/analytics/consuming_device_agent.h>
#include <nx/debugging/visual_metadata_debugger_factory.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/log/log_main.h>
#include <mediaserver_ini.h>
#include <plugins/plugins_ini.h>
#include <nx/sdk/analytics/pixel_format.h>
#include <nx/sdk/analytics/plugin.h>

#include "video_data_receptor.h"
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
using PixelFormat = UncompressedVideoFrame::PixelFormat;

Manager::Manager(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule),
    m_visualMetadataDebugger(
        VisualMetadataDebuggerFactory::makeDebugger(DebuggerType::analyticsManager)),
    m_thread(new QThread(this))
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
    m_contexts.clear();
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

    const auto cameras = resourcePool()->getAllCameras(
        mediaServer,
        /*ignoreDesktopCamera*/ true);

    for (const auto& camera: cameras)
        at_resourceAdded(camera);
}

void Manager::at_resourceAdded(const QnResourcePtr& resource)
{
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been added.")
            .args(resource->getName(), resource->getId()));

    connect(
        resource, &QnResource::statusChanged,
        this, &Manager::handleResourceChanges);

    connect(
        resource, &QnResource::parentIdChanged,
        this, &Manager::handleResourceChanges);

    connect(
        resource, &QnResource::urlChanged,
        this, &Manager::handleResourceChanges);

    connect(
        camera, &QnSecurityCamResource::logicalIdChanged,
        this, &Manager::handleResourceChanges);

    connect(
        resource, &QnResource::propertyChanged,
        this, &Manager::at_propertyChanged);

    handleResourceChanges(resource);
}

void Manager::at_propertyChanged(const QnResourcePtr& resource, const QString& name)
{
    if (name == Qn::CAMERA_CREDENTIALS_PARAM_NAME)
    {
        NX_DEBUG(
            this,
            lm("Credentials have been changed for resource %1 (%2)")
                .args(resource->getName(), resource->getId()));

        handleResourceChanges(resource);
    }
}

void Manager::at_resourceRemoved(const QnResourcePtr& resource)
{
    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been removed.")
            .args(resource->getName(), resource->getId()));

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    camera->disconnect(this);
    QnMutexLocker lock(&m_contextMutex);
    m_contexts.erase(resource->getId());
}

void Manager::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
    NX_VERBOSE(
        this,
        lm("Rules have been updated. Affected resources: %1").arg(affectedResources));

    for (const auto& resourceId: affectedResources)
    {
        auto resource = resourcePool()->getResourceById(resourceId);
        if (!resource)
            releaseResourceDeviceAgentsUnsafe(resourceId);
        else
            handleResourceChanges(resource);
    }
}

Manager::EngineList Manager::availableEngines() const
{
    auto pluginManager = serverModule()->pluginManager();
    NX_ASSERT(pluginManager, "Cannot access PluginManager instance");
    if (!pluginManager)
        return EngineList();

    const auto plugins = pluginManager->findNxPlugins<nx::sdk::analytics::Plugin>(
        nx::sdk::analytics::IID_Plugin);
    EngineList engines;
    for (const auto& plugin: plugins)
    {
        Error error = Error::noError;
        const auto engine = plugin->createEngine(&error);
        if (!engine)
        {
            NX_ERROR(this, "Cannot create Engine for plugin %1: plugin returned null.",
                plugin->name());
            continue;
        }
        if (error != Error::noError)
        {
            NX_ERROR(this, "Cannot create Engine for plugin %1: plugin returned error %2.",
                plugin->name(), error);
            engine->releaseRef();
            continue;
        }
        NX_VERBOSE(this, "Created Engine of %1", plugin->name());
        engines.append(engine);
    }
    return engines;
    // TODO: Rewrite; the above implementation does not care about Plugin and Engine lifetimes. 
}

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

/** @return Dir ending with "/", intended to receive manifest files. */
static QString manifestFileDir()
{
    QString dir = QDir::cleanPath( //< Normalize to use forward slashes, as required by QFile.
        QString::fromUtf8(pluginsIni().analyticsManifestOutputPath));

    if (QFileInfo(dir).isRelative())
    {
        dir.insert(0,
            // NOTE: QDir::cleanPath() removes trailing '/'.
            QDir::cleanPath(QString::fromUtf8(nx::kit::IniConfig::iniFilesDir())) + lit("/"));
    }

    if (!dir.isEmpty() && dir.at(dir.size() - 1) != '/')
        dir.append('/');

    return dir;
}

/**
 * Intended for debug. On error, just log the error message.
 */
void Manager::saveManifestToFile(
    const char* const manifest,
    const QString& fileDescription,
    const QString& pluginLibName,
    const QString& filenameExtraSuffix)
{
    const QString dir = manifestFileDir();
    const QString filename = dir + pluginLibName + filenameExtraSuffix + lit("_manifest.json");

    using nx::utils::log::Level;
    auto log = //< Can be used to return after logging: return log(...).
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, this) << lm("Metadata %1 manifest: %2: [%3]")
                .args(fileDescription, message, filename);
        };

    log(Level::info, lit("Saving to file"));

    if (!nx::utils::file_system::ensureDir(dir))
        return log(Level::error, lit("Unable to create directory for file"));

    QFile f(filename);
    if (!f.open(QFile::WriteOnly))
        return log(Level::error, lit("Unable to (re)create file"));

    const qint64 len = (qint64) strlen(manifest);
    if (f.write(manifest, len) != len)
        return log(Level::error, lit("Unable to write to file"));
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
    const QnSecurityCamResourcePtr& camera)
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

void Manager::createDeviceAgentsForResourceUnsafe(const QnSecurityCamResourcePtr& camera)
{
    if (ini().enablePersistentAnalyticsDeviceAgent)
    {
        const auto itr = m_contexts.find(camera->getId());
        if (itr != m_contexts.cend())
        {
            if (!itr->second.deviceAgentContexts().empty())
                return;
        }
    }
    QnMediaServerResourcePtr server = resourcePool()
        ->getResourceById<QnMediaServerResource>(moduleGUID());
    NX_ASSERT(server, lm("Can not obtain current server resource."));
    if (!server)
        return;

    for (Engine* const engine: availableEngines())
    {
        // TODO: Consider assigning Engine settings earlier.
        setEngineSettings(engine);

        nxpt::ScopedRef<DeviceAgent> deviceAgent(
            createDeviceAgent(camera, engine), /*increaseRef*/ false);
        if (!deviceAgent)
            continue;
        boost::optional<nx::vms::api::analytics::EngineManifest> engineManifest =
            loadEngineManifest(engine);
        if (!engineManifest)
            continue; //< The error is already logged.
        assignEngineManifestToServer(*engineManifest, server);

        boost::optional<nx::vms::api::analytics::DeviceAgentManifest> deviceAgentManifest;
        boost::optional<nx::vms::api::analytics::EngineManifest> auxiliaryEngineManifest;
        std::tie(deviceAgentManifest, auxiliaryEngineManifest) =
            loadDeviceAgentManifest(engine, deviceAgent.get(), camera);
        if (deviceAgentManifest)
        {
            // TODO: Fix: Camera property should receive a union of data from all plugins.
            addManifestToCamera(*deviceAgentManifest, camera);
        }
        if (auxiliaryEngineManifest)
        {
            auxiliaryEngineManifest->pluginId = engineManifest->pluginId;
            engineManifest = mergeEngineManifestToServer(*auxiliaryEngineManifest, server);
        }

        setDeviceAgentSettings(deviceAgent.get(), camera);

        auto& context = m_contexts[camera->getId()];
        std::unique_ptr<MetadataHandler> handler(
            createMetadataHandler(camera, *engineManifest));

        if (auto consumingDeviceAgent = nxpt::ScopedRef<DeviceAgent>(
            deviceAgent->queryInterface(IID_DeviceAgent)))
        {
            handler->registerDataReceptor(&context);
            handler->setVisualDebugger(m_visualMetadataDebugger.get());
        }

        context.addDeviceAgent(std::move(deviceAgent), std::move(handler), *engineManifest);
    }
}

DeviceAgent* Manager::createDeviceAgent(
    const QnSecurityCamResourcePtr& camera,
    Engine* engine) const
{
    NX_ASSERT(camera && engine);
    if (!camera || !engine)
        return nullptr;

    NX_DEBUG(this, lm("Creating DeviceAgent for device %1 (%2).")
        .args(camera->getUserDefinedName(), camera->getId()));

    CameraInfo cameraInfo;
    bool success = cameraInfoFromResource(camera, &cameraInfo);
    if (!success)
    {
        NX_WARNING(this, lm("Cannot create resource info from resource %1 (%2)")
            .args(camera->getUserDefinedName(), camera->getId()));
        return nullptr;
    }

    NX_DEBUG(this, lm("Resource info for resource %1 (%2): %3")
        .args(camera->getUserDefinedName(), camera->getId(), cameraInfo));

    Error error = Error::noError;
    const auto deviceAgent = engine->obtainDeviceAgent(&cameraInfo, &error);
    if (!deviceAgent)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned null.")
            .args(camera->getUserDefinedName(), camera->getId()));
        return nullptr;
    }
    if (error != Error::noError)
    {
        NX_ERROR(this, lm("Cannot obtain DeviceAgent %1 (%2), plugin returned error %3.")
            .args(camera->getUserDefinedName(), camera->getId()), error);
        deviceAgent->releaseRef();
        return nullptr;
    }
    return deviceAgent;
}

void Manager::releaseResourceDeviceAgentsUnsafe(const QnSecurityCamResourcePtr& camera)
{
    releaseResourceDeviceAgentsUnsafe(camera->getId());
}

void Manager::releaseResourceDeviceAgentsUnsafe(const QnUuid& cameraId)
{
    if (ini().enablePersistentAnalyticsDeviceAgent)
        return;

    auto& context = m_contexts[cameraId];
    context.clearDeviceAgentContexts();
    context.setDeviceAgentContextsInitialized(false);
}

MetadataHandler* Manager::createMetadataHandler(
    const QnResourcePtr& resource,
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
    {
        NX_ERROR(
            this,
            lm("Resource %1 (%2) is not an instance of SecurityCameResource")
                .args(resource->getName(), resource->getId()));
        return nullptr;
    }

    auto handler = new MetadataHandler(serverModule());
    handler->setResource(camera);
    handler->setManifest(manifest);

    return handler;
}

void Manager::handleResourceChanges(const QnResourcePtr& resource)
{
    NX_ASSERT(resource);
    if (!resource)
        return;

    NX_VERBOSE(
        this,
        lm("Handling resource changes for device %1 (%2)")
            .args(resource->getName(), resource->getId()));

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    {
        NX_DEBUG(
            this,
            lm("Creating DeviceAgents for device %1 (%2).")
                .args(camera->getUserDefinedName(), camera->getId()));

    }
    auto resourceId = camera->getId();
    QnMutexLocker lock(&m_contextMutex);
    auto& context = m_contexts[resourceId];
    if (isCameraAlive(camera))
    {
        if (!context.areDeviceAgentContextsInitialized())
            createDeviceAgentsForResourceUnsafe(camera);
        auto events = serverModule()->analyticsEventRuleWatcher()->watchedEventsForResource(resourceId);
        fetchMetadataForResourceUnsafe(resourceId, context, events);
    }
    else
    {
        releaseResourceDeviceAgentsUnsafe(camera);
    }
}

bool Manager::isCameraAlive(const QnSecurityCamResourcePtr& camera) const
{
    if (!camera)
        return false;

    const auto flags = camera->flags();

    NX_VERBOSE(
        this,
        lm("Determining if metadata can be fetched from resource: "
            "is foreign resource: %1, "
            "is desktop camera: %2, "
            "is resource status ok: %3")
            .args(
                flags.testFlag(Qn::foreigner),
                flags.testFlag(Qn::desktop_camera),
                (camera->getStatus() == Qn::Online || camera->getStatus() == Qn::Recording)));

    if (flags.testFlag(Qn::foreigner) || flags.testFlag(Qn::desktop_camera))
        return false;

    return camera->getStatus() >= Qn::Online;
}

void Manager::fetchMetadataForResourceUnsafe(
    const QnUuid& resourceId,
    ResourceAnalyticsContext& context,
    const QSet<QString>& eventTypeIds)
{
    for (auto& deviceEngineContext: context.deviceAgentContexts())
    {
        if (eventTypeIds.empty() && !deviceEngineContext->isStreamConsumer())
        {
            NX_DEBUG(
                this,
                lm("Event list is empty, stopping metadata fetching for resource %1.")
                    .arg(resourceId));

            // Stop the thread to guarantee there is no 2 simultaneous calls.
            deviceEngineContext->stop();

            if (deviceEngineContext->deviceAgent()->stopFetchingMetadata() != Error::noError)
            {
                NX_WARNING(this, lm("Failed to stop fetching metadata from Engine %1")
                    .arg(deviceEngineContext->engineManifest().pluginName.value));
            }
        }
        else
        {
            NX_DEBUG(this, lm("Statopping metadata fetching for resource %1").args(resourceId));
            if (deviceEngineContext->deviceAgent()->stopFetchingMetadata() != Error::noError)
            {
                NX_WARNING(this, lm("Failed to stop fetching metadata from Engine %1")
                    .arg(deviceEngineContext->engineManifest().pluginName.value));
            }

            NX_DEBUG(this, lm("Starting metadata fetching for resource %1. Event list is %2")
                .args(resourceId, eventTypeIds));

            std::vector<std::string> eventTypeList;
            std::vector<const char*> eventTypePtrs;
            for (const auto& eventTypeId: eventTypeIds)
            {
                eventTypeList.push_back(eventTypeId.toStdString());
                eventTypePtrs.push_back(eventTypeList.back().c_str());
            }
            const auto result = deviceEngineContext->deviceAgent()->startFetchingMetadata(
                eventTypePtrs.empty() ? nullptr : &eventTypePtrs.front(),
                (int) eventTypePtrs.size());

            if (result != Error::noError)
            {
                NX_WARNING(this, lm("Failed to stop fetching metadata from Engine %1")
                    .arg(deviceEngineContext->engineManifest().pluginName.value));
            }
        }
    }
}

static uint qHash(const nx::vms::api::analytics::EventType& eventType) // noexcept
{
    return qHash(eventType.id);
}

boost::optional<nx::vms::api::analytics::EngineManifest> Manager::loadEngineManifest(
    Engine* engine)
{
    Error error = Error::noError;
    // TODO: #mshevchenko: Consider a dedicated mechanism for localization.

    const char* const manifestStr = engine->manifest(&error);
    if (error != Error::noError)
    {
        NX_ERROR(this, "Unable to receive Engine manifest for plugin \"%1\": %2.",
            engine->plugin()->name(), error);
        return boost::none;
    }
    if (manifestStr == nullptr)
    {
        NX_ERROR(this, "Received null Engine manifest for plugin \"%1\".",
            engine->plugin()->name());
        return boost::none;
    }

    if (pluginsIni().analyticsManifestOutputPath[0])
    {
        saveManifestToFile(
            manifestStr,
            "Engine",
            engine->plugin()->name());
    }

    auto engineManifest = deserializeManifest<nx::vms::api::analytics::EngineManifest>(manifestStr);

    if (!engineManifest)
    {
        NX_ERROR(this, "Cannot deserialize Engine manifest from plugin \"%1\"",
            engine->plugin()->name());
        return boost::none; //< Error already logged.
    }

    return engineManifest;
}

void Manager::assignEngineManifestToServer(
    const nx::vms::api::analytics::EngineManifest& manifest,
    const QnMediaServerResourcePtr& server)
{
    auto existingManifests = server->analyticsDrivers();
    auto it = std::find_if(existingManifests.begin(), existingManifests.end(),
        [&manifest](const nx::vms::api::analytics::EngineManifest& m)
        {
            return m.pluginId == manifest.pluginId;
        });

    if (it == existingManifests.cend())
        existingManifests.push_back(manifest);
    else
        *it = manifest;

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();
}

nx::vms::api::analytics::EngineManifest Manager::mergeEngineManifestToServer(
    const nx::vms::api::analytics::EngineManifest& manifest,
    const QnMediaServerResourcePtr& server)
{
    nx::vms::api::analytics::EngineManifest* result = nullptr;
    auto existingManifests = server->analyticsDrivers();
    auto it = std::find_if(existingManifests.begin(), existingManifests.end(),
        [&manifest](const nx::vms::api::analytics::EngineManifest& m)
        {
            return m.pluginId == manifest.pluginId;
        });

    if (it == existingManifests.cend())
    {
        existingManifests.push_back(manifest);
        result = &existingManifests.back();
    }
    else
    {
        it->outputEventTypes = it->outputEventTypes.toSet().
            unite(manifest.outputEventTypes.toSet()).toList();
        result = &*it;
    }

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();
    NX_ASSERT(result);
    return *result;
}

std::pair<
    boost::optional<nx::vms::api::analytics::DeviceAgentManifest>,
    boost::optional<nx::vms::api::analytics::EngineManifest>
>
Manager::loadDeviceAgentManifest(
    const Engine* engine,
    DeviceAgent* deviceAgent,
    const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(deviceAgent);
    NX_ASSERT(camera);

    Error error = Error::noError;

    // "deviceAgentManifest" contains const char* representation of the manifest.
    // unique_ptr allows us to automatically ask the DeviceAgent to release it when the memory is
    // not needed anymore.
    auto deleter = [deviceAgent](const char* ptr) { deviceAgent->freeManifest(ptr); };
    std::unique_ptr<const char, decltype(deleter)> deviceAgentManifest{
        deviceAgent->manifest(&error), deleter};

    if (error != Error::noError)
    {
        NX_ERROR(
            this,
            lm("Can not fetch manifest for resource %1 (%2), plugin returned error.")
            .args(camera->getUserDefinedName(), camera->getId()));
        return std::make_pair(boost::none, boost::none);
    }

    if (!deviceAgentManifest.get())
    {
        NX_ERROR(this) << lm("Received null DeviceAgent manifest for plugin \"%1\".")
            .arg(engine->plugin()->name());
        return std::make_pair(boost::none, boost::none);
    }

    if (pluginsIni().analyticsManifestOutputPath[0])
    {
        saveManifestToFile(
            deviceAgentManifest.get(),
            "DeviceAgent",
            engine->plugin()->name(),
            lit("_device_agent"));
    }

    // TODO: Rewrite.
    // DeviceAgent::manifest() can return data in two json formats: either
    // DeviceAgentManifest or EngineManifest.
    // First we try AnalyticsDeviceManifest.
    auto deviceManifest = deserializeManifest<nx::vms::api::analytics::DeviceAgentManifest>(
        deviceAgentManifest.get());

    if (deviceManifest && !deviceManifest->supportedEventTypes.empty())
        return std::make_pair(deviceManifest, boost::none);

    // If manifest occurred to be not AnalyticsDeviceManifest, we try to treat it as
    // EngineManifest.
    auto driverManifest = deserializeManifest<nx::vms::api::analytics::EngineManifest>(
        deviceAgentManifest.get());

    if (driverManifest && driverManifest->outputEventTypes.size())
    {
        deviceManifest = nx::vms::api::analytics::DeviceAgentManifest();
        std::transform(
            driverManifest->outputEventTypes.cbegin(),
            driverManifest->outputEventTypes.cend(),
            std::back_inserter(deviceManifest->supportedEventTypes),
            [](const nx::vms::api::analytics::EventType& eventType) { return eventType.id; });
        return std::make_pair(deviceManifest, driverManifest);
    }

    // If manifest format is invalid.
    NX_ERROR(
        this,
        lm("Can not deserialize manifest for resource %1 (%2)")
        .args(camera->getUserDefinedName(), camera->getId()));
    return std::make_pair(boost::none, boost::none);
}

// TODO: #mshevchenko: Rename to addDataFromDeviceAgentManifestToCameraResource().
void Manager::addManifestToCamera(
    const nx::vms::api::analytics::DeviceAgentManifest& manifest,
    const QnSecurityCamResourcePtr& camera)
{
    camera->setSupportedAnalyticsEventTypeIds(manifest.supportedEventTypes);
    camera->saveParams();
}

bool Manager::cameraInfoFromResource(
    const QnSecurityCamResourcePtr& camera,
    CameraInfo* outCameraInfo) const
{
    NX_ASSERT(camera);
    NX_ASSERT(outCameraInfo);

    strncpy(
        outCameraInfo->vendor,
        camera->getVendor().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outCameraInfo->model,
        camera->getModel().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outCameraInfo->firmware,
        camera->getFirmware().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outCameraInfo->uid,
        camera->getId().toByteArray().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outCameraInfo->sharedId,
        camera->getSharedId().toStdString().c_str(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outCameraInfo->url,
        camera->getUrl().toUtf8().data(),
        CameraInfo::kTextParameterMaxLength);

    auto auth = camera->getAuth();
    strncpy(
        outCameraInfo->login,
        auth.user().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    strncpy(
        outCameraInfo->password,
        auth.password().toUtf8().data(),
        CameraInfo::kStringParameterMaxLength);

    outCameraInfo->channel = camera->getChannel();
    outCameraInfo->logicalId = camera->logicalId();

    return true;
}

void Manager::issueMissingUncompressedFrameWarningOnce()
{
    auto logLevel = nx::utils::log::Level::verbose;
    if (!m_uncompressedFrameWarningIssued)
    {
        m_uncompressedFrameWarningIssued = true;
        logLevel = nx::utils::log::Level::warning;
    }
    NX_UTILS_LOG(logLevel, this, "Uncompressed frame requested but not received.");
}

void Manager::putVideoFrame(
    const QnUuid& id,
    const QnConstCompressedVideoDataPtr& compressedFrame,
    const CLConstVideoDecoderOutputPtr& uncompressedFrame)
{
    if (!NX_ASSERT(compressedFrame))
        return;

    NX_VERBOSE(this, "putVideoFrame(\"%1\", compressedFrame, %2)", id,
        uncompressedFrame ? "uncompressedFrame" : "/*uncompressedFrame*/ nullptr");

    if (uncompressedFrame)
        m_visualMetadataDebugger->push(uncompressedFrame);
    else
        m_visualMetadataDebugger->push(compressedFrame);

    FrameConverter frameConverter(
        [&]() { return compressedFrame; },
        [&, this]()
        {
            if (!uncompressedFrame)
                issueMissingUncompressedFrameWarningOnce();
            return uncompressedFrame;
        });

    QnMutexLocker lock(&m_contextMutex);
    for (auto& deviceAgentContexts: m_contexts[id].deviceAgentContexts())
    {
        if (!deviceAgentContexts->isStreamConsumer())
            continue;

        if (DataPacket* const dataPacket = frameConverter.getDataPacket(
            pixelFormatFromEngineManifest(deviceAgentContexts->engineManifest())))
        {
            if (deviceAgentContexts->canAcceptData()
                && NX_ASSERT(dataPacket->timestampUsec() >= 0))
            {
                deviceAgentContexts->putData(std::make_shared<DataPacketAdapter>(dataPacket));
            }
            else
            {
                NX_INFO(this, "Skipped video frame for %1 from camera %2: queue overflow.",
                    deviceAgentContexts->engineManifest().pluginName.value, id);
            }
        }
        else
        {
            NX_VERBOSE(this, "Video frame not sent to DeviceAgent: see the log above.");
        }
    }
}

boost::optional<PixelFormat> Manager::pixelFormatFromEngineManifest(
    const nx::vms::api::analytics::EngineManifest& manifest)
{
    using Capability = nx::vms::api::analytics::EngineManifest::Capability;

    int uncompressedFrameCapabilityCount = 0; //< To check there is 0 or 1 of such capabilities.
    PixelFormat pixelFormat = PixelFormat::yuv420;
    auto pixelFormats = getAllPixelFormats(); //< To assert that all pixel formats are tested.

    auto checkCapability =
        [&](Capability value, PixelFormat correspondingPixelFormat)
        {
            if (manifest.capabilities.testFlag(value))
            {
                ++uncompressedFrameCapabilityCount;
                pixelFormat = correspondingPixelFormat;
            }

            // Delete the pixel format which has been tested.
            auto it = std::find(pixelFormats.begin(), pixelFormats.end(), correspondingPixelFormat);
            NX_ASSERT(it != pixelFormats.end());
            pixelFormats.erase(it);
        };

    checkCapability(Capability::needUncompressedVideoFrames_yuv420, PixelFormat::yuv420);
    checkCapability(Capability::needUncompressedVideoFrames_argb, PixelFormat::argb);
    checkCapability(Capability::needUncompressedVideoFrames_abgr, PixelFormat::abgr);
    checkCapability(Capability::needUncompressedVideoFrames_rgba, PixelFormat::rgba);
    checkCapability(Capability::needUncompressedVideoFrames_bgra, PixelFormat::bgra);
    checkCapability(Capability::needUncompressedVideoFrames_rgb, PixelFormat::rgb);
    checkCapability(Capability::needUncompressedVideoFrames_bgr, PixelFormat::bgr);

    NX_ASSERT(pixelFormats.empty());

    NX_ASSERT(uncompressedFrameCapabilityCount >= 0);
    if (uncompressedFrameCapabilityCount > 1)
    {
        NX_ERROR(this) << lm(
            "More than one needUncompressedVideoFrames_... capability found"
            "in Engine manifest of analytics plugin \"%1\"").arg(manifest.pluginId);
    }
    if (uncompressedFrameCapabilityCount != 1)
        return boost::optional<PixelFormat>{};

    return boost::optional<PixelFormat>(pixelFormat);
}

QWeakPointer<VideoDataReceptor> Manager::createVideoDataReceptor(const QnUuid& id)
{
    QnMutexLocker lock(&m_contextMutex);
    auto& context = m_contexts[id];

    // Collect pixel formats required by at least one DeviceAgent.
    VideoDataReceptor::NeededUncompressedPixelFormats neededUncompressedPixelFormats;
    bool needsCompressedFrames = false;
    for (auto& deviceAgentContext: m_contexts[id].deviceAgentContexts())
    {
        nxpt::ScopedRef<ConsumingDeviceAgent> consumingDeviceAgent(
            deviceAgentContext->deviceAgent()->queryInterface(IID_ConsumingDeviceAgent));
        if (!consumingDeviceAgent)
            continue;

        const boost::optional<PixelFormat> pixelFormat =
            pixelFormatFromEngineManifest(deviceAgentContext->engineManifest());
        if (!pixelFormat)
            needsCompressedFrames = true;
        else
            neededUncompressedPixelFormats.insert(pixelFormat.get());
    }

    auto videoDataReceptor = VideoDataReceptorPtr(new VideoDataReceptor(
        [this, id](
            const QnConstCompressedVideoDataPtr& compressedFrame,
            const CLConstVideoDecoderOutputPtr& uncompressedFrame)
        {
            if (!NX_ASSERT(compressedFrame))
                return;
            putVideoFrame(id, compressedFrame, uncompressedFrame);
        },
        needsCompressedFrames,
        neededUncompressedPixelFormats));

    context.setVideoDataReceptor(videoDataReceptor);
    return videoDataReceptor.toWeakRef();
}

void Manager::registerDataReceptor(
    const QnResourcePtr& resource,
    QWeakPointer<QnAbstractDataReceptor> dataReceptor)
{
    QnMutexLocker lock(&m_contextMutex);
    const auto& id = resource->getId();

    auto& context = m_contexts[id];
    context.setMetadataDataReceptor(dataReceptor);
}

} // namespace analytics
} // namespace mediaserver
} // namespace nx

namespace nx {
namespace sdk {

QString toString(const nx::sdk::CameraInfo& cameraInfo)
{
    return lm(
        "Vendor: %1, Model: %2, Firmware: %3, UID: %4, Shared ID: %5, URL: %6, Channel: %7")
        .args(
            cameraInfo.vendor,
            cameraInfo.model,
            cameraInfo.firmware,
            cameraInfo.uid,
            cameraInfo.sharedId,
            cameraInfo.url,
            cameraInfo.channel).toQString();
}

} // namespace sdk
} // namespace nx
