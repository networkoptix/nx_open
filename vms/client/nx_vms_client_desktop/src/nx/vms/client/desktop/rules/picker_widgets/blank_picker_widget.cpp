// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "blank_picker_widget.h"

namespace nx::vms::client::desktop::rules {

BlankPickerWidget::BlankPickerWidget(QWidget* parent):
    PickerWidget(parent)
{
}

void BlankPickerWidget::setReadOnly(bool /*value*/)
{
}

void BlankPickerWidget::onDescriptorSet()
{
}

} // namespace nx::vms::client::desktop::rules
