// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sound_picker_widget.h"

#include "ui_sound_picker_widget.h"

namespace nx::vms::client::desktop::rules {

SoundPickerWidget::SoundPickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent),
    ui(new Ui::SoundPickerWidget)
{
    ui->setupUi(this);
}

SoundPickerWidget::~SoundPickerWidget()
{
}

void SoundPickerWidget::setReadOnly(bool value)
{
    ui->soundComboBox->setEnabled(!value);
    ui->managePushButton->setEnabled(!value);
}

void SoundPickerWidget::onDescriptorSet()
{
    ui->label->setText(fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
