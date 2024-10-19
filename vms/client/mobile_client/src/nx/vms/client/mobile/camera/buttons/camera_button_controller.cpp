// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_button_controller.h"

#include <QtQml/QtQml>

#include <nx/vms/client/core/camera/buttons/extended_output_camera_button_controller.h>
#include <nx/vms/client/core/camera/buttons/software_trigger_camera_button_controller.h>
#include <nx/vms/client/core/camera/buttons/two_way_audio_camera_button_controller.h>
#include <nx/vms/client/mobile/camera/buttons/ptz_camera_button_controller.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::mobile {

void CameraButtonController::registerQmlType()
{
    qmlRegisterType<CameraButtonController>("nx.vms.client.mobile", 1, 0, "CameraButtonController");
}

CameraButtonController::CameraButtonController(QObject* parent):
    base_type(parent)
{
    const auto lazyInitialize =
        [this]()
        {
            addController<PtzCameraButtonController>(ButtonGroup::ptz);

            addController<core::TwoWayAudioCameraButtonController>(ButtonGroup::twoWayAudio);

            using HintStyle = core::SoftwareTriggerCameraButtonController::HintStyle;
            addController<core::SoftwareTriggerCameraButtonController>(
                ButtonGroup::softwareTriggers, HintStyle::mobile);

            const auto commonOutputs =
                api::ExtendedCameraOutputs(api::ExtendedCameraOutput::heater)
                | api::ExtendedCameraOutput::wiper
                | api::ExtendedCameraOutput::powerRelay;
            addController<core::ExtendedOutputCameraButtonController>(
                ButtonGroup::extendedOutputs, commonOutputs);

            addController<core::ExtendedOutputCameraButtonController>(
                ButtonGroup::objectTracking, api::ExtendedCameraOutput::autoTracking);
        };
    executeLater(lazyInitialize, this);
}

} // namespace nx::vms::client::mobile
