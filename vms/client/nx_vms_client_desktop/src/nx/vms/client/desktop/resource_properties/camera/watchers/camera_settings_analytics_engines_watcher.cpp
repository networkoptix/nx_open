#include "camera_settings_analytics_engines_watcher.h"

#include <algorithm>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <utils/common/connective.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../data/analytics_engine_info.h"

#include <common/common_module.h>
#include <api/runtime_info_manager.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

AnalyticsEngineInfo engineInfoFromResource(const AnalyticsEngineResourcePtr& engine)
{
    return AnalyticsEngineInfo {
        engine->getId(),
        engine->getName(),
        engine->manifest().deviceAgentSettingsModel
    };
}

} // namespace

class CameraSettingsAnalyticsEnginesWatcher::Private: public Connective<QObject>
{
public:
    void initialize(QnResourcePool* resourcePool);
    void setCamera(const QnVirtualCameraResourcePtr& camera);
    void updateStore();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_enginePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_engineManifestChanged(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

public:
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    QHash<QnUuid, AnalyticsEngineInfo> engines;
};

void CameraSettingsAnalyticsEnginesWatcher::Private::initialize(QnResourcePool* resourcePool)
{
    connect(resourcePool, &QnResourcePool::resourceAdded, this, &Private::at_resourceAdded);
    connect(resourcePool, &QnResourcePool::resourceRemoved, this, &Private::at_resourceRemoved);

    for (const auto& engine: resourcePool->getResources<AnalyticsEngineResource>())
        at_resourceAdded(engine);
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
        connect(camera, &QnResource::parentIdChanged, this, &Private::updateStore);

    updateStore();
}

void CameraSettingsAnalyticsEnginesWatcher::Private::updateStore()
{
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
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_resourceAdded(
    const QnResourcePtr& resource)
{
    if (const auto& engine = resource.dynamicCast<AnalyticsEngineResource>())
    {
        const auto info = engineInfoFromResource(engine);
        if (info.id.isNull())
            return;

        engines[info.id] = info;

        connect(engine, &QnResource::nameChanged,
            this, &Private::at_engineNameChanged);
        connect(engine, &QnResource::propertyChanged,
            this, &Private::at_enginePropertyChanged);
        connect(engine, &nx::vms::common::AnalyticsEngineResource::manifestChanged,
            this, &Private::at_engineManifestChanged);

        updateStore();
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_resourceRemoved(
    const QnResourcePtr& resource)
{
    resource->disconnect(this);
    if (engines.remove(resource->getId()) > 0)
        updateStore();
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
    const auto engine = resource.staticCast<AnalyticsEngineResource>();

    if (key == AnalyticsEngineResource::kSettingsValuesProperty)
    {
        store->setDeviceAgentSettingsValues(engine->getId(), engine->settingsValues());
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_engineManifestChanged(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine)
{
    if (const auto it = engines.find(engine->getId()); it != engines.end())
    {
        it->settingsModel = engine->manifest().deviceAgentSettingsModel;
        updateStore();
    }
}

CameraSettingsAnalyticsEnginesWatcher::CameraSettingsAnalyticsEnginesWatcher(
    CameraSettingsDialogStore* store, QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private())
{
    d->store = store;
    d->initialize(resourcePool());
}

CameraSettingsAnalyticsEnginesWatcher::~CameraSettingsAnalyticsEnginesWatcher() = default;

QnVirtualCameraResourcePtr CameraSettingsAnalyticsEnginesWatcher::camera() const
{
    return d->camera;
}

void CameraSettingsAnalyticsEnginesWatcher::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    d->setCamera(camera);
}

} // namespace nx::vms::client::desktop
