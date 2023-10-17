// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_manager.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/analytics/device_agent_active_setting_changed_response.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>
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

struct AnalyticsSettingsManager::Private
{
    AnalyticsSettingsManager* const q;
    AnalyticsSettingsServerInterfacePtr serverInterface;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;

    void start();

    void refreshSettings(const DeviceAgentId& agentId);
    void handleListenerDeleted(const DeviceAgentId& id);

    bool hasSubscription(const DeviceAgentId& id) const;

    void updateStatus(const DeviceAgentId& id, DeviceAgentData::Status status);
    void updateStatusAndSettings(
        const DeviceAgentId& id,
        DeviceAgentData::Status status,
        const QJsonObject& settings);

    void previewDataReceived(const DeviceAgentId& id, const DeviceAgentData& data);
    void actionResultReceived(const DeviceAgentId& id, const AnalyticsActionResult& result);

    void loadResponseData(
        const DeviceAgentId& id,
        bool success,
        const DeviceAgentSettingsResponse& response);

    auto toResources(const DeviceAgentId& agentId)
    {
        return std::make_pair(
            q->resourcePool()->getResourceById<QnVirtualCameraResource>(agentId.device),
            q->resourcePool()->getResourceById<AnalyticsEngineResource>(agentId.engine)
        );
    }

    QString toString(const DeviceAgentId& agentId)
    {
        auto [device, engine] = toResources(agentId);
        return QString("%1 - %2").arg(
            device ? device->getName() : "Deleted device",
            engine ? engine->getName() : "Deleted engine");
    };

    struct SettingsData: DeviceAgentData
    {
        std::weak_ptr<AnalyticsSettingsListener> listener;
        DeviceAgentSettingsSession session;
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
};

void AnalyticsSettingsManager::Private::start()
{
    auto engineManifestChangesListener =
        new core::SessionResourcesSignalListener<AnalyticsEngineResource>(
            q->systemContext(),
            q);

    auto updateEngineSettings =
        [this](const AnalyticsEngineResourcePtr& engine)
        {
            const QnUuid& engineId = engine->getId();
            for (auto it = subscriptionByDeviceId.begin(); it != subscriptionByDeviceId.end(); ++it)
            {
                if (it->settingsByEngineId.contains(engineId))
                    refreshSettings({it.key(), engineId});
            }
        };

    engineManifestChangesListener->setOnAddedHandler(
        [updateEngineSettings](const AnalyticsEngineResourceList& engines)
        {
            for (const auto& engine: engines)
                updateEngineSettings(engine);
        });

    engineManifestChangesListener->addOnSignalHandler(
        &AnalyticsEngineResource::manifestChanged,
        [this, updateEngineSettings](const AnalyticsEngineResourcePtr& engine)
        {
            NX_DEBUG(this, "Engine manifest changed for %1", engine->getName());
            updateEngineSettings(engine);
        });

    engineManifestChangesListener->start();
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
            const DeviceAgentSettingsResponse& result)
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

void AnalyticsSettingsManager::Private::previewDataReceived(
    const DeviceAgentId& id,
    const DeviceAgentData& data)
{
    if (auto listener = dataByAgentId(id).listener.lock())
        emit listener->previewDataReceived(data);
}

void AnalyticsSettingsManager::Private::actionResultReceived(
    const DeviceAgentId& id,
    const AnalyticsActionResult& result)
{
    if (auto listener = dataByAgentId(id).listener.lock())
        emit listener->actionResultReceived(result);
}

void AnalyticsSettingsManager::Private::loadResponseData(
    const DeviceAgentId& id,
    bool success,
    const DeviceAgentSettingsResponse& response)
{
    auto& data = dataByAgentIdRef(id);

    // Data can be received as an API request reply or directly in the transaction. We need to
    // determine which source is more actual. Session id will be changed when camera jumps onto
    // another server. Sequence number grows on the server side when model or values are changed.
    // Empty session id means plugin is disabled currently, so we should always handle it.
    const bool receivedDataIsNewer = response.session.id.isNull()
        || response.session.id != data.session.id
        || response.session.sequenceNumber >= data.session.sequenceNumber;

    data.session = response.session;

    // When model is changed on the server side, it's id is re-generated, so we may quickly compare
    // model id first.
    const bool modelChanged = receivedDataIsNewer
        && (data.modelId != response.settingsModelId || data.model != response.settingsModel);

    if (modelChanged)
    {
        data.model = response.settingsModel;
        data.modelId = response.settingsModelId;
    }

    const bool valuesChanged = receivedDataIsNewer && (data.values != response.settingsValues);
    if (valuesChanged)
        data.values = response.settingsValues;

    const auto status = success
        ? DeviceAgentData::Status::ok
        : DeviceAgentData::Status::failure;
    const bool statusChanged = (data.status != status);
    data.status = status;

    NX_VERBOSE(this, "Store settings for %1, response is actual: %2 "
        "model changed: %3, values changed: %4, status %5",
        toString(id), receivedDataIsNewer, modelChanged, valuesChanged, (int)status);
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
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q=this})
{
    d->start();
}

AnalyticsSettingsManager::~AnalyticsSettingsManager()
{
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

void AnalyticsSettingsManager::storeSettings(
    const DeviceAgentId& agentId,
    const nx::vms::api::analytics::DeviceAgentSettingsResponse& data)
{
    NX_VERBOSE(this, "Settings update received for %1", d->toString(agentId));
    if (d->hasSubscription(agentId))
        d->loadResponseData(agentId, /*success*/ true, data);
}

bool AnalyticsSettingsManager::activeSettingsChanged(
    const DeviceAgentId& agentId,
    const QString& activeElement,
    const QJsonObject& settingsModel,
    const QJsonObject& settingsValues,
    const QJsonObject& paramValues)
{
    if (!NX_ASSERT(d->serverInterface))
        return false;

    auto [device, engine] = d->toResources(agentId);
    if (!device || !engine)
        return false;

    const auto handle = d->serverInterface->activeSettingsChanged(
        device,
        engine,
        activeElement,
        settingsModel,
        settingsValues,
        paramValues,
        [this, agentId](
            bool success,
            rest::Handle requestId,
            const DeviceAgentActiveSettingChangedResponse& result)
        {
            if (!success)
                return;

            d->previewDataReceived(
                agentId,
                DeviceAgentData{
                    .model = result.settingsModel,
                    .values = result.settingsValues,
                    .errors = result.settingsErrors,
                    .modelId = result.settingsModelId,
                    .status = DeviceAgentData::Status::ok
                });

            d->actionResultReceived(
                agentId,
                AnalyticsActionResult{
                    .actionUrl = result.actionUrl,
                    .messageToUser = result.messageToUser,
                    .useProxy = result.useProxy,
                    .useDeviceCredentials = result.useDeviceCredentials
                });
        });

    return handle > 0;
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
    connect(listener.get(), &QObject::destroyed, this,
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
                const DeviceAgentSettingsResponse& result)
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
