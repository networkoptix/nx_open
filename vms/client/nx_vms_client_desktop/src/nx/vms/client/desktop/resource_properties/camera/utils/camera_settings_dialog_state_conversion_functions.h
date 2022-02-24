// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;

class NX_VMS_CLIENT_DESKTOP_API CameraSettingsDialogStateConversionFunctions
{
    Q_DECLARE_TR_FUNCTIONS(nx::vms::client::desktop::CameraSettingsDialogStateConversionFunctions)

public:
    static void applyStateToCameras(
        const CameraSettingsDialogState& state,
        const QnVirtualCameraResourceList& cameras);
};

} // namespace nx::vms::client::desktop
