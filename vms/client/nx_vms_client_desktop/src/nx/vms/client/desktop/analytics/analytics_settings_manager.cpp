#include "analytics_settings_manager.h"

#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>
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
    return std::make_pair(
        resourcePool->getResourceById<QnVirtualCameraResource>(agentId.device),
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

//--------------------------------------------------------------------------------------------------

class AnalyticsSettingsServerRestApiInterface: public AnalyticsSettingsServerInterface
{
public:
    AnalyticsSettingsServerRestApiInterface(QObject* owner):
        m_owner(owner)
    {}

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) override
    {
        const auto server = NX_ASSERT(device)
            ? device->getParentServer()
            : QnMediaServerResourcePtr();

        if (!NX_ASSERT(server) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        return server->restConnection()->getDeviceAnalyticsSettings(
            device,
            engine,
            nx::utils::guarded(m_owner, callback),
            m_owner->thread());
    }

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        AnalyticsSettingsCallback callback) override
    {
        const auto server = NX_ASSERT(device)
            ? device->getParentServer()
            : QnMediaServerResourcePtr();

        if (!NX_ASSERT(server) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        return server->restConnection()->setDeviceAnalyticsSettings(
            device,
            engine,
            settings,
            nx::utils::guarded(m_owner, callback),
            m_owner->thread());
    }

private:
    QPointer<QObject> m_owner;
};

//--------------------------------------------------------------------------------------------------

class AnalyticsSettingsManager::Private: public QObject, public QnConnectionContextAware
{
public:
    Private();

    void refreshSettings(const DeviceAgentId& agentId);
    void handleListenerDeleted(const DeviceAgentId& id);

    bool hasSubscription(const DeviceAgentId& id) const;

    void setSettings(const DeviceAgentId& id, const QJsonObject& model, const QJsonObject& values);

public:
    struct SettingsData
    {
        QJsonObject model;
        QJsonObject values;

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

    AnalyticsSettingsServerInterfacePtr serverInterface;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;
};

AnalyticsSettingsManager::Private::Private()
{
    serverInterface = std::make_shared<AnalyticsSettingsServerRestApiInterface>(this);

    using nx::vms::common::AnalyticsEngineResource;
    auto engineManifestChangesListener = new QnResourceChangesListener(
        QnResourceChangesListener::Policy::silentRemove,
        this);

    engineManifestChangesListener->connectToResources<AnalyticsEngineResource>(
        &AnalyticsEngineResource::manifestChanged,
        [this](const nx::vms::common::AnalyticsEngineResourcePtr& engine)
        {
            NX_DEBUG(this, "Engine manifest changed for %1", engine->getName());

            const QnUuid& engineId = engine->getId();
            for (auto it = subscriptionByDeviceId.begin(); it != subscriptionByDeviceId.end(); ++it)
            {
                if (it->settingsByEngineId.contains(engineId))
                    refreshSettings({it.key(), engineId});
            }
        });
}

void AnalyticsSettingsManager::Private::refreshSettings(const DeviceAgentId& agentId)
{
    auto [device, engine] = toResources(agentId, resourcePool());
    if (!device || !engine)
        return;

    NX_DEBUG(this, "Refreshing settings for %1 - %2", device->getName(), engine->getName());

    const auto handle = serverInterface->getSettings(
        device,
        engine,
        [this, agentId](
            bool success,
            rest::Handle requestId,
            const nx::vms::api::analytics::DeviceAnalyticsSettingsResponse& result)
        {
            NX_VERBOSE(this, "Received reply %1 (success: %2)", requestId, success);
            if (!pendingRefreshRequests.removeOne(requestId))
                return;

            if (!success)
                return;

            setSettings(agentId, result.settingsModel, result.settingsValues);
        });

    NX_VERBOSE(this, "Request handle %1", handle);
    if (handle > 0)
        pendingRefreshRequests.append(handle);
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

AnalyticsSettingsManager::AnalyticsSettingsManager(
    QObject* parent)
    :
    QObject(parent),
    d(new Private())
{
}

AnalyticsSettingsManager::~AnalyticsSettingsManager()
{
}

void AnalyticsSettingsManager::setCustomServerInterface(
    AnalyticsSettingsServerInterfacePtr serverInterface)
{
    d->serverInterface = serverInterface;
}

void AnalyticsSettingsManager::refreshSettings(const DeviceAgentId& agentId)
{
    NX_VERBOSE(this, "Force refresh called for %1", toString(agentId, d->resourcePool()));
    if (d->hasSubscription(agentId))
        d->refreshSettings(agentId);
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
    d->refreshSettings(agentId);

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
        auto [device, engine] = toResources(agentId, d->resourcePool());
        if (!device || !engine)
            continue;

        const auto settings = it.value();

        const auto handle = d->serverInterface->applySettings(
            device,
            engine,
            settings,
            [this, agentId](
                bool success,
                rest::Handle requestId,
                const nx::vms::api::analytics::DeviceAnalyticsSettingsResponse& result)
            {
                if (!d->pendingApplyRequests.removeOne(requestId))
                    return;

                if (success)
                {
                    if (d->hasSubscription(agentId))
                        d->setSettings(agentId, result.settingsModel, result.settingsValues);
                }

                if (!isApplyingChanges())
                    emit appliedChanges();
            });

        if (handle > 0)
            d->pendingApplyRequests.append(handle);
    }

    return Error::noError;
}

bool AnalyticsSettingsManager::isApplyingChanges() const
{
    return !d->pendingApplyRequests.isEmpty();
}

} // namespace nx::vms::client::desktop
