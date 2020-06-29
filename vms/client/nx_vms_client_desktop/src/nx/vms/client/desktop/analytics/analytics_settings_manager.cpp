#include "analytics_settings_manager.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_changes_listener.h>
#include <core/resource/camera_resource.h>
#include <client_core/connection_context_aware.h>

#include <nx/vms/api/analytics/device_analytics_settings_data.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::common;
using namespace nx::vms::api::analytics;

//--------------------------------------------------------------------------------------------------

AnalyticsSettingsServerInterface::~AnalyticsSettingsServerInterface()
{
}

//--------------------------------------------------------------------------------------------------

DeviceAgentData AnalyticsSettingsListener::data() const
{
    return m_manager->data(agentId);
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

class AnalyticsSettingsManager::Private: public QObject
{
public:
    void listenResourcePool(QnResourcePool* resourcePool);

    void refreshSettings(const DeviceAgentId& agentId);
    void handleListenerDeleted(const DeviceAgentId& id);

    bool hasSubscription(const DeviceAgentId& id) const;

    void updateStatus(const DeviceAgentId& id, DeviceAgentData::Status status);
    void updateStatusAndSettings(
        const DeviceAgentId& id,
        DeviceAgentData::Status status,
        const QJsonObject& settings);

    void loadResponseData(
        const DeviceAgentId& id,
        bool success,
        const DeviceAnalyticsSettingsResponse& response);

    auto toResources(const DeviceAgentId& agentId)
    {
        if (!NX_ASSERT(m_resourcePool))
            return std::make_pair(QnVirtualCameraResourcePtr(), AnalyticsEngineResourcePtr());

        return std::make_pair(
            m_resourcePool->getResourceById<QnVirtualCameraResource>(agentId.device),
            m_resourcePool->getResourceById<AnalyticsEngineResource>(agentId.engine)
        );
    }

    QString toString(const DeviceAgentId& agentId)
    {
        if (!NX_ASSERT(m_resourcePool))
            return QString();

        auto [device, engine] = toResources(agentId);
        return QString("%1 - %2").arg(
            device ? device->getName() : "Deleted device",
            engine ? engine->getName() : "Deleted engine");
    };

public:
    struct SettingsData: DeviceAgentData
    {
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

private:
    QPointer<QnResourcePool> m_resourcePool;
};

void AnalyticsSettingsManager::Private::listenResourcePool(QnResourcePool* resourcePool)
{
    m_resourcePool = resourcePool;
    if (!NX_ASSERT(resourcePool))
        return;

    auto engineManifestChangesListener = new QnResourceChangesListener(
        QnResourceChangesListener::Policy::silentRemove,
        this);

    engineManifestChangesListener->connectToResources<AnalyticsEngineResource>(
        resourcePool,
        &AnalyticsEngineResource::manifestChanged,
        [this](const AnalyticsEngineResourcePtr& engine)
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
    auto [device, engine] = toResources(agentId);
    if (!device || !engine)
        return;

    if (!NX_ASSERT(serverInterface))
        return;

    NX_DEBUG(this, "Refreshing settings for %1 - %2", device->getName(), engine->getName());

    const auto handle = serverInterface->getSettings(
        device,
        engine,
        [this, agentId](
            bool success,
            rest::Handle requestId,
            const DeviceAnalyticsSettingsResponse& result)
        {
            NX_VERBOSE(this, "Received reply %1 (success: %2)", requestId, success);
            if (!pendingRefreshRequests.removeOne(requestId))
                return;

            loadResponseData(agentId, success, result);
        });

    NX_VERBOSE(this, "Request handle %1", handle);
    if (handle > 0)
    {
        pendingRefreshRequests.append(handle);
        updateStatus(agentId, DeviceAgentData::Status::loading);
    }
}

void AnalyticsSettingsManager::Private::handleListenerDeleted(const DeviceAgentId& id)
{
    NX_VERBOSE(this, "Listener destroyed: %1", toString(id));
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

void AnalyticsSettingsManager::Private::updateStatus(
    const DeviceAgentId& id,
    DeviceAgentData::Status status)
{
    if (!hasSubscription(id))
        return;

    auto& data = dataByAgentIdRef(id);
    if (data.status == status)
        return;

    data.status = status;
    if (auto listener = data.listener.lock())
        emit listener->dataChanged(data);
}

void AnalyticsSettingsManager::Private::updateStatusAndSettings(
    const DeviceAgentId& id,
    DeviceAgentData::Status status,
    const QJsonObject& settings)
{
    if (!hasSubscription(id))
        return;

    auto& data = dataByAgentIdRef(id);
    if (data.status == status && data.values == settings)
        return;

    data.status = status;
    data.values = settings;
    if (auto listener = data.listener.lock())
        emit listener->dataChanged(data);
}

void AnalyticsSettingsManager::Private::loadResponseData(
    const DeviceAgentId& id,
    bool success,
    const DeviceAnalyticsSettingsResponse& response)
{
    auto& data = dataByAgentIdRef(id);

    const bool modelChanged =
        (data.model != response.settingsModel || data.modelId != response.settingsModelId);
    if (modelChanged)
    {
        data.model = response.settingsModel;
        data.modelId = response.settingsModelId;
    }

    const bool valuesChanged = (data.values != response.settingsValues);
    if (valuesChanged)
        data.values = response.settingsValues;

    const auto status = success
        ? DeviceAgentData::Status::ok
        : DeviceAgentData::Status::failure;
    const bool statusChanged = (data.status != status);
    data.status = status;

    NX_VERBOSE(this, "Store settings for %1, model changed: %2, values changed: %3, status %4",
        toString(id), modelChanged, valuesChanged, (int)status);
    if (modelChanged)
        NX_VERBOSE(this, "Updated model:\n%1", data.model);
    if (valuesChanged)
        NX_VERBOSE(this, "Updated values:\n%1", data.values);

    if (modelChanged || valuesChanged || statusChanged)
    {
        if (auto listener = data.listener.lock())
            emit listener->dataChanged(data);
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

void AnalyticsSettingsManager::setResourcePool(QnResourcePool* resourcePool)
{
    d->listenResourcePool(resourcePool);
}

void AnalyticsSettingsManager::setServerInterface(
    AnalyticsSettingsServerInterfacePtr serverInterface)
{
    d->serverInterface = serverInterface;
}

void AnalyticsSettingsManager::refreshSettings(const DeviceAgentId& agentId)
{
    NX_VERBOSE(this, "Force refresh called for %1", d->toString(agentId));
    if (d->hasSubscription(agentId))
        d->refreshSettings(agentId);
}

AnalyticsSettingsListenerPtr AnalyticsSettingsManager::getListener(const DeviceAgentId& agentId)
{
    NX_VERBOSE(this, "Listener requested for %1", d->toString(agentId));
    auto& data = d->dataByAgentIdRef(agentId);
    if (const auto& listener = data.listener.lock())
        return listener;

    NX_DEBUG(this, "New listener created for %1", d->toString(agentId));
    AnalyticsSettingsListenerPtr listener(new AnalyticsSettingsListener(agentId, this));
    data.listener = listener;
    connect(listener.get(), &QObject::destroyed, d.data(),
        [this, agentId]() { d->handleListenerDeleted(agentId); });
    d->refreshSettings(agentId);

    return listener;
}

DeviceAgentData AnalyticsSettingsManager::data(const DeviceAgentId& agentId) const
{
    return d->dataByAgentId(agentId);
}

AnalyticsSettingsManager::Error AnalyticsSettingsManager::applyChanges(
    const QHash<DeviceAgentId, QJsonObject>& valuesByAgentId)
{
    if (!d->pendingApplyRequests.isEmpty())
        return Error::busy;

    if (!NX_ASSERT(d->serverInterface))
        return Error::noError;

    for (auto it = valuesByAgentId.begin(); it != valuesByAgentId.end(); ++it)
    {
        const auto agentId = it.key();
        auto [device, engine] = d->toResources(agentId);
        if (!device || !engine)
            continue;

        const auto settings = it.value();

        const auto handle = d->serverInterface->applySettings(
            device,
            engine,
            settings,
            d->dataByAgentId(agentId).modelId,
            [this, agentId](
                bool success,
                rest::Handle requestId,
                const DeviceAnalyticsSettingsResponse& result)
            {
                if (!d->pendingApplyRequests.removeOne(requestId))
                    return;

                if (d->hasSubscription(agentId))
                    d->loadResponseData(agentId, success, result);
            });

        if (handle > 0)
        {
            d->pendingApplyRequests.append(handle);
            d->updateStatusAndSettings(
                agentId,
                DeviceAgentData::Status::applying,
                settings);
        }
    }

    return Error::noError;
}

} // namespace nx::vms::client::desktop
