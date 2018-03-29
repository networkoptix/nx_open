#pragma once

#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;

struct CameraSettingsDialogStateConversionFunctions
{
    static void applyStateToCameras(
        const CameraSettingsDialogState& state,
        const QnVirtualCameraResourceList& cameras);
};

} // namespace desktop
} // namespace client
} // namespace nx
