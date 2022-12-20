// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "blank_picker_widget.h"

namespace nx::vms::client::desktop::rules {

BlankPickerWidget::BlankPickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent)
{
    setVisible(false);
}

void BlankPickerWidget::onDescriptorSet()
{
}

} // namespace nx::vms::client::desktop::rules
