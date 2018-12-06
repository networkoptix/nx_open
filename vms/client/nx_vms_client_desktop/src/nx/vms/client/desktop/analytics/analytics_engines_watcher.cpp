#include "analytics_engines_watcher.h"

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <core/resource_management/resource_pool.h>
#include <client_core/connection_context_aware.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

AnalyticsEnginesWatcher::AnalyticsEngineInfo engineInfoFromResource(
    const AnalyticsEngineResourcePtr& engine)
{
    const auto plugin = engine->getParentResource().dynamicCast<AnalyticsPluginResource>();
    if (!plugin)
        return {};

    return AnalyticsEnginesWatcher::AnalyticsEngineInfo {
        engine->getId(),
        engine->getName(),
        plugin->manifest().engineSettingsModel
    };
}

} // namespace

class AnalyticsEnginesWatcher::Private: public QObject, public QnConnectionContextAware
{
    AnalyticsEnginesWatcher* const q = nullptr;

public:
    Private(AnalyticsEnginesWatcher* q);
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_pluginPropertyChanged(const QnResourcePtr& resource, const QString& key);

public:
    QHash<QnUuid, AnalyticsEngineInfo> engines;
};

AnalyticsEnginesWatcher::Private::Private(AnalyticsEnginesWatcher* q):
    q(q)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &Private::at_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, &Private::at_resourceRemoved);

    for (const auto& engine: resourcePool()->getResources<AnalyticsEngineResource>())
        at_resourceAdded(engine);
}

void AnalyticsEnginesWatcher::Private::at_resourceAdded(const QnResourcePtr& resource)
{
    if (const auto& engine = resource.dynamicCast<AnalyticsEngineResource>())
    {
        const auto& info = engineInfoFromResource(engine);
        if (info.id.isNull())
            return;

        engines[info.id] = info;

        connect(engine.data(), &QnResource::nameChanged, this, &Private::at_engineNameChanged);

        emit q->engineAdded(info.id, info);
    }
    else if (const auto& plugin = resource.dynamicCast<AnalyticsPluginResource>())
    {
        connect(plugin.data(), &QnResource::propertyChanged,
            this, &Private::at_pluginPropertyChanged);
    }
}

void AnalyticsEnginesWatcher::Private::at_resourceRemoved(const QnResourcePtr& resource)
{
    const auto id = resource->getId();
    if (engines.remove(id))
    {
        resource->disconnect(this);
        emit q->engineRemoved(id);
    }
}

void AnalyticsEnginesWatcher::Private::at_engineNameChanged(const QnResourcePtr& resource)
{
    const auto it = engines.find(resource->getId());
    if (it == engines.end())
        return;

    it->name = resource->getName();
    emit q->engineNameChanged(it->id, it->name);
}

void AnalyticsEnginesWatcher::Private::at_pluginPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (key == AnalyticsPluginResource::kPluginManifestProperty)
    {
        const auto plugin = resource.staticCast<AnalyticsPluginResource>();
        const QJsonObject& settingsModel = plugin->manifest().engineSettingsModel;

        for (const auto& engine: resourcePool()->getResourcesByParentId(resource->getId()))
        {
            const auto it = engines.find(engine->getId());
            if (it == engines.end())
                continue;

            it->settingsModel = settingsModel;
            emit q->engineSettingsModelChanged(it->id, it->settingsModel);
        }
    }
}

AnalyticsEnginesWatcher::AnalyticsEnginesWatcher(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

AnalyticsEnginesWatcher::~AnalyticsEnginesWatcher()
{
}

AnalyticsEnginesWatcher::AnalyticsEngineInfo AnalyticsEnginesWatcher::engineInfo(
    const QnUuid& engineId) const
{
    return d->engines.value(engineId);
}

QHash<QnUuid, AnalyticsEnginesWatcher::AnalyticsEngineInfo>
AnalyticsEnginesWatcher::engineInfos() const
{
    return d->engines;
}

} // namespace nx::vms::client::desktop
