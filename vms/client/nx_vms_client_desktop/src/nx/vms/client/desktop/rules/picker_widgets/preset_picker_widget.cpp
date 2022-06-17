// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "preset_picker_widget.h"

#include "ui_preset_picker_widget.h"

namespace nx::vms::client::desktop::rules {

PresetPickerWidget::PresetPickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent),
    ui(new Ui::PresetPickerWidget)
{
    ui->setupUi(this);
}

PresetPickerWidget::~PresetPickerWidget()
{
}

void PresetPickerWidget::setReadOnly(bool value)
{
    ui->presetComboBox->setEnabled(!value);
}

void PresetPickerWidget::onDescriptorSet()
{
    ui->label->setText(fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
