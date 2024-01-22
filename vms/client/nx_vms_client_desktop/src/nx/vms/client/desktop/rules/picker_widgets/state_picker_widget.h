// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/event_filter_fields/state_field.h>
#include <nx/vms/rules/utils/field.h>

#include "dropdown_text_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class StatePicker: public DropdownTextPickerWidgetBase<vms::rules::StateField>
{
    Q_OBJECT

public:
    using DropdownTextPickerWidgetBase<vms::rules::StateField>::DropdownTextPickerWidgetBase;

protected:
    void onDescriptorSet() override;
    void updateUi() override;
    void onCurrentIndexChanged() override;
};

} // namespace nx::vms::client::desktop::rules
