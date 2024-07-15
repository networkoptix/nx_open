// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/output_port_field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class OutputPortPicker: public DropdownTextPickerWidgetBase<vms::rules::OutputPortField>
{
public:
    using DropdownTextPickerWidgetBase<vms::rules::OutputPortField>::DropdownTextPickerWidgetBase;

private:
    void updateUi() override;
    void onActivated() override;
};

} // namespace nx::vms::client::desktop::rules
