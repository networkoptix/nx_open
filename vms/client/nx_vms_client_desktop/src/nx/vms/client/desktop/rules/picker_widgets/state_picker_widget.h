// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/event_filter_fields/state_field.h>

#include "../utils/state_combo_box.h"
#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class StatePicker: public DropdownTextPickerWidgetBase<vms::rules::StateField, StateComboBox>
{
    Q_OBJECT

public:
    StatePicker(vms::rules::StateField* field, SystemContext* context, ParamsWidget* parent);

protected:
    void updateUi() override;
    void onActivated() override;
};

} // namespace nx::vms::client::desktop::rules
