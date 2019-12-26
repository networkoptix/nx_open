#include "analytics_settings_multi_listener.h"

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <client_core/connection_context_aware.h>

#include "analytics_settings_manager.h"

namespace nx::vms::client::desktop {

class AnalyticsSettingsMultiListener::Private: public QObject, public QnConnectionContextAware
{
    AnalyticsSettingsMultiListener* q;

public:
    AnalyticsSettingsManager* const settingsManager;
    QnUuid deviceId;
    QnVirtualCameraResourcePtr camera;
    ListenPolicy listenPolicy = ListenPolicy::enabledEngines;
    QHash<QnUuid, AnalyticsSettingsListenerPtr> listeners;
    QSet<QnUuid> enabledEngines;

public:
    Private(AnalyticsSettingsMultiListener* q);
    bool isSuitableEngine(const common::AnalyticsEngineResourcePtr& engine) const;
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void resubscribe();
    void handleDevicePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void addListener(const QnUuid& engineId);
};

AnalyticsSettingsMultiListener::Private::Private(AnalyticsSettingsMultiListener* q):
    q(q),
    settingsManager(commonModule()->instance<AnalyticsSettingsManager>())
{
}

bool AnalyticsSettingsMultiListener::Private::isSuitableEngine(
    const common::AnalyticsEngineResourcePtr& engine) const
{
    if (listenPolicy == ListenPolicy::enabledEngines)
        return enabledEngines.contains(engine->getId());

    return true;
}

void AnalyticsSettingsMultiListener::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto engine = resource.dynamicCast<common::AnalyticsEngineResource>(); 
        engine && isSuitableEngine(engine))
    {
        addListener(resource->getId());
    }
}

void AnalyticsSettingsMultiListener::Private::handleResourceRemoved(const QnResourcePtr& resource)
{
    listeners.remove(resource->getId());
}

void AnalyticsSettingsMultiListener::Private::resubscribe()
{
    // Store listeners to prevent cache from wiping their data.
    QHash<QnUuid, AnalyticsSettingsListenerPtr> oldListeners = listeners;

    const auto engines = resourcePool()->getResources<common::AnalyticsEngineResource>();
    for (const auto& engine: engines)
    {
        if (isSuitableEngine(engine))
            addListener(engine->getId());
    }

    oldListeners.clear();
}

void AnalyticsSettingsMultiListener::Private::handleDevicePropertyChanged(
    const QnResourcePtr& /*resource*/, const QString& key)
{
    if (key == camera->kUserEnabledAnalyticsEnginesProperty
        && listenPolicy == ListenPolicy::enabledEngines)
    {
        const auto& newEnabledEngines = camera->enabledAnalyticsEngines();
        if (enabledEngines != newEnabledEngines)
        {
            enabledEngines = newEnabledEngines;
            resubscribe();
        }
    }
}

void AnalyticsSettingsMultiListener::Private::addListener(const QnUuid& engineId)
{
    auto listener = settingsManager->getListener(DeviceAgentId{camera->getId(), engineId});
    listeners.insert(engineId, listener);

    connect(listener.get(), &AnalyticsSettingsListener::valuesChanged, this,
        [this, engineId](const QJsonObject& values)
        {
            emit q->valuesChanged(engineId, values);
        });
    connect(listener.get(), &AnalyticsSettingsListener::modelChanged, this,
        [this, engineId](const QJsonObject& model)
        {
            emit q->modelChanged(engineId, model);
        });
}

AnalyticsSettingsMultiListener::AnalyticsSettingsMultiListener(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
    :
    QObject(parent),
    d(new Private(this))
{
    d->camera = camera;
    connect(camera.data(), &QnResource::propertyChanged, d.data(),
        &Private::handleDevicePropertyChanged);

    connect(d->resourcePool(), &QnResourcePool::resourceAdded, d.data(),
        &Private::handleResourceAdded);
    connect(d->resourcePool(), &QnResourcePool::resourceRemoved, d.data(),
        &Private::handleResourceRemoved);

    d->enabledEngines = camera->enabledAnalyticsEngines();
    d->resubscribe();
}

AnalyticsSettingsMultiListener::~AnalyticsSettingsMultiListener()
{
}

AnalyticsSettingsMultiListener::ListenPolicy AnalyticsSettingsMultiListener::listenPolicy() const
{
    return d->listenPolicy;
}

void AnalyticsSettingsMultiListener::setListenPolicy(
    AnalyticsSettingsMultiListener::ListenPolicy policy)
{
    if (d->listenPolicy == policy)
        return;

    d->listenPolicy = policy;
}

QJsonObject AnalyticsSettingsMultiListener::values(const QnUuid& engineId) const
{
    if (const auto& listener = d->listeners.value(engineId))
        return listener->values();
    return {};
}

QJsonObject AnalyticsSettingsMultiListener::model(const QnUuid& engineId) const
{
    if (const auto& listener = d->listeners.value(engineId))
        return listener->model();
    return {};
}

QSet<QnUuid> AnalyticsSettingsMultiListener::engineIds() const
{
    return d->listeners.keys().toSet();
}

} // namespace nx::vms::client::desktop
