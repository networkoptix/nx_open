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
    NX_ASSERT(d->settingsManager);
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
                qnClientModule->analyticsSettingsManager(),
                camera,
                AnalyticsSettingsMultiListener::ListenPolicy::allEngines);

            connect(d->settingsListener.get(), &AnalyticsSettingsMultiListener::dataChanged,
                this,
                [this](const QnUuid& engineId, const DeviceAgentData& data)
                {
                    d->store->resetDeviceAgentData(engineId, data);
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

void DeviceAgentSettingsAdapter::refreshSettings()
{
    if (!d->settingsManager)
        return;

    if (!d->camera)
        return;

    const QnUuid& cameraId = d->camera->getId();
    QSet<QnUuid> enabledEngines = d->store->state().analytics.enabledEngines.get();
    for (const QnUuid& engineId: enabledEngines)
        d->settingsManager->refreshSettings(DeviceAgentId{cameraId, engineId});
}

std::unordered_map<QnUuid, DeviceAgentData> DeviceAgentSettingsAdapter::dataByEngineId() const
{
    std::unordered_map<QnUuid, DeviceAgentData> result;
    if (!d->settingsListener)
        return result;

    for (const auto& engineId: d->settingsListener->engineIds())
        result.emplace(engineId, d->settingsListener->data(engineId));
    return result;
}

} // namespace nx::vms::client::desktop
