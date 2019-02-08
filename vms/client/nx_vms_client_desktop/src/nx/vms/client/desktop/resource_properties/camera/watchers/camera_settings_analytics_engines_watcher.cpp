#include "camera_settings_analytics_engines_watcher.h"

#include <algorithm>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../data/analytics_engine_info.h"

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

class CameraSettingsAnalyticsEnginesWatcher::Private: public QObject
{
public:
    void updateStore();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_enginePropertyChanged(const QnResourcePtr& resource, const QString& key);

public:
    CameraSettingsDialogStore* store = nullptr;
    QHash<QnUuid, AnalyticsEngineInfo> engines;
    QSet<QnUuid> enabledEngines;
    QHash<QnUuid, QVariantMap> settingsValuesByEngineId;
};

void CameraSettingsAnalyticsEnginesWatcher::Private::updateStore()
{
    auto enginesList = engines.values();
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

        connect(engine.data(), &QnResource::nameChanged,
            this, &Private::at_engineNameChanged);
        connect(engine.data(), &QnResource::propertyChanged,
            this, &Private::at_enginePropertyChanged);

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
    else if (key == AnalyticsEngineResource::kEngineManifestProperty)
    {
        if (const auto it = engines.find(resource->getId()); it != engines.end())
        {
            it->settingsModel = engine->manifest().deviceAgentSettingsModel;
            updateStore();
        }
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

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        d.data(), &Private::at_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        d.data(), &Private::at_resourceRemoved);

    for (const auto& engine: resourcePool()->getResources<AnalyticsEngineResource>())
        d->at_resourceAdded(engine);
}

CameraSettingsAnalyticsEnginesWatcher::~CameraSettingsAnalyticsEnginesWatcher() = default;

} // namespace nx::vms::client::desktop
