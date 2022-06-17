// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "quality_picker_widget.h"

#include "ui_quality_picker_widget.h"

namespace nx::vms::client::desktop::rules {

QualityPickerWidget::QualityPickerWidget(common::SystemContext* context, QWidget* parent):
    PickerWidget(context, parent),
    ui(new Ui::QualityPickerWidget)
{
    ui->setupUi(this);
}

QualityPickerWidget::~QualityPickerWidget()
{
}

void QualityPickerWidget::setReadOnly(bool value)
{
    ui->qualityComboBox->setEnabled(!value);
}

void QualityPickerWidget::onDescriptorSet()
{
    ui->label->setText(fieldDescriptor->displayName);
}

} // namespace nx::vms::client::desktop::rules
