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

    NX_VERBOSE(this, "Refreshing settings");
    updateLoadingState();

    const QJsonObject& model = settingsListener->model(engineId);
    if (model.isEmpty())
        return;

    store->setDeviceAgentSettingsModel(engineId, model);
    const auto actualValues = settingsListener->values(engineId);
    NX_VERBOSE(this, "Reset actual values to store:\n%1", actualValues);
    store->resetDeviceAgentSettingsValues(engineId, actualValues);
}

void DeviceAgentSettingsAdapter::Private::updateLoadingState()
{
    bool loading = false;

    if (settingsManager->isApplyingChanges())
    {
        NX_VERBOSE(this, "Keep loading state as apply is in progress");
        loading = true;
    }
    else if (!currentEngineId.isNull() && settingsListener->model(currentEngineId).isEmpty())
    {
        NX_VERBOSE(this, "Set loading state as values are still not loaded");
        loading = true;
    }
    else
    {
        NX_VERBOSE(this, "Reset loading state");
    }

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
            NX_VERBOSE(this, "Current engine changed to %1, refreshing settings", id);
            executeDelayedParented([this, id]() { d->refreshSettings(id); }, this);
        });

    connect(d->settingsManager, &AnalyticsSettingsManager::appliedChanges, this,
        [this]() { d->updateLoadingState(); });
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
            d->settingsListener = std::make_unique<AnalyticsSettingsMultiListener>(
                camera,
                AnalyticsSettingsMultiListener::ListenPolicy::allEngines);

            if (!d->currentEngineId.isNull())
                d->refreshSettings(d->currentEngineId);

            connect(d->settingsListener.get(), &AnalyticsSettingsMultiListener::modelChanged,
                this,
                [this](const QnUuid& engineId, const QJsonObject& model)
                {
                    if (!d->store->deviceAgentSettingsModel(engineId).isEmpty())
                    {
                        d->store->setDeviceAgentSettingsModel(engineId, model);
                        const auto actualValues = d->settingsListener->values(engineId);
                        NX_VERBOSE(this, "Model changed, reloading actual values to store:\n%1",
                            actualValues);
                        d->store->resetDeviceAgentSettingsValues(engineId, actualValues);
                        d->updateLoadingState();
                    }
                });

            connect(d->settingsListener.get(), &AnalyticsSettingsMultiListener::valuesChanged,
                this,
                [this](const QnUuid& engineId)
                {
                    NX_VERBOSE(this, "Settings values changed, load them to store");
                    d->refreshSettings(engineId);
                });
        }
        else
        {
            d->settingsListener.reset();
        }
    }
    else if (camera && !d->currentEngineId.isNull())
    {
        // Refresh settings even if camera is not changed. This happens when the dialog is re-opened
        // or instantly after we have applied the changes.
        d->refreshSettings(d->currentEngineId);
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
        {
            valuesToSet.insert(DeviceAgentId{cameraId, it.key()}, it.value().get());
            NX_VERBOSE(this, "Applying changes:\n%1", it.value().get());
        }
    }

    d->settingsManager->applyChanges(valuesToSet);
    d->updateLoadingState();
}

} // namespace nx::vms::client::desktop
