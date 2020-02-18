#include "device_agent_settings_adapter.h"

#include <client/client_module.h>

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
    //QnUuid currentEngineId;
    std::unique_ptr<AnalyticsSettingsMultiListener> settingsListener;
};

DeviceAgentSettingsAdapter::DeviceAgentSettingsAdapter(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->store = store;

    d->settingsManager = qnClientModule->analyticsSettingsManager();
    if (!NX_ASSERT(d->settingsManager))
        return;

    //d->currentEngineId = d->store->currentAnalyticsEngineId();

    //connect(d->store, &CameraSettingsDialogStore::stateChanged, this,
    //    [this]()
    //    {
    //        const auto id = d->store->currentAnalyticsEngineId();
    //        if (d->currentEngineId == id)
    //            return;

    //        d->currentEngineId = id;
    //        NX_VERBOSE(this, "Current engine changed to %1, refreshing settings", id);
    //        executeDelayedParented([this, id]() { d->refreshSettings(id); }, this);
    //    });
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

            connect(d->settingsListener.get(), &AnalyticsSettingsMultiListener::dataChanged,
                this,
                [this](const QnUuid& engineId, const DeviceAgentData& data)
                {
                    d->store->resetDeviceAgentData(engineId, data);
                });

            for (const auto& engineId: d->settingsListener->engineIds())
                d->store->resetDeviceAgentData(engineId, d->settingsListener->data(engineId));
        }
        else
        {
            d->settingsListener.reset();
        }
    }

    //else if (camera && !d->currentEngineId.isNull())
    //{
        // Refresh settings even if camera is not changed. This happens when the dialog is re-opened
        // or instantly after we have applied the changes.
    //    d->refreshSettings(d->currentEngineId);
    //}

}

void DeviceAgentSettingsAdapter::applySettings()
{
    if (!d->settingsManager)
        return;

    if (!d->camera)
        return;

    const QnUuid& cameraId = d->camera->getId();

    const auto& settingsByEngineId = d->store->state().analytics.settingsByEngineId;
    QHash<DeviceAgentId, QJsonObject> valuesToSet;

    for (auto it = settingsByEngineId.begin(); it != settingsByEngineId.end(); ++it)
    {
        if (it->values.hasUser())
        {
            valuesToSet.insert(DeviceAgentId{cameraId, it.key()}, it->values.get());
            NX_VERBOSE(this, "Applying changes:\n%1", it->values.get());
        }
    }

    d->settingsManager->applyChanges(valuesToSet);
}

} // namespace nx::vms::client::desktop
