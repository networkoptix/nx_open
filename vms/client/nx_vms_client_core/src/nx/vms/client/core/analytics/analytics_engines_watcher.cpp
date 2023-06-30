// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_engines_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/api/data/analytics_integration_model.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

#include "analytics_data_helper.h"

using namespace nx::vms::common;

namespace nx::vms::client::core {

class AnalyticsEnginesWatcher::Private:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    AnalyticsEnginesWatcher* const q = nullptr;

public:
    Private(AnalyticsEnginesWatcher* q);
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_engineNameChanged(const QnResourcePtr& resource);
    void at_engineManifestChanged(const AnalyticsEngineResourcePtr& engine);
    void at_enginePropertyChanged(const QnResourcePtr& resource, const QString& key);
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
        const auto& info =
            engineInfoFromResource(engine, SettingsModelSource::resourceProperty);
        if (info.id.isNull())
            return;

        engines[info.id] = info;

        connect(engine.get(), &QnResource::nameChanged, this, &Private::at_engineNameChanged);
        connect(engine.get(), &AnalyticsEngineResource::manifestChanged,
            this, &Private::at_engineManifestChanged);
        connect(engine.get(), &QnResource::propertyChanged, this, &Private::at_enginePropertyChanged);

        emit q->engineAdded(info.id, info);
    }
    else if (const auto& plugin = resource.dynamicCast<AnalyticsPluginResource>())
    {
        connect(plugin.get(), &QnResource::propertyChanged,
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
    emit q->engineUpdated(it->id);
}

void AnalyticsEnginesWatcher::Private::at_engineManifestChanged(
    const AnalyticsEngineResourcePtr& engine)
{
    const auto it = engines.find(engine->getId());
    if (it == engines.end())
        return;

    it->isDeviceDependent = engine->isDeviceDependent();
    emit q->engineUpdated(it->id);
}

void AnalyticsEnginesWatcher::Private::at_enginePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (key == AnalyticsEngineResource::kSettingsModelProperty)
    {
        const QJsonObject& settingsModel =
            QJson::deserialized<QJsonObject>(resource->getProperty(key).toUtf8());

        for (const auto& engine: resourcePool()->getResourcesByParentId(resource->getId()))
        {
            const auto it = engines.find(engine->getId());
            if (it == engines.end())
                continue;

            it->settingsModel = settingsModel;
            emit q->engineSettingsModelChanged(it->id);
        }
    }
}

void AnalyticsEnginesWatcher::Private::at_pluginPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (key == nx::vms::api::analytics::kPluginManifestProperty)
    {
        const auto plugin = resource.staticCast<AnalyticsPluginResource>();
        const QJsonObject& settingsModel = plugin->manifest().engineSettingsModel;

        for (const auto& engine: resourcePool()->getResourcesByParentId(resource->getId()))
        {
            const auto it = engines.find(engine->getId());
            if (it == engines.end())
                continue;

            it->settingsModel = settingsModel;
            emit q->engineSettingsModelChanged(it->id);
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

AnalyticsEngineInfo AnalyticsEnginesWatcher::engineInfo(const QnUuid& engineId) const
{
    return d->engines.value(engineId);
}

QHash<QnUuid, AnalyticsEngineInfo> AnalyticsEnginesWatcher::engineInfos() const
{
    return d->engines;
}

} // namespace nx::vms::client::core
