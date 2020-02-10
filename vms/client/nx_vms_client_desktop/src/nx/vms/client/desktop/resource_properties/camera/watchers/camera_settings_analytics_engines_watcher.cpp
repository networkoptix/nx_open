#include "camera_settings_analytics_engines_watcher.h"

#include <algorithm>

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <utils/common/connective.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../data/analytics_engine_info.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

AnalyticsEngineInfo engineInfoFromResource(const AnalyticsEngineResourcePtr& engine)
{
    const auto plugin = engine->getParentResource().dynamicCast<AnalyticsPluginResource>();
    const auto pluginManifest = NX_ASSERT(plugin)
        ? plugin->manifest()
        : api::analytics::PluginManifest{};

    return AnalyticsEngineInfo {
        engine->getId(),
        engine->getName(),
        pluginManifest.description,
        pluginManifest.version,
        pluginManifest.vendor,
        engine->manifest().deviceAgentSettingsModel,
        engine->isDeviceDependent()
    };
}

} // namespace

class CameraSettingsAnalyticsEnginesWatcher::Private:
    public Connective<QObject>,
    public QnCommonModuleAware
{
public:
    Private(QnCommonModule* commonModule, CameraSettingsDialogStore* store);

    void setCamera(const QnVirtualCameraResourcePtr& camera);
    void updateStore();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_enginePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_engineManifestChanged(const AnalyticsEngineResourcePtr& engine);
    void at_connectionOpened();
    void at_connectionClosed();

public:
    const QPointer<CameraSettingsDialogStore> store;
    QnVirtualCameraResourcePtr camera;
    QHash<QnUuid, AnalyticsEngineInfo> engines;
    bool m_online = false;
};

CameraSettingsAnalyticsEnginesWatcher::Private::Private(
    QnCommonModule* commonModule,
    CameraSettingsDialogStore* store)
    :
    QnCommonModuleAware(commonModule),
    store(store)
{
    const auto messageProcessor =
        qobject_cast<QnClientMessageProcessor*>(commonModule->messageProcessor());

    if (!NX_ASSERT(messageProcessor))
        return;

    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived,
        this, &Private::at_connectionOpened);
    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed,
        this, &Private::at_connectionClosed);

    connect(commonModule->resourcePool(), &QnResourcePool::resourceAdded,
        this, &Private::at_resourceAdded);
    connect(commonModule->resourcePool(), &QnResourcePool::resourceRemoved,
        this, &Private::at_resourceRemoved);

    if (messageProcessor->connectionStatus()->state() == QnConnectionState::Ready)
        at_connectionOpened();
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_connectionOpened()
{
    NX_ASSERT(!m_online);
    m_online = true;

    engines.clear();

    for (const auto& engine: commonModule()->resourcePool()->getResources<AnalyticsEngineResource>())
        at_resourceAdded(engine);
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_connectionClosed()
{
    NX_ASSERT(m_online);
    m_online = false;
}

void CameraSettingsAnalyticsEnginesWatcher::Private::setCamera(
    const QnVirtualCameraResourcePtr& newCamera)
{
    if (camera == newCamera)
        return;

    if (camera)
        camera->disconnect(this);

    camera = newCamera;

    if (camera)
    {
        connect(camera, &QnResource::parentIdChanged, this, &Private::updateStore);

        connect(camera, &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& resource, const QString& key)
            {
                if (resource == camera && key == QnVirtualCameraResource::kAnalyzedStreamIndexes)
                    updateStore();
            });
    }

    updateStore();
}

void CameraSettingsAnalyticsEnginesWatcher::Private::updateStore()
{
    if (!store)
        return;

    if (!camera)
    {
        store->setAnalyticsEngines({});
        return;
    }

    auto enginesList = engines.values();
    const auto compatibleEngines = camera->compatibleAnalyticsEngines();

    const auto isIncompatibleEngine =
        [compatibleEngines](const AnalyticsEngineInfo& engineInfo)
        {
            return !compatibleEngines.contains(engineInfo.id);
        };

    enginesList.erase(
        std::remove_if(enginesList.begin(), enginesList.end(), isIncompatibleEngine),
        enginesList.end());

    std::sort(enginesList.begin(), enginesList.end(),
        [](const auto& left, const auto& right) { return left.name < right.name; });

    store->setAnalyticsEngines(enginesList);

    for (const auto& engine: enginesList)
    {
        store->setAnalyticsStreamIndex(engine.id, camera->analyzedStreamIndex(engine.id),
            ModificationSource::remote);
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_resourceAdded(
    const QnResourcePtr& resource)
{
    if (!m_online)
        return;

    if (const auto& engine = resource.objectCast<AnalyticsEngineResource>())
    {
        const auto info = engineInfoFromResource(engine);
        if (info.id.isNull())
            return;

        engines[info.id] = info;

        connect(engine, &QnResource::nameChanged,
            this, &Private::at_engineNameChanged);
        connect(engine, &QnResource::propertyChanged,
            this, &Private::at_enginePropertyChanged);
        connect(engine, &AnalyticsEngineResource::manifestChanged,
            this, &Private::at_engineManifestChanged);

        updateStore();
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_resourceRemoved(
    const QnResourcePtr& resource)
{
    if (const auto& engine = resource.objectCast<AnalyticsEngineResource>())
    {
        engine->disconnect(this);
        if (engines.remove(engine->getId()) > 0)
            updateStore();
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_engineNameChanged(
    const QnResourcePtr& resource)
{
    const auto it = engines.find(resource->getId());
    if (it == engines.end())
        return;

    it->name = resource->getName();
    updateStore();
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_enginePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (!store)
        return;

    const auto engine = resource.staticCast<AnalyticsEngineResource>();

    if (key == AnalyticsEngineResource::kSettingsValuesProperty)
    {
        store->setDeviceAgentSettingsValues(engine->getId(), engine->settingsValues());
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_engineManifestChanged(
    const AnalyticsEngineResourcePtr& engine)
{
    if (const auto it = engines.find(engine->getId()); it != engines.end())
    {
        it->settingsModel = engine->manifest().deviceAgentSettingsModel;
        it->isDeviceDependent = engine->isDeviceDependent();
        updateStore();
    }
}

CameraSettingsAnalyticsEnginesWatcher::CameraSettingsAnalyticsEnginesWatcher(
    CameraSettingsDialogStore* store, QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(commonModule(), store))
{
}

CameraSettingsAnalyticsEnginesWatcher::~CameraSettingsAnalyticsEnginesWatcher()
{
}

QnVirtualCameraResourcePtr CameraSettingsAnalyticsEnginesWatcher::camera() const
{
    return d->camera;
}

void CameraSettingsAnalyticsEnginesWatcher::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

void CameraSettingsAnalyticsEnginesWatcher::update()
{
    d->updateStore();
}

} // namespace nx::vms::client::desktop
