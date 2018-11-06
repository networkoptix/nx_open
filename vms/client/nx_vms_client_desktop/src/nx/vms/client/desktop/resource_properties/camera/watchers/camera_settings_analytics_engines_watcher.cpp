#include "camera_settings_analytics_engines_watcher.h"

#include <algorithm>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../utils/analytics_engine_info.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

AnalyticsEngineInfo engineInfoFromResource(const AnalyticsEngineResourcePtr& engine)
{
    const auto plugin = engine->getParentResource().dynamicCast<AnalyticsPluginResource>();
    if (!plugin)
        return {};

    return AnalyticsEngineInfo {
        engine->getId(),
        engine->getName(),
        plugin->deviceAgentSettingsModel()
    };
}

AnalyticsEngineInfo engineInfoFromResource(const QnResourcePtr& resource)
{
    if (const auto engine = resource.dynamicCast<AnalyticsEngineResource>())
        return engineInfoFromResource(engine);
    return {};
}

} // namespace

class CameraSettingsAnalyticsEnginesWatcher::Private: public QObject
{
    CameraSettingsAnalyticsEnginesWatcher* q = nullptr;

public:
    Private(CameraSettingsAnalyticsEnginesWatcher* q);

    void updateStore();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_enginePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void at_pluginPropertyChanged(const QnResourcePtr& resource, const QString& key);

public:
    CameraSettingsDialogStore* store = nullptr;
    QHash<QnUuid, AnalyticsEngineInfo> engines;
    QSet<QnUuid> enabledEngines;
    QHash<QnUuid, QVariantMap> settingsValuesByEngineId;
};

CameraSettingsAnalyticsEnginesWatcher::Private::Private(CameraSettingsAnalyticsEnginesWatcher* q):
    q(q)
{
}

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
        const auto info = engineInfoFromResource(resource);
        if (info.id.isNull())
            return;

        engines[info.id] = info;

        connect(engine.data(), &QnResource::nameChanged,
            this, &Private::at_engineNameChanged);
        connect(engine.data(), &QnResource::propertyChanged,
            this, &Private::at_enginePropertyChanged);

        updateStore();
    }
    else if (const auto& plugin = resource.dynamicCast<AnalyticsPluginResource>())
    {
        connect(plugin.data(), &QnResource::propertyChanged,
            this, &Private::at_pluginPropertyChanged);
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
    if (key == AnalyticsEngineResource::kSettingsValuesProperty)
    {
        const auto engine = resource.staticCast<AnalyticsEngineResource>();
        store->setDeviceAgentSettingsValues(engine->getId(), engine->settingsValues());
    }
}

void CameraSettingsAnalyticsEnginesWatcher::Private::at_pluginPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (key == AnalyticsPluginResource::kDeviceAgentSettingsModelProperty)
    {
        const auto plugin = resource.staticCast<AnalyticsPluginResource>();
        const QJsonObject settingsModel = plugin->deviceAgentSettingsModel();

        for (const auto& engine: q->resourcePool()->getResourcesByParentId(resource->getId()))
        {
            const auto it = engines.find(engine->getId());
            if (it == engines.end())
                continue;

            it->settingsModel = settingsModel;
        }

        updateStore();
    }
}

CameraSettingsAnalyticsEnginesWatcher::CameraSettingsAnalyticsEnginesWatcher(
    CameraSettingsDialogStore* store, QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
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
