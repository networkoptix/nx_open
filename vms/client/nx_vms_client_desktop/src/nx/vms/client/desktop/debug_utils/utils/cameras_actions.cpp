// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cameras_actions.h"

// Windows header must be included before defining DeviceCapabilities to avoid compilation failure
// due to DeviceCapabilities define in the wingdi header (actual for no-pch build only).
#if defined(_WIN32)
    #include <windows.h>
#endif

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/data/device_model.h>
#include <ui/workbench/workbench_context.h>

#include "debug_custom_actions.h"

namespace nx::vms::client::desktop {

void CamerasActions::registerAction()
{
    registerDebugAction(
        "Cameras - Toggle default password",
        [](QnWorkbenchContext* context)
        {
            const auto cameras = context->resourcePool()->getAllCameras(
                QnResourcePtr(),
                true);
            if (cameras.empty())
                return;

            const auto caps =
                nx::vms::api::DeviceCapabilities(nx::vms::api::DeviceCapability::setUserPassword)
                | nx::vms::api::DeviceCapability::isDefaultPassword;

            const bool isDefaultPassword = cameras.first()->needsToChangeDefaultPassword();

            for (const auto& camera: cameras)
            {
                // Toggle current option.
                if (isDefaultPassword)
                    camera->setCameraCapabilities(camera->getCameraCapabilities() & ~caps);
                else
                    camera->setCameraCapabilities(camera->getCameraCapabilities() | caps);
                camera->savePropertiesAsync();
            }
        });

    registerDebugAction(
        "Cameras - Randomize Ptz",
        [](QnWorkbenchContext* context)
        {
            QList<Ptz::Capabilities> presets;
            presets.push_back(Ptz::NoPtzCapabilities);
            presets.push_back(Ptz::ContinuousZoomCapability);
            presets.push_back(Ptz::ContinuousZoomCapability | Ptz::ContinuousFocusCapability);
            presets.push_back(Ptz::ContinuousZoomCapability | Ptz::ContinuousFocusCapability
                | Ptz::AuxiliaryPtzCapability);
            presets.push_back(Ptz::ContinuousPanTiltCapabilities);
            presets.push_back(Ptz::ContinuousPtzCapabilities | Ptz::ContinuousFocusCapability
                | Ptz::AuxiliaryPtzCapability | Ptz::PresetsPtzCapability);

            for (const auto& camera: context->resourcePool()->getAllCameras(
                QnResourcePtr(),
                true))
            {
                int idx = presets.indexOf(camera->getPtzCapabilities());
                if (idx < 0)
                    idx = 0;
                else
                    idx = (idx + 1) % presets.size();

                camera->setPtzCapabilities(presets[idx]);
            }
        });
}

} // namespace nx::vms::client::desktop
