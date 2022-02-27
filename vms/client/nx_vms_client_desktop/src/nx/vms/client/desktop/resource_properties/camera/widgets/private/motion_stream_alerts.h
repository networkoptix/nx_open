// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "../../flux/camera_settings_dialog_state.h"

namespace nx::vms::client::desktop {

class MotionStreamAlerts: public QObject
{
    Q_OBJECT
    using State = CameraSettingsDialogState;
    using MotionStreamAlert = State::MotionStreamAlert;

public:
    MotionStreamAlerts() = delete;
    static QString resolutionAlert(std::optional<MotionStreamAlert> alert);
    static QString implicitlyDisabledAlert(
        std::optional<MotionStreamAlert> alert, bool multiSelection = false);
};

} // namespace nx::vms::client::desktop
