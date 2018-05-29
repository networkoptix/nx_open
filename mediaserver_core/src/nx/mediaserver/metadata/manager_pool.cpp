#include "manager_pool.h"

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

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/model_functions.h>
#include <nx/mediaserver/metadata/metadata_handler.h>
#include <nx/mediaserver/metadata/event_rule_watcher.h>
#include <nx/mediaserver/metadata/plugin_setting.h>

#include <nx/api/analytics/device_manifest.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <nx/sdk/metadata/consuming_camera_manager.h>
#include <nx/debugging/visual_metadata_debugger_factory.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/log/log_main.h>
#include <mediaserver_ini.h>
#include <plugins/plugins_ini.h>
#include <nx/sdk/metadata/pixel_format.h>

#include "video_data_receptor.h"
#include "yuv420_uncompressed_video_frame.h"
#include "generic_uncompressed_video_frame.h"
#include "wrapping_compressed_video_packet.h"

namespace nx {
namespace api {

uint qHash(const Analytics::EventType& t)
{
    return qHash(t.typeId.toByteArray());
}

} // namespace api
} // namespace nx

namespace nx {
namespace mediaserver {
namespace metadata {

using namespace nx::sdk;
using namespace nx::sdk::metadata;
using namespace nx::debugging;
using PixelFormat = UncompressedVideoFrame::PixelFormat;

ManagerPool::ManagerPool(QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule),
    m_visualMetadataDebugger(
        VisualMetadataDebuggerFactory::makeDebugger(DebuggerType::managerPool)),
    m_thread(new QThread(this))
{
    m_thread->setObjectName(lit("MetadataManagerPool"));
    moveToThread(m_thread);
    m_thread->start();
}

ManagerPool::~ManagerPool()
{
    NX_DEBUG(this, lit("Destroying metadata manager pool."));
    stop();
}

void ManagerPool::stop()
{
    disconnect(this);
    m_thread->quit();
    m_thread->wait();
    m_contexts.clear();
}

void ManagerPool::init()
{
    NX_DEBUG(this, lit("Initializing metadata manager pool."));
    auto resourcePool = commonModule()->resourcePool();

    connect(
        resourcePool, &QnResourcePool::resourceAdded,
        this, &ManagerPool::at_resourceAdded);

    connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        this, &ManagerPool::at_resourceRemoved);

    connect(
        serverModule()->metadataRuleWatcher(), &EventRuleWatcher::rulesUpdated,
        this, &ManagerPool::at_rulesUpdated);

    QMetaObject::invokeMethod(this, "initExistingResources");
}

void ManagerPool::initExistingResources()
{
    auto resourcePool = commonModule()->resourcePool();
    const auto mediaServer = resourcePool->getResourceById<QnMediaServerResource>(
        commonModule()->moduleGUID());

    const auto cameras = resourcePool->getAllCameras(
        mediaServer,
        /*ignoreDesktopCamera*/ true);

    for (const auto& camera: cameras)
        at_resourceAdded(camera);
}

void ManagerPool::at_resourceAdded(const QnResourcePtr& resource)
{
    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been added.")
            .args(resource->getName(), resource->getId()));

    connect(
        resource.data(), &QnResource::statusChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::parentIdChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::urlChanged,
        this, &ManagerPool::handleResourceChanges);

    connect(
        resource.data(), &QnResource::propertyChanged,
        this, &ManagerPool::at_propertyChanged);

    handleResourceChanges(resource);
}

void ManagerPool::at_propertyChanged(const QnResourcePtr& resource, const QString& name)
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

void ManagerPool::at_resourceRemoved(const QnResourcePtr& resource)
{
    NX_VERBOSE(
        this,
        lm("Resource %1 (%2) has been removed.")
            .args(resource->getName(), resource->getId()));

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return;

    QnMutexLocker lock(&m_contextMutex);
    m_contexts.erase(resource->getId());
}

void ManagerPool::at_rulesUpdated(const QSet<QnUuid>& affectedResources)
{
    NX_VERBOSE(
        this,
        lm("Rules have been updated. Affected resources: %1").arg(affectedResources));

    for (const auto& resourceId: affectedResources)
    {
        auto resource = commonModule()->resourcePool()->getResourceById(resourceId);
        handleResourceChanges(resource);
    }
}

ManagerPool::PluginList ManagerPool::availablePlugins() const
{
    auto pluginManager = serverModule()->pluginManager();
    NX_ASSERT(pluginManager, lit("Cannot access PluginManager instance"));
    if (!pluginManager)
        return PluginList();

    return pluginManager->findNxPlugins<Plugin>(IID_Plugin);
}

static void freeSettings(std::vector<nxpl::Setting>* settings)
{
    for (auto& setting: *settings)
    {
        free(setting.name);
        free(setting.value);
    }
}

/** @return Empty settings if the file does not exist, or on error. */
std::vector<nxpl::Setting> ManagerPool::loadSettingsFromFile(
    const QString& fileDescription, const QString& filename)
{
    using nx::utils::log::Level;
    auto log = //< Can be used to return empty settings: return log(...)
        [&](Level level, const QString& message)
        {
            NX_UTILS_LOG(level, this) << lm("Metadata %1 settings: %2: [%3]")
                .args(fileDescription, message, filename);
            return std::vector<nxpl::Setting>{};
        };

    if (!QFileInfo::exists(filename))
        return log(Level::info, lit("File does not exist"));

    log(Level::info, lit("Loading from file"));

    QFile f(filename);
    if (!f.open(QFile::ReadOnly))
        return log(Level::error, lit("Unable to open file"));

    const QString& settingsStr = f.readAll();
    if (settingsStr.isEmpty())
        return log(Level::error, lit("Unable to read from file"));

    bool success = false;
    const auto& settingsFromJson = QJson::deserialized<QList<PluginSetting>>(
        settingsStr.toUtf8(), /*defaultValue*/ {}, &success);
    if (!success)
        return log(Level::error, lit("Invalid JSON in file"));

    std::vector<nxpl::Setting> settings(settingsFromJson.size());
    for (auto i = 0; i < settingsFromJson.size(); ++i)
    {
        // Memory will be deallocated by freeSettings().
        settings[i].name = strdup(settingsFromJson.at(i).name.toUtf8().data());
        settings[i].value = strdup(settingsFromJson.at(i).value.toUtf8().data());
    }
    return settings;
}

/** @return Dir ending with "/", intended to receive manifest files. */
static QString manifestFileDir()
{
    QString dir = QDir::cleanPath( //< Normalize to use forward slashes, as required by QFile.
        QString::fromUtf8(pluginsIni().metadataPluginManifestOutputPath));

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
void ManagerPool::saveManifestToFile(
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

void ManagerPool::setCameraManagerDeclaredSettings(
    CameraManager* manager,
    const QnSecurityCamResourcePtr& /*camera*/,
    const QString& pluginLibName)
{
    // TODO: Stub. Implement storing the settings in the database.
    auto settings = loadSettingsFromFile(lit("Plugin Camera Manager"), settingsFilename(
        pluginsIni().metadataPluginCameraManagerSettingsPath,
        pluginLibName, lit("_camera_manager")));
    manager->setDeclaredSettings(
        settings.empty() ? nullptr : &settings.front(), (int) settings.size());
    freeSettings(&settings);
}

void ManagerPool::setPluginDeclaredSettings(Plugin* plugin, const QString& pluginLibName)
{
    // TODO: Stub. Implement storing the settings in the database.
    auto settings = loadSettingsFromFile(lit("Plugin"), settingsFilename(
        pluginsIni().metadataPluginSettingsPath, pluginLibName));
    plugin->setDeclaredSettings(
        settings.empty() ? nullptr : &settings.front(), (int) settings.size());
    freeSettings(&settings);
}

void ManagerPool::createCameraManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera)
{
    if (ini().enablePersistentMetadataManager)
    {
        const auto itr = m_contexts.find(camera->getId());
        if (itr != m_contexts.cend())
        {
            if (!itr->second.managers().empty())
                return;
        }
    }
    QnMediaServerResourcePtr server = commonModule()->resourcePool()
        ->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    NX_ASSERT(server, lm("Can not obtain current server resource."));
    if (!server)
        return;

    for (Plugin* const plugin: availablePlugins())
    {
        const QString& pluginLibName = serverModule()->pluginManager()->pluginLibName(plugin);

        // TODO: Consider assigning plugin settings earlier.
        setPluginDeclaredSettings(plugin, pluginLibName);

        nxpt::ScopedRef<Plugin> pluginGuard(plugin, /*increaseRef*/ false);

        nxpt::ScopedRef<CameraManager> manager(
            createCameraManager(camera, plugin), /*increaseRef*/ false);
        if (!manager)
            continue;
        boost::optional<nx::api::AnalyticsDriverManifest> pluginManifest =
            loadPluginManifest(plugin);
        if (!pluginManifest)
            continue; //< The error is already logged.
        assignPluginManifestToServer(*pluginManifest, server);

        boost::optional<nx::api::AnalyticsDeviceManifest> managerManifest;
        boost::optional<nx::api::AnalyticsDriverManifest> auxiliaryPluginManifest;
        std::tie(managerManifest, auxiliaryPluginManifest) =
            loadManagerManifest(plugin, manager.get(), camera);
        if (managerManifest)
        {
            // TODO: Fix: Camera property should receive a union of data from all plugins.
            addManifestToCamera(*managerManifest, camera);
        }
        if (auxiliaryPluginManifest)
        {
            auxiliaryPluginManifest->driverId = pluginManifest->driverId;
            pluginManifest = mergePluginManifestToServer(*auxiliaryPluginManifest, server);
        }

        setCameraManagerDeclaredSettings(manager.get(), camera, pluginLibName);

        auto& context = m_contexts[camera->getId()];
        std::unique_ptr<MetadataHandler> handler(
            createMetadataHandler(camera, *pluginManifest));

        if (auto consumingCameraManager = nxpt::ScopedRef<CameraManager>(
            manager->queryInterface(IID_CameraManager)))
        {
            handler->registerDataReceptor(&context);
            handler->setVisualDebugger(m_visualMetadataDebugger.get());
        }

        context.addManager(std::move(manager), std::move(handler), *pluginManifest);
    }
}

CameraManager* ManagerPool::createCameraManager(
    const QnSecurityCamResourcePtr& camera,
    Plugin* plugin) const
{
    NX_ASSERT(camera && plugin);
    if (!camera || !plugin)
        return nullptr;

    NX_DEBUG(
        this,
        lm("Creating metadata manager for resource %1 (%2).")
            .args(camera->getUserDefinedName(), camera->getId()));

    CameraInfo cameraInfo;
    bool success = cameraInfoFromResource(camera, &cameraInfo);
    if (!success)
    {
        NX_WARNING(
            this,
            lm("Cannot create resource info from resource %1 (%2)")
                .args(camera->getUserDefinedName(), camera->getId()));
        return nullptr;
    }

    NX_DEBUG(
        this,
        lm("Resource info for resource %1 (%2): %3")
        .args(camera->getUserDefinedName(), camera->getId(), cameraInfo));

    Error error = Error::noError;
    return plugin->obtainCameraManager(cameraInfo, &error);
}

void ManagerPool::releaseResourceCameraManagersUnsafe(const QnSecurityCamResourcePtr& camera)
{
    if (ini().enablePersistentMetadataManager)
        return;

    auto& context = m_contexts[camera->getId()];
    context.clearManagers();
    context.setManagersInitialized(false);
}

MetadataHandler* ManagerPool::createMetadataHandler(
    const QnResourcePtr& resource,
    const nx::api::AnalyticsDriverManifest& manifest)
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

    auto handler = new MetadataHandler();
    handler->setResource(camera);
    handler->setManifest(manifest);

    return handler;
}

void ManagerPool::handleResourceChanges(const QnResourcePtr& resource)
{
    NX_VERBOSE(
        this,
        lm("Handling resource changes for resource %1 (%2)")
            .args(resource->getName(), resource->getId()));

    auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
    {
        return;
    }

    {
        NX_DEBUG(
            this,
            lm("Creating metadata managers for resource %1 (%2).")
                .args(camera->getUserDefinedName(), camera->getId()));

    }
    auto resourceId = camera->getId();
    QnMutexLocker lock(&m_contextMutex);
    auto& context = m_contexts[resourceId];
    if (isCameraAlive(camera))
    {
        if (!context.isManagerInitialized())
            createCameraManagersForResourceUnsafe(camera);
        auto events = serverModule()->metadataRuleWatcher()->watchedEventsForResource(resourceId);
        fetchMetadataForResourceUnsafe(resourceId, context, events);
    }
    else
    {
        releaseResourceCameraManagersUnsafe(camera);
    }
}

bool ManagerPool::isCameraAlive(const QnSecurityCamResourcePtr& camera) const
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

void ManagerPool::fetchMetadataForResourceUnsafe(
    const QnUuid& resourceId,
    ResourceMetadataContext& context,
    QSet<QnUuid>& eventTypeIds)
{
    for (auto& data: context.managers())
    {
        if (eventTypeIds.empty() && !data.isStreamConsumer)
        {
            NX_DEBUG(
                this,
                lm("Event list is empty, stopping metadata fetching for resource %1.")
                    .arg(resourceId));

            if (data.manager->stopFetchingMetadata() != Error::noError)
            {
                NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1")
                    .arg(data.manifest.driverName.value));
            }
        }
        else
        {
            NX_DEBUG(this, lm("Statopping metadata fetching for resource %1").args(resourceId));
            if (data.manager->stopFetchingMetadata() != Error::noError)
            {
                NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1")
                    .arg(data.manifest.driverName.value));
            }

            NX_DEBUG(this, lm("Starting metadata fetching for resource %1. Event list is %2")
                .args(resourceId, eventTypeIds));

            std::vector<nxpl::NX_GUID> eventTypeList;
            for (const auto& eventTypeId: eventTypeIds)
                eventTypeList.push_back(nxpt::NxGuidHelper::fromRawData(eventTypeId.toRfc4122()));
            auto result = data.manager->startFetchingMetadata(
                !eventTypeList.empty() ? &eventTypeList[0] : nullptr,
                static_cast<int>(eventTypeList.size()));

            if (result != Error::noError)
                NX_WARNING(this, lm("Failed to stop fetching metadata from plugin %1").arg(data.manifest.driverName.value));
        }
    }
}

uint qHash(const nx::api::Analytics::EventType& t)// noexcept
{
    return qHash(t.typeId.toByteArray());
}

boost::optional<nx::api::AnalyticsDriverManifest> ManagerPool::loadPluginManifest(
    Plugin* plugin)
{
    Error error = Error::noError;
    // TODO: #mike: Consider a dedicated mechanism for localization.
    // TODO: #mike: Refactor GUIDs to be string-based hierarchical ids (e.g. "nx.eventType.LineCrossing").

    const char* const manifestStr = plugin->capabilitiesManifest(&error);
    if (error != Error::noError)
    {
        NX_ERROR(this) << lm("Unable to receive Plugin manifest for plugin \"%1\": %2.")
            .args(plugin->name(), error);
        return boost::none;
    }
    if (manifestStr == nullptr)
    {
        NX_ERROR(this) << lm("Received null Plugin manifest for plugin \"%1\".")
            .arg(plugin->name());
        return boost::none;
    }

    if (pluginsIni().metadataPluginManifestOutputPath[0])
    {
        saveManifestToFile(
            manifestStr, "Plugin", qnServerModule->pluginManager()->pluginLibName(plugin));
    }

    auto pluginManifest = deserializeManifest<nx::api::AnalyticsDriverManifest>(manifestStr);

    if (!pluginManifest)
    {
        NX_ERROR(this) << lm(
            "Cannot deserialize Plugin manifest from plugin \"%1\"").arg(plugin->name());
        return boost::none; //< Error already logged.
    }
    return pluginManifest;
}

void ManagerPool::assignPluginManifestToServer(
    const nx::api::AnalyticsDriverManifest& manifest,
    const QnMediaServerResourcePtr& server)
{
    auto existingManifests = server->analyticsDrivers();
    auto it = std::find_if(existingManifests.begin(), existingManifests.end(),
        [&manifest](const nx::api::AnalyticsDriverManifest& m)
        {
            return m.driverId == manifest.driverId;
        });

    if (it == existingManifests.cend())
        existingManifests.push_back(manifest);
    else
        *it = manifest;

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();
}

nx::api::AnalyticsDriverManifest ManagerPool::mergePluginManifestToServer(
    const nx::api::AnalyticsDriverManifest& manifest,
    const QnMediaServerResourcePtr& server)
{
    nx::api::AnalyticsDriverManifest* result = nullptr;
    auto existingManifests = server->analyticsDrivers();
    auto it = std::find_if(existingManifests.begin(), existingManifests.end(),
        [&manifest](const nx::api::AnalyticsDriverManifest& m)
        {
            return m.driverId == manifest.driverId;
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

#if defined _DEBUG
    // Sometimes in debug purposes we need do clean existingManifest.outputEventTypes list.
    if (!manifest.outputEventTypes.empty() &&
        manifest.outputEventTypes.front().typeId == nx::api::kResetPluginManifestEventId)
    {
        it->outputEventTypes.clear();
    }
#endif

    server->setAnalyticsDrivers(existingManifests);
    server->saveParams();
    NX_ASSERT(result);
    return *result;
}

std::pair<
    boost::optional<nx::api::AnalyticsDeviceManifest>,
    boost::optional<nx::api::AnalyticsDriverManifest>
>
ManagerPool::loadManagerManifest(
    const Plugin* plugin,
    CameraManager* manager,
    const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(manager);
    NX_ASSERT(camera);

    Error error = Error::noError;

    // "managerManifest" contains const char* representation of camera manifest.
    // unique_ptr allows us to automatically ask manager to release it when memory is not needed more.
    auto deleter = [manager](const char* ptr) { manager->freeManifest(ptr); };
    std::unique_ptr<const char, decltype(deleter)> managerManifest{
        manager->capabilitiesManifest(&error), deleter};

    if (error != Error::noError)
    {
        NX_ERROR(
            this,
            lm("Can not fetch manifest for resource %1 (%2), plugin returned error.")
            .args(camera->getUserDefinedName(), camera->getId()));
        return std::make_pair(boost::none, boost::none);
    }

    if (!managerManifest.get())
    {
        NX_ERROR(this) << lm("Received null Plugin Camera Manager manifest for plugin \"%1\".")
            .arg(plugin->name());
        return std::make_pair(boost::none, boost::none);
    }

    if (pluginsIni().metadataPluginManifestOutputPath[0])
    {
        saveManifestToFile(
            managerManifest.get(),
            "Plugin Camera Manager",
            qnServerModule->pluginManager()->pluginLibName(plugin),
            lit("_camera_manager"));
    }

    // Manager::capabilitiesManifest can return data in two json formats: either
    // AnalyticsDeviceManifest or AnalyticsDriverManifest.
    // First we try AnalyticsDeviceManifest.
    auto deviceManifest = deserializeManifest<nx::api::AnalyticsDeviceManifest>(
        managerManifest.get());

    if (deviceManifest && !deviceManifest->supportedEventTypes.empty())
        return std::make_pair(deviceManifest, boost::none);

    // If manifest occurred to be not AnalyticsDeviceManifest, we try to treat it as
    // AnalyticsDriverManifest.
    auto driverManifest = deserializeManifest<nx::api::AnalyticsDriverManifest>(
        managerManifest.get());

    if (driverManifest && driverManifest->outputEventTypes.size())
    {
        deviceManifest = nx::api::AnalyticsDeviceManifest();
        std::transform(
            driverManifest->outputEventTypes.cbegin(),
            driverManifest->outputEventTypes.cend(),
            std::back_inserter(deviceManifest->supportedEventTypes),
            [](const nx::api::Analytics::EventType& driverManifestElement)
            {
                return driverManifestElement.typeId;
            });
        return std::make_pair(deviceManifest, driverManifest);
    }

    // If manifest format is invalid.
    NX_ERROR(
        this,
        lm("Can not deserialize manifest for resource %1 (%2)")
        .args(camera->getUserDefinedName(), camera->getId()));
    return std::make_pair(boost::none, boost::none);
}

// TODO: #mshevchenko: Rename to addDataFromCameraManagerManifestToCameraResource().
void ManagerPool::addManifestToCamera(
    const nx::api::AnalyticsDeviceManifest& manifest,
    const QnSecurityCamResourcePtr& camera)
{
    camera->setAnalyticsSupportedEvents(manifest.supportedEventTypes);
    camera->saveParams();
}

bool ManagerPool::cameraInfoFromResource(
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

    // If getLogicalId() returns incorrect number, logicalId is set to 0.
    outCameraInfo->logicalId = atoi(camera->getLogicalId().toStdString().c_str());

    return true;
}

void ManagerPool::warnOnce(bool* warningIssued, const QString& message)
{
    if (!*warningIssued)
    {
        *warningIssued = true;
        NX_WARNING(this) << message;
    }
    else
    {
        NX_VERBOSE(this) << message;
    }
}

void ManagerPool::putVideoFrame(
    const QnUuid& id,
    const QnConstCompressedVideoDataPtr& compressedFrame,
    const CLConstVideoDecoderOutputPtr& uncompressedFrame)
{
    NX_VERBOSE(this) << lm("putVideoFrame(\"%1\", %2, %3)").args(
        id,
        compressedFrame ? "compressedFrame" : "/*compressedFrame*/ nullptr",
        uncompressedFrame ? "uncompressedFrame" : "/*uncompressedFrame*/ nullptr");

    if (uncompressedFrame)
        m_visualMetadataDebugger->push(uncompressedFrame);
    else
        m_visualMetadataDebugger->push(compressedFrame);

    std::map<PixelFormat, nxpt::ScopedRef<UncompressedVideoFrame>> rgbFrames;
    nxpt::ScopedRef<Yuv420UncompressedVideoFrame> yuv420Frame; //< Will be created on first need.

    QnMutexLocker lock(&m_contextMutex);
    for (auto& managerData: m_contexts[id].managers())
    {
        nxpt::ScopedRef<ConsumingCameraManager> manager(
            managerData.manager->queryInterface(IID_ConsumingCameraManager));
        if (!manager)
            continue;

        DataPacket* dataPacket = nullptr;

        const auto pixelFormat = pixelFormatFromManifest(managerData.manifest);
        if (!pixelFormat)
        {
            if (compressedFrame)
            {
                dataPacket = new WrappingCompressedVideoPacket(compressedFrame);
            }
            else
            {
                warnOnce(&m_compressedFrameWarningIssued,
                    lm("Compressed frame requested but not received."));
            }
        }
        else
        {
            if (uncompressedFrame)
            {
                if (pixelFormat.get() == PixelFormat::yuv420)
                {
                    if (yuv420Frame.get() == nullptr)
                        yuv420Frame.reset(new Yuv420UncompressedVideoFrame(uncompressedFrame));
                    yuv420Frame->addRef();
                    dataPacket = yuv420Frame.get();
                }
                else
                {
                    auto it = rgbFrames.find(pixelFormat.get());
                    if (it == rgbFrames.cend())
                    {
                        auto insertResult = rgbFrames.insert(std::make_pair(pixelFormat.get(),
                            nxpt::ScopedRef<UncompressedVideoFrame>(
                                videoDecoderOutputToUncompressedVideoFrame(
                                    uncompressedFrame, pixelFormat.get()))));
                        NX_ASSERT(insertResult.second);
                        it = insertResult.first;
                    }
                    it->second->addRef();
                    dataPacket = it->second.get();
                }
            }
            else
            {
                warnOnce(&m_uncompressedFrameWarningIssued,
                    lm("Uncompressed frame requested but not received."));
            }
        }

        if (dataPacket)
        {
            manager->pushDataPacket(dataPacket);
            dataPacket->releaseRef();
        }
        else
        {
            NX_VERBOSE(this) << lm("Video frame not sent to CameraManager.");
        }
    }
}

boost::optional<PixelFormat> ManagerPool::pixelFormatFromManifest(
    const nx::api::AnalyticsDriverManifest& manifest)
{
    using Capability = api::AnalyticsDriverManifestBase::Capability;

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
            "More than one needUncompressedVideoFrames_... capability found in manifest of \"%1\"")
            .arg(manifest.driverId);
    }
    if (uncompressedFrameCapabilityCount != 1)
        return boost::optional<PixelFormat>{};

    return boost::optional<PixelFormat>(pixelFormat);
}

QWeakPointer<VideoDataReceptor> ManagerPool::createVideoDataReceptor(const QnUuid& id)
{
    QnMutexLocker lock(&m_contextMutex);
    auto& context = m_contexts[id];

    // Collect pixel formats required by at least one CameraManager.
    VideoDataReceptor::NeededUncompressedPixelFormats neededUncompressedPixelFormats;
    bool needsCompressedFrames = false;
    for (auto& managerData: m_contexts[id].managers())
    {
        nxpt::ScopedRef<ConsumingCameraManager> manager(
            managerData.manager->queryInterface(IID_ConsumingCameraManager));
        if (!manager)
            continue;

        boost::optional<PixelFormat> pixelFormat =
            pixelFormatFromManifest(managerData.manifest);
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
            putVideoFrame(id, compressedFrame, uncompressedFrame);
        },
        needsCompressedFrames,
        neededUncompressedPixelFormats));

    context.setVideoDataReceptor(videoDataReceptor);
    return videoDataReceptor.toWeakRef();
}

void ManagerPool::registerDataReceptor(
    const QnResourcePtr& resource,
    QWeakPointer<QnAbstractDataReceptor> dataReceptor)
{
    QnMutexLocker lock(&m_contextMutex);
    const auto& id = resource->getId();

    auto& context = m_contexts[id];
    context.setMetadataDataReceptor(dataReceptor);
}

/**
 * Converts rgb-like pixel formats. Asserts that pixelFormat is a supported RGB-based format.
 */
/*static*/ AVPixelFormat ManagerPool::rgbToAVPixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
        case PixelFormat::argb: return AV_PIX_FMT_ARGB;
        case PixelFormat::abgr: return AV_PIX_FMT_ABGR;
        case PixelFormat::rgba: return AV_PIX_FMT_RGBA;
        case PixelFormat::bgra: return AV_PIX_FMT_BGRA;
        case PixelFormat::rgb: return AV_PIX_FMT_RGB24;
        case PixelFormat::bgr: return AV_PIX_FMT_BGR24;

        default:
            NX_ASSERT(false, lm(
                "Unsupported nx::sdk::metadata::UncompressedVideoFrame::PixelFormat \"%1\" = %2")
                .args(pixelFormatToStdString(pixelFormat), (int) pixelFormat));
            return AV_PIX_FMT_NONE;
    }
}

/*static*/ UncompressedVideoFrame* ManagerPool::videoDecoderOutputToUncompressedVideoFrame(
    const CLConstVideoDecoderOutputPtr& frame, PixelFormat pixelFormat)
{
    // TODO: Consider supporting other decoded video frame formats.
    const auto expectedFormat = AV_PIX_FMT_YUV420P;
    if (frame->format != expectedFormat)
    {
        NX_ERROR(typeid(VideoDataReceptor)) << lm(
            "Decoded frame has AVPixelFormat %1 instead of %2; ignoring")
            .args(frame->format, expectedFormat);
        return nullptr;
    }

    if (pixelFormat == PixelFormat::yuv420)
        return new Yuv420UncompressedVideoFrame(frame);

    const AVPixelFormat avPixelFormat = rgbToAVPixelFormat(pixelFormat);

    std::vector<std::vector<char>> data(1);
    std::vector<int> lineSize(1);
    data[0] = frame->toRgb(&lineSize[0], avPixelFormat);

    return new GenericUncompressedVideoFrame(
        frame->pkt_dts, frame->width, frame->height, pixelFormat,
        std::move(data), std::move(lineSize));
}

} // namespace metadata
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
