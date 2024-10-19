// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/camera/buttons/aggregated_camera_button_controller.h>

namespace nx::vms::client::mobile {

class CameraButtonController: public core::AggregatedCameraButtonController
{
    Q_OBJECT
    using base_type = core::AggregatedCameraButtonController;

public:
    enum class ButtonGroup
    {
        ptz,
        objectTracking,
        twoWayAudio,
        softwareTriggers,
        extendedOutputs
    };
    Q_ENUM(ButtonGroup)

    static void registerQmlType();

    CameraButtonController(QObject* parent = nullptr);
};

} // namespace nx::vms::client::mobile
