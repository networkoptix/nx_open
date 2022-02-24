// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_media_settings_widget.h"
#include "ui_export_media_settings_widget.h"

#include <QtWidgets/QStyle>

namespace nx::vms::client::desktop {

ExportMediaSettingsWidget::ExportMediaSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportMediaSettingsWidget())
{
    ui->setupUi(this);

    connect(ui->filtersCheckBox, &QCheckBox::clicked,
        this, &ExportMediaSettingsWidget::emitDataEdited);
}

ExportMediaSettingsWidget::~ExportMediaSettingsWidget()
{
}

void ExportMediaSettingsWidget::emitDataEdited()
{
    Data data;
    data.applyFilters = ui->filtersCheckBox->isChecked();
    emit dataEdited(data);
}

void ExportMediaSettingsWidget::setData(const Data& data)
{
    ui->filtersCheckBox->setEnabled(data.transcodingAllowed);
    ui->filtersCheckBox->setChecked(data.applyFilters);
}

QLayout* ExportMediaSettingsWidget::passwordPlaceholder()
{
    return ui->passwordWidget->layout();
}

} // namespace nx::vms::client::desktop
