#include "analytics_settings_multi_listener.h"

#include <client/client_module.h>

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
    const ListenPolicy listenPolicy;
    QnUuid deviceId;
    QnVirtualCameraResourcePtr camera;
    QHash<QnUuid, AnalyticsSettingsListenerPtr> listeners;
    QSet<QnUuid> compatibleEngines;
    QSet<QnUuid> enabledEngines;

public:
    Private(AnalyticsSettingsMultiListener* q, ListenPolicy listenPolicy);
    bool isSuitableEngine(const common::AnalyticsEngineResourcePtr& engine) const;
    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void resubscribe();
    void handleDevicePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void addListener(const QnUuid& engineId);
};

AnalyticsSettingsMultiListener::Private::Private(
    AnalyticsSettingsMultiListener* q,
    ListenPolicy listenPolicy)
    :
    q(q),
    settingsManager(qnClientModule->analyticsSettingsManager()),
    listenPolicy(listenPolicy)
{
}

bool AnalyticsSettingsMultiListener::Private::isSuitableEngine(
    const common::AnalyticsEngineResourcePtr& engine) const
{
    if (listenPolicy == ListenPolicy::enabledEngines)
        return enabledEngines.contains(engine->getId());

    return compatibleEngines.contains(engine->getId());
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
    listeners.clear();

    const auto engines = resourcePool()->getResources<common::AnalyticsEngineResource>();
    for (const auto& engine: engines)
    {
        if (isSuitableEngine(engine))
            addListener(engine->getId());
    }

    oldListeners.clear();

    emit q->enginesChanged();
}

void AnalyticsSettingsMultiListener::Private::handleDevicePropertyChanged(
    const QnResourcePtr& /*resource*/, const QString& key)
{
    if (key == camera->kCompatibleAnalyticsEnginesProperty
        && listenPolicy == ListenPolicy::allEngines)
    {
         const auto newCompatibleEngines = camera->compatibleAnalyticsEngines();
         if (newCompatibleEngines != compatibleEngines)
         {
             compatibleEngines = newCompatibleEngines;
             resubscribe();
         }
    }
    else if (key == camera->kUserEnabledAnalyticsEnginesProperty
        && listenPolicy == ListenPolicy::enabledEngines)
    {
        const auto newEnabledEngines = camera->enabledAnalyticsEngines();
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

    connect(listener.get(), &AnalyticsSettingsListener::dataChanged, this,
        [this, engineId](const DeviceAgentData& data)
        {
            emit q->dataChanged(engineId, data);
        });
}

AnalyticsSettingsMultiListener::AnalyticsSettingsMultiListener(
    const QnVirtualCameraResourcePtr& camera,
    ListenPolicy listenPolicy,
    QObject* parent)
    :
    QObject(parent),
    d(new Private(this, listenPolicy))
{
    d->camera = camera;
    connect(camera.data(), &QnResource::propertyChanged, d.data(),
        &Private::handleDevicePropertyChanged);

    connect(d->resourcePool(), &QnResourcePool::resourceAdded, d.data(),
        &Private::handleResourceAdded);
    connect(d->resourcePool(), &QnResourcePool::resourceRemoved, d.data(),
        &Private::handleResourceRemoved);

    d->enabledEngines = camera->enabledAnalyticsEngines();
    d->compatibleEngines = camera->compatibleAnalyticsEngines();
    d->resubscribe();
}

AnalyticsSettingsMultiListener::~AnalyticsSettingsMultiListener()
{
}

DeviceAgentData AnalyticsSettingsMultiListener::data(const QnUuid& engineId) const
{
    if (const auto& listener = d->listeners.value(engineId))
        return listener->data();
    return {};
}

QSet<QnUuid> AnalyticsSettingsMultiListener::engineIds() const
{
    return d->listeners.keys().toSet();
}

} // namespace nx::vms::client::desktop
