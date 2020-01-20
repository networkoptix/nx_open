#include "analytics_settings_manager.h"

#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <client_core/connection_context_aware.h>
#include <api/server_rest_connection.h>

namespace nx::vms::client::desktop {

uint qHash(const DeviceAgentId& key)
{
    return qHash(key.device) + qHash(key.engine);
}

auto toResources(const DeviceAgentId& agentId, QnResourcePool* resourcePool)
{
    const auto camera = resourcePool->getResourceById<QnVirtualCameraResource>(agentId.device);
    return std::make_tuple(
        camera,
        NX_ASSERT(camera) ? camera->getParentServer() : QnMediaServerResourcePtr(),
        resourcePool->getResourceById<nx::vms::common::AnalyticsEngineResource>(agentId.engine)
    );
}

QString toString(const DeviceAgentId& agentId, QnResourcePool* resourcePool)
{
    const auto device = resourcePool->getResourceById<QnVirtualCameraResource>(agentId.device);
    const auto engine = resourcePool->getResourceById<nx::vms::common::AnalyticsEngineResource>(
        agentId.engine);

    return QString("%1 - %2").arg(
        device ? device->getName() : "Deleted device",
        engine ? engine->getName() : "Deleted engine");
};

//-------------------------------------------------------------------------------------------------

QJsonObject AnalyticsSettingsListener::model() const
{
    return m_manager->model(agentId);
}

QJsonObject AnalyticsSettingsListener::values() const
{
    return m_manager->values(agentId);
}

AnalyticsSettingsListener::AnalyticsSettingsListener(
    const DeviceAgentId& agentId,
    AnalyticsSettingsManager* manager)
    :
    agentId(agentId),
    m_manager(manager)
{
}

//-------------------------------------------------------------------------------------------------

class AnalyticsSettingsManager::Private: public QObject, public QnConnectionContextAware
{
public:
    Private();

    /**
     * Raw values are values stored in the camera's property. They may differ from the actual values,
     * received by a separate REST request. We are caching them to distinguish whether current user
     * changes the settings or they are changed by another user (and we must refresh actual values).
     */
    void refreshSettings(const DeviceAgentId& agentId, QJsonObject newRawValues);
    void handleListenerDeleted(const DeviceAgentId& id);

    bool hasSubscription(const DeviceAgentId& id) const;

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleDevicePropertyChanged(const QnResourcePtr& resource, const QString& key);
    void handleEnginePropertyChanged(const QnResourcePtr& resource, const QString& key);

    void setSettings(const DeviceAgentId& id, const QJsonObject& model, const QJsonObject& values);

public:
    struct SettingsData
    {
        QJsonObject model;
        QJsonObject values;

        QJsonObject rawValues;
        std::weak_ptr<AnalyticsSettingsListener> listener;
    };
    struct Subscription
    {
        QHash<QnUuid, SettingsData> settingsByEngineId;
    };
    QHash<QnUuid, Subscription> subscriptionByDeviceId;

    SettingsData& dataByAgentIdRef(const DeviceAgentId& id)
    {
        return subscriptionByDeviceId[id.device].settingsByEngineId[id.engine];
    }

    SettingsData dataByAgentId(const DeviceAgentId& id) const
    {
        return subscriptionByDeviceId[id.device].settingsByEngineId[id.engine];
    }

    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;
};

AnalyticsSettingsManager::Private::Private()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &Private::handleResourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &Private::handleResourceRemoved);

    for (const auto& engine:
        resourcePool()->getResources<nx::vms::common::AnalyticsEngineResource>())
    {
        handleResourceAdded(engine);
    }

    for (const QnVirtualCameraResourcePtr& camera: resourcePool()->getAllCameras())
        handleResourceAdded(camera);
}

void AnalyticsSettingsManager::Private::refreshSettings(
    const DeviceAgentId& agentId, QJsonObject newRawValues)
{
    auto [device, server, engine] = toResources(agentId, resourcePool());
    if (!device || !server || !engine)
        return;

    if (newRawValues.isEmpty())
        newRawValues = device->deviceAgentSettingsValues()[agentId.engine];

    NX_DEBUG(this, "Refreshing settings for %1 - %2", device->getName(), engine->getName());
    NX_VERBOSE(this, "Raw values:\n%1", newRawValues);

    const auto handle = server->restConnection()->getDeviceAnalyticsSettings(
        device,
        engine,
        nx::utils::guarded(this,
            [this, agentId, newRawValues](
                bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::SettingsResponse &result)
            {
                NX_VERBOSE(this, "Received reply %1 (success: %2)", requestId, success);
                if (!pendingRefreshRequests.removeOne(requestId))
                    return;

                // TODO: Shouldn't we queue another request here?
                if (!success)
                    return;

                dataByAgentIdRef(agentId).rawValues = newRawValues;
                setSettings(agentId, result.model, result.values);
            }),
        thread());

    NX_VERBOSE(this, "Request handle %1", handle);
    if (handle > 0)
        pendingRefreshRequests.append(handle);
}

void AnalyticsSettingsManager::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        connect(camera.data(), &QnResource::propertyChanged, this,
            &Private::handleDevicePropertyChanged);
    }
    else if (const auto& engine = resource.dynamicCast<common::AnalyticsEngineResource>())
    {
        connect(engine.data(), &QnResource::propertyChanged, this,
            &Private::handleEnginePropertyChanged);
    }
}

void AnalyticsSettingsManager::Private::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
        camera->disconnect(this);
    else if (const auto& engine = resource.dynamicCast<common::AnalyticsEngineResource>())
        engine->disconnect(this);
}

void AnalyticsSettingsManager::Private::handleDevicePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    const auto& device = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(device))
        return;

    if (key == QnVirtualCameraResource::kDeviceAgentsSettingsValuesProperty
        || key == QnVirtualCameraResource::kDeviceAgentManifestsProperty)
    {
        NX_DEBUG(this, "Device agent property %1 changed for %2", key, device->getName());

        const QnUuid& deviceId = device->getId();
        const QHash<QnUuid, QJsonObject>& valuesByEngine = device->deviceAgentSettingsValues();
        for (auto it = valuesByEngine.begin(); it != valuesByEngine.end(); ++it)
        {
            DeviceAgentId agentId{deviceId, it.key()};
            const auto& data = dataByAgentId(agentId);
            if (!data.listener.expired() && data.rawValues != it.value())
                refreshSettings(agentId, it.value());
        }
    }
}

void AnalyticsSettingsManager::Private::handleEnginePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    const auto& engine = resource.dynamicCast<common::AnalyticsEngineResource>();
    if (!NX_ASSERT(engine))
        return;

    const QnUuid& engineId = engine->getId();

    if (key == common::AnalyticsEngineResource::kEngineManifestProperty)
    {
        NX_DEBUG(this, "Engine manifest changed for %1", engine->getName());
        for (auto it = subscriptionByDeviceId.begin(); it != subscriptionByDeviceId.end(); ++it)
        {
            if (it->settingsByEngineId.contains(engineId))
                refreshSettings({it.key(), engineId}, {});
        }
    }
}

void AnalyticsSettingsManager::Private::handleListenerDeleted(const DeviceAgentId& id)
{
    NX_VERBOSE(this, "Listener destroyed: %1", toString(id, resourcePool()));
    auto subscriptionIt = subscriptionByDeviceId.find(id.device);
    if (subscriptionIt != subscriptionByDeviceId.end())
    {
        subscriptionIt->settingsByEngineId.remove(id.engine);
        if (subscriptionIt->settingsByEngineId.isEmpty())
            subscriptionByDeviceId.erase(subscriptionIt);
    }
}

bool AnalyticsSettingsManager::Private::hasSubscription(const DeviceAgentId& id) const
{
    const auto it = subscriptionByDeviceId.find(id.device);
    if (it == subscriptionByDeviceId.end())
        return false;

    return it->settingsByEngineId.contains(id.engine);
}

void AnalyticsSettingsManager::Private::setSettings(
    const DeviceAgentId& id, const QJsonObject& model, const QJsonObject& values)
{
    auto& data = dataByAgentIdRef(id);

    const bool modelChanged = (data.model != model);
    if (modelChanged)
        data.model = model;

    const bool valuesChanged = (data.values != values);
    if (valuesChanged)
        data.values = values;

    NX_VERBOSE(this, "Store settings for %1, model changed: %2, values changed: %3",
        toString(id, resourcePool()), modelChanged, valuesChanged);
    if (modelChanged)
        NX_VERBOSE(this, "Updated model:\n%1", model);
    if (valuesChanged)
        NX_VERBOSE(this, "Updated values:\n%1", values);

    if (auto listener = data.listener.lock())
    {
        if (modelChanged)
            emit listener->modelChanged(model);
        if (valuesChanged)
            emit listener->valuesChanged(values);
    }
}

//-------------------------------------------------------------------------------------------------

AnalyticsSettingsManager::AnalyticsSettingsManager(QObject* parent):
    QObject(parent),
    d(new Private())
{
}

AnalyticsSettingsManager::~AnalyticsSettingsManager()
{
}

AnalyticsSettingsListenerPtr AnalyticsSettingsManager::getListener(const DeviceAgentId& agentId)
{
    NX_VERBOSE(this, "Listener requested for %1", toString(agentId, d->resourcePool()));
    auto& data = d->dataByAgentIdRef(agentId);
    if (const auto& listener = data.listener.lock())
        return listener;

    NX_DEBUG(this, "New listener created for %1", toString(agentId, d->resourcePool()));
    AnalyticsSettingsListenerPtr listener(new AnalyticsSettingsListener(agentId, this));
    data.listener = listener;
    connect(listener.get(), &QObject::destroyed, d.data(),
        [this, agentId]() { d->handleListenerDeleted(agentId); });
    d->refreshSettings(agentId, {});

    return listener;
}

QJsonObject AnalyticsSettingsManager::values(const DeviceAgentId& agentId) const
{
    return d->dataByAgentId(agentId).values;
}

QJsonObject AnalyticsSettingsManager::model(const DeviceAgentId& agentId) const
{
    return d->dataByAgentId(agentId).model;
}

AnalyticsSettingsManager::Error AnalyticsSettingsManager::applyChanges(
    const QHash<DeviceAgentId, QJsonObject>& valuesByAgentId)
{
    if (isApplyingChanges())
        return Error::busy;

    for (auto it = valuesByAgentId.begin(); it != valuesByAgentId.end(); ++it)
    {
        const auto agentId = it.key();
        auto [device, server, engine] = toResources(agentId, d->resourcePool());
        if (!device || !server || !engine)
            continue;

        const auto rawValues = it.value();
        if (d->hasSubscription(agentId))
        {
            auto& data = d->dataByAgentIdRef(agentId);
            data.rawValues = rawValues;
        }

        const auto handle = server->restConnection()->setDeviceAnalyticsSettings(
            device,
            engine,
            rawValues,
            nx::utils::guarded(this,
                [this, agentId](
                    bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::SettingsResponse& result)
                {
                    if (!d->pendingApplyRequests.removeOne(requestId))
                        return;

                    if (success)
                    {
                        if (d->hasSubscription(agentId))
                            d->setSettings(agentId, result.model, result.values);
                    }

                    if (!isApplyingChanges())
                        emit appliedChanges();
                }),
            thread());

        if (handle >= 0)
            d->pendingApplyRequests.append(handle);
    }

    return Error::noError;
}

bool AnalyticsSettingsManager::isApplyingChanges() const
{
    return !d->pendingApplyRequests.isEmpty();
}

} // namespace nx::vms::client::desktop
