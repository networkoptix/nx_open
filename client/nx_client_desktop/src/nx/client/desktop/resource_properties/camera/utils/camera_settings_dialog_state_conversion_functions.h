#pragma once

#include <core/resource/resource_fwd.h>

namespace nx::client::desktop {

struct CameraSettingsDialogState;

struct CameraSettingsDialogStateConversionFunctions
{
    static void applyStateToCameras(
        const CameraSettingsDialogState& state,
        const QnVirtualCameraResourceList& cameras);
};

} // namespace nx::client::desktop
