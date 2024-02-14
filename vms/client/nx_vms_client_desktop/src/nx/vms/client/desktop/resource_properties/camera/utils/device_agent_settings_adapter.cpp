// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent_settings_adapter.h"

#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/analytics/analytics_actions_helper.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_multi_listener.h>
#include <nx/vms/client/desktop/common/utils/camera_web_authenticator.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::common;

class DeviceAgentSettingsAdapter::Private: public nx::vms::client::core::CommonModuleAware
{
public:
    QWidget* parent = nullptr;
    QnWorkbenchContext* context = nullptr;
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    AnalyticsSettingsManager* settingsManager = nullptr;
    std::unique_ptr<AnalyticsSettingsMultiListener> settingsListener;
};

DeviceAgentSettingsAdapter::DeviceAgentSettingsAdapter(
    CameraSettingsDialogStore* store,
    QnWorkbenchContext* context,
    QWidget* parent)
    :
    base_type(parent),
    d(new Private())
{
    d->store = store;
    d->parent = parent;
    d->context = context;
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

            connect(
                d->settingsListener.get(),
                &AnalyticsSettingsMultiListener::dataChanged,
                this,
                [this](const nx::Uuid& engineId, const DeviceAgentData& data)
                {
                    d->store->resetDeviceAgentData(engineId, data);
                });

            connect(
                d->settingsListener.get(),
                &AnalyticsSettingsMultiListener::previewDataReceived,
                this,
                [this](const nx::Uuid& engineId, const DeviceAgentData& data)
                {
                    d->store->resetDeviceAgentData(engineId, data, /*replaceUser*/ true);
                });

            connect(
                d->settingsListener.get(),
                &AnalyticsSettingsMultiListener::actionResultReceived,
                this,
                [this, camera](const nx::Uuid& engineId, const AnalyticsActionResult& result)
                {
                    const CameraSettingsDialogState& state = d->store->state();
                    std::shared_ptr<CameraWebAuthenticator> authenticator = std::make_shared<CameraWebAuthenticator>(
                        d->context->systemContext(),
                        camera,
                        result.actionUrl,
                        state.credentials.login.valueOr(QString{}),
                        state.credentials.password.valueOr(QString{}));

                    AnalyticsActionsHelper::processResult(
                        result,
                        d->context,
                        d->resourcePool()->getResourceById(engineId),
                        authenticator,
                        d->parent);
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

    const nx::Uuid& cameraId = d->camera->getId();

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

    const nx::Uuid& cameraId = d->camera->getId();
    for (const nx::Uuid& engineId: d->store->state().analytics.enabledEngines())
        d->settingsManager->refreshSettings(DeviceAgentId{cameraId, engineId});
}

std::unordered_map<nx::Uuid, DeviceAgentData> DeviceAgentSettingsAdapter::dataByEngineId() const
{
    std::unordered_map<nx::Uuid, DeviceAgentData> result;
    if (!d->settingsListener)
        return result;

    for (const auto& engineId: d->settingsListener->engineIds())
        result.emplace(engineId, d->settingsListener->data(engineId));
    return result;
}

} // namespace nx::vms::client::desktop
