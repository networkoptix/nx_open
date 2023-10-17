// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>

#include <nx/vms/client/desktop/window_context_aware.h>
#include <nx/vms/rules/action_builder_fields/volume_field.h>

#include "field_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class VolumePicker:
    public PlainFieldPickerWidget<vms::rules::VolumeField>,
    public WindowContextAware
{
    Q_OBJECT

public:
    VolumePicker(WindowContext* context, CommonParamsWidget* parent);

private:
    QSlider* m_volumeSlider = nullptr;
    QPushButton* m_testPushButton = nullptr;
    bool m_inProgress = false;
    float m_audioDeviceCachedVolume = 0.;

    void updateUi() override;

    void onVolumeChanged();
    void onTestButtonClicked();
    void onTextSaid();
};

} // namespace nx::vms::client::desktop::rules
