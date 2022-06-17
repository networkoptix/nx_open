// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "input_id_picker_widget.h"

#include "ui_input_id_picker_widget.h"

namespace nx::vms::client::desktop::rules {

InputIDPickerWidget::InputIDPickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent),
    ui(new Ui::InputIDPickerWidget)
{
    ui->setupUi(this);
}

InputIDPickerWidget::~InputIDPickerWidget()
{
}

void InputIDPickerWidget::setReadOnly(bool value)
{
    ui->inputIDComboBox->setEnabled(!value);
}

void InputIDPickerWidget::onDescriptorSet()
{
    ui->label->setText(fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
