#include "analytics_settings_manager.h"

#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <client_core/connection_context_aware.h>
#include <api/server_rest_connection.h>

namespace nx::vms::client::desktop {

struct AgentId
{
    QnUuid device;
    QnUuid engine;

    bool operator==(const AgentId& other) const
    {
        return device == other.device && engine == other.engine;
    }
};

uint qHash(const AgentId& key)
{
    return qHash(key.device) + qHash(key.engine);
}

//-------------------------------------------------------------------------------------------------

QJsonObject AnalyticsSettingsListener::model() const
{
    return m_cache->model(deviceId, engineId);
}

QJsonObject AnalyticsSettingsListener::values() const
{
    return m_cache->values(deviceId, engineId);
}

AnalyticsSettingsListener::AnalyticsSettingsListener(
    const QnUuid& deviceId,
    const QnUuid& engineId,
    AnalyticsSettingsManager* cache)
    :
    deviceId(deviceId),
    engineId(engineId),
    m_cache(cache)
{
}

//-------------------------------------------------------------------------------------------------

class AnalyticsSettingsManager::Private: public QObject, public QnConnectionContextAware
{
public:
    Private();

    void refreshSettings(const AgentId& agentId, QJsonObject newRawValues);
    void handleListenerDeleted(const AgentId& id);

private:
    void addDevice(const QnResourcePtr& resource);
    void removeDevice(const QnResourcePtr& resource);
    void handleDevicePropertyChanged(const QnResourcePtr& resource, const QString& key);

    void setSettings(const AgentId& id, const QJsonObject& model, const QJsonObject& values);

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

    SettingsData& dataByAgentIdRef(const AgentId& id)
    {
        return subscriptionByDeviceId[id.device].settingsByEngineId[id.engine];
    }
    SettingsData dataByAgentId(const AgentId& id) const
    {
        return subscriptionByDeviceId[id.device].settingsByEngineId[id.engine];
    }

private:
    QList<rest::Handle> m_pendingRefreshRequests;
};

AnalyticsSettingsManager::Private::Private()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this, &Private::addDevice);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &Private::removeDevice);

    for (const QnVirtualCameraResourcePtr& camera: resourcePool()->getAllCameras())
        addDevice(camera);
}

void AnalyticsSettingsManager::Private::refreshSettings(
    const AgentId& agentId, QJsonObject newRawValues)
{
    const auto& camera =
        resourcePool()->getResourceById<QnVirtualCameraResource>(agentId.device);
    if (!camera)
        return;

    const auto& engine =
        resourcePool()->getResourceById<nx::vms::common::AnalyticsEngineResource>(agentId.engine);
    if (!engine)
        return;

    const QnMediaServerResourcePtr& server = camera->getParentServer();

    if (newRawValues.isEmpty())
        newRawValues = camera->deviceAgentSettingsValues()[agentId.engine];

    const auto handle = server->restConnection()->getDeviceAnalyticsSettings(
        camera,
        engine,
        nx::utils::guarded(this,
            [this, agentId, newRawValues](
                bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::SettingsResponse &result)
            {
                if (!m_pendingRefreshRequests.removeOne(requestId))
                    return;

                if (!success)
                    return;

                dataByAgentIdRef(agentId).rawValues = newRawValues;
                setSettings(agentId, result.model, result.values);
            }),
        thread());

    if (handle > 0)
        m_pendingRefreshRequests.append(handle);
}

void AnalyticsSettingsManager::Private::addDevice(const QnResourcePtr& resource)
{
    const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    connect(camera.data(), &QnResource::propertyChanged, this,
        &Private::handleDevicePropertyChanged);
}

void AnalyticsSettingsManager::Private::removeDevice(const QnResourcePtr& resource)
{
    const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    camera->disconnect(this);
}

void AnalyticsSettingsManager::Private::handleDevicePropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    const auto& camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!NX_ASSERT(camera))
        return;

    const QnUuid& deviceId = camera->getId();

    if (key == QnVirtualCameraResource::kDeviceAgentsSettingsValuesProperty)
    {
        const QHash<QnUuid, QJsonObject>& valuesByEngine = camera->deviceAgentSettingsValues();
        for (auto it = valuesByEngine.begin(); it != valuesByEngine.end(); ++it)
        {
            AgentId agentId{deviceId, it.key()};
            const auto& data = dataByAgentId(agentId);
            if (!data.listener.expired() && data.rawValues != it.value())
                refreshSettings(agentId, it.value());
        }
    }
}

void AnalyticsSettingsManager::Private::handleListenerDeleted(const AgentId& id)
{
    auto subscriptionIt = subscriptionByDeviceId.find(id.device);
    if (subscriptionIt != subscriptionByDeviceId.end())
    {
        subscriptionIt->settingsByEngineId.remove(id.engine);
        if (subscriptionIt->settingsByEngineId.isEmpty())
            subscriptionByDeviceId.erase(subscriptionIt);
    }
}

void AnalyticsSettingsManager::Private::setSettings(
    const AgentId& id, const QJsonObject& model, const QJsonObject& values)
{
    auto& data = dataByAgentIdRef(id);

    const bool modelChanged = (data.model != model);
    if (modelChanged)
        data.model = model;

    const bool valuesChanged = (data.values != values);
    if (valuesChanged)
        data.values = values;

    if (const auto& listener = data.listener.lock())
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

AnalyticsSettingsListenerPtr AnalyticsSettingsManager::getListener(
    const QnUuid& deviceId, const QnUuid& engineId)
{
    const AgentId agentId{deviceId, engineId};
    auto& data = d->dataByAgentIdRef(agentId);
    if (const auto& listener = data.listener.lock())
        return listener;

    AnalyticsSettingsListenerPtr listener(new AnalyticsSettingsListener(deviceId, engineId, this));
    data.listener = listener;
    connect(listener.get(), &QObject::destroyed, d.data(),
        [this, agentId]() { d->handleListenerDeleted(agentId); });
    d->refreshSettings(agentId, {});

    return listener;
}

QJsonObject AnalyticsSettingsManager::values(const QnUuid& deviceId, const QnUuid& engineId) const
{
    return d->dataByAgentId({deviceId, engineId}).values;
}

QJsonObject AnalyticsSettingsManager::model(const QnUuid& deviceId, const QnUuid& engineId) const
{
    return d->dataByAgentId({deviceId, engineId}).model;
}

} // namespace nx::vms::client::desktop
