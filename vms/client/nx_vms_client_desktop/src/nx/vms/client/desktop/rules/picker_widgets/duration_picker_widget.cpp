// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "duration_picker_widget.h"

#include "ui_duration_picker_widget.h"

namespace nx::vms::client::desktop::rules {

DurationPickerWidget::DurationPickerWidget(QWidget* parent):
    PickerWidget(parent),
    ui(new Ui::DurationPickerWidget)
{
    ui->setupUi(this);
}

DurationPickerWidget::~DurationPickerWidget()
{
}

void DurationPickerWidget::setReadOnly(bool value)
{
    ui->timeDurationWidget->setEnabled(!value);
}

void DurationPickerWidget::onDescriptorSet()
{
    ui->label->setText(fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
