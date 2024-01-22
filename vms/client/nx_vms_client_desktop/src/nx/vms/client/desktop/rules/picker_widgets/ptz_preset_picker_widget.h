// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_preset.h>
#include <nx/vms/rules/action_builder_fields/ptz_preset_field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class PtzPresetPicker: public DropdownTextPickerWidgetBase<vms::rules::PtzPresetField>
{
    Q_OBJECT

public:
    PtzPresetPicker(SystemContext* context, ParamsWidget* parent);

private:
    QnPtzControllerPtr m_ptzController;
    QnPtzPresetList m_presets;

    void updateUi() override;
    void onCurrentIndexChanged() override;
    void updateComboBox();
};

} // namespace nx::vms::client::desktop::rules
