#include "device_agent_settings_adapter.h"

#include <nx/utils/log/assert.h>
#include <utils/common/delayed.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_multi_listener.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>
#include <core/resource/camera_resource.h>
#include <client_core/connection_context_aware.h>
#include <common/common_module.h>

#include "../redux/camera_settings_dialog_store.h"
#include "../redux/camera_settings_dialog_state.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::common;

class DeviceAgentSettingsAdapter::Private: public QnConnectionContextAware
{
public:
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    AnalyticsSettingsManager* settingsManager = nullptr;
    QnUuid currentEngineId;
    std::unique_ptr<AnalyticsSettingsMultiListener> settingsListener;

public:
    void refreshSettings(const QnUuid& engineId);
    void updateLoadingState();
};

void DeviceAgentSettingsAdapter::Private::refreshSettings(const QnUuid& engineId)
{
    if (!settingsListener)
        return;

    updateLoadingState();

    const QJsonObject& model = settingsListener->model(engineId);
    if (model.isEmpty())
        return;

    store->setDeviceAgentSettingsModel(engineId, model);
    store->resetDeviceAgentSettingsValues(engineId, settingsListener->values(engineId));
}

void DeviceAgentSettingsAdapter::Private::updateLoadingState()
{
    bool loading = false;

    if (settingsManager->isApplyingChanges())
        loading = true;
    else if (!currentEngineId.isNull() && settingsListener->model(currentEngineId).isEmpty())
        loading = true;

    store->setAnalyticsSettingsLoading(loading);
}

DeviceAgentSettingsAdapter::DeviceAgentSettingsAdapter(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->store = store;

    d->settingsManager = d->commonModule()->findInstance<AnalyticsSettingsManager>();
    if (!NX_ASSERT(d->settingsManager))
        return;

    d->currentEngineId = d->store->currentAnalyticsEngineId();

    connect(d->store, &CameraSettingsDialogStore::stateChanged, this,
        [this]()
        {
            const auto id = d->store->currentAnalyticsEngineId();
            if (d->currentEngineId == id)
                return;

            d->currentEngineId = id;
            executeDelayedParented([this, id]() { d->refreshSettings(id); }, this);
        });
}

DeviceAgentSettingsAdapter::~DeviceAgentSettingsAdapter()
{
}

void DeviceAgentSettingsAdapter::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (d->camera != camera)
    {
        d->camera = camera;

        if (camera)
        {
            d->settingsListener = std::make_unique<AnalyticsSettingsMultiListener>(camera);

            if (!d->currentEngineId.isNull())
                d->refreshSettings(d->currentEngineId);

            connect(d->settingsListener.get(), &AnalyticsSettingsMultiListener::modelChanged,
                this,
                [this](const QnUuid& engineId, const QJsonObject& model)
                {
                    if (!d->store->deviceAgentSettingsModel(engineId).isEmpty())
                    {
                        d->store->setDeviceAgentSettingsModel(engineId, model);
                        d->store->resetDeviceAgentSettingsValues(
                            engineId, d->settingsListener->values(engineId));
                        d->updateLoadingState();
                    }
                });
            connect(d->settingsListener.get(), &AnalyticsSettingsMultiListener::valuesChanged,
                this,
                [this](const QnUuid& engineId)
                {
                    d->refreshSettings(engineId);
                });
        }
        else
        {
            d->settingsListener.reset();
        }
    }
}

void DeviceAgentSettingsAdapter::applySettings()
{
    if (!d->settingsManager)
        return;

    if (!d->camera)
        return;

    const QnUuid& cameraId = d->camera->getId();

    const auto& valuesByEngineId = d->store->state().analytics.settingsValuesByEngineId;
    QHash<DeviceAgentId, QJsonObject> valuesToSet;

    for (auto it = valuesByEngineId.begin(); it != valuesByEngineId.end(); ++it)
    {
        if (it.value().hasUser())
            valuesToSet.insert(DeviceAgentId{cameraId, it.key()}, it.value().get());
    }

    d->settingsManager->setValues(valuesToSet);
    d->updateLoadingState();
}

} // namespace nx::vms::client::desktop
