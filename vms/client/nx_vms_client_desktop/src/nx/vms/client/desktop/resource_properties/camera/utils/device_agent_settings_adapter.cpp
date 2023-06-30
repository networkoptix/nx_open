// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent_settings_adapter.h"

#include <client/client_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/analytics/analytics_settings_manager.h>
#include <nx/vms/client/core/analytics/analytics_settings_multi_listener.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/analytics/analytics_actions_helper.h>
#include <nx/vms/client/desktop/common/utils/camera_web_authenticator.h>
#include <utils/common/delayed.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::common;

class DeviceAgentSettingsAdapter::Private
{
public:
    QWidget* parent = nullptr;
    CameraSettingsDialogStore* store = nullptr;
    QnVirtualCameraResourcePtr camera;
    core::AnalyticsSettingsManager* settingsManager = nullptr;
    std::unique_ptr<core::AnalyticsSettingsMultiListener> settingsListener;
};

DeviceAgentSettingsAdapter::DeviceAgentSettingsAdapter(
    CameraSettingsDialogStore* store,
    WindowContext* context,
    QWidget* parent)
    :
    base_type(parent),
    WindowContextAware(context),
    d(new Private())
{
    d->store = store;
    d->parent = parent;
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
            d->settingsListener = std::make_unique<core::AnalyticsSettingsMultiListener>(
                qnClientModule->analyticsSettingsManager(),
                camera,
                core::AnalyticsSettingsMultiListener::ListenPolicy::allEngines);

            connect(
                d->settingsListener.get(),
                &core::AnalyticsSettingsMultiListener::dataChanged,
                this,
                [this](const QnUuid& engineId, const core::DeviceAgentData& data)
                {
                    d->store->resetDeviceAgentData(engineId, data);
                });

            connect(
                d->settingsListener.get(),
                &core::AnalyticsSettingsMultiListener::previewDataReceived,
                this,
                [this](const QnUuid& engineId, const core::DeviceAgentData& data)
                {
                    d->store->resetDeviceAgentData(engineId, data, /*replaceUser*/ true);
                });

            connect(
                d->settingsListener.get(),
                &core::AnalyticsSettingsMultiListener::actionResultReceived,
                this,
                [this, camera](const QnUuid& engineId, const AnalyticsActionResult& result)
                {
                    const CameraSettingsDialogState& state = d->store->state();
                    std::shared_ptr<CameraWebAuthenticator> authenticator =
                        std::make_shared<CameraWebAuthenticator>(
                            camera,
                            result.actionUrl,
                            state.credentials.login.valueOr(QString{}),
                            state.credentials.password.valueOr(QString{}));

                    AnalyticsActionsHelper::processResult(
                        result,
                        windowContext(),
                        camera->resourcePool()->getResourceById(engineId),
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

    const QnUuid& cameraId = d->camera->getId();

    const auto& settingsByEngineId = d->store->state().analytics.settingsByEngineId;
    QHash<core::DeviceAgentId, QJsonObject> valuesToSet;

    for (auto it = settingsByEngineId.begin(); it != settingsByEngineId.end(); ++it)
    {
        if (it->values.hasUser())
        {
            valuesToSet.insert(core::DeviceAgentId{cameraId, it.key()}, it->values.get());
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
    for (const QnUuid& engineId: d->store->state().analytics.enabledEngines())
        d->settingsManager->refreshSettings(core::DeviceAgentId{cameraId, engineId});
}

std::unordered_map<QnUuid, core::DeviceAgentData> DeviceAgentSettingsAdapter::dataByEngineId() const
{
    std::unordered_map<QnUuid, core::DeviceAgentData> result;
    if (!d->settingsListener)
        return result;

    for (const auto& engineId: d->settingsListener->engineIds())
        result.emplace(engineId, d->settingsListener->data(engineId));
    return result;
}

} // namespace nx::vms::client::desktop
