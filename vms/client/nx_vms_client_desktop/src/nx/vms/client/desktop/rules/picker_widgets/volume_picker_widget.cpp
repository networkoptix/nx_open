// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "volume_picker_widget.h"

#include "ui_volume_picker_widget.h"

namespace nx::vms::client::desktop::rules {

VolumePickerWidget::VolumePickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent),
    ui(new Ui::VolumePickerWidget)
{
    ui->setupUi(this);
}

VolumePickerWidget::~VolumePickerWidget()
{
}

void VolumePickerWidget::setReadOnly(bool value)
{
    ui->volumeSlider->setEnabled(!value);
    ui->testPushButton->setEnabled(!value);
}

void VolumePickerWidget::onDescriptorSet()
{
    ui->label->setText(fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
