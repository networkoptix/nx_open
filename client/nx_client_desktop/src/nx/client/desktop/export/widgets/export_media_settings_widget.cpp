#include "export_media_settings_widget.h"
#include "ui_export_media_settings_widget.h"

namespace nx {
namespace client {
namespace desktop {

ExportMediaSettingsWidget::ExportMediaSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ExportMediaSettingsWidget())
{
    ui->setupUi(this);
    connect(ui->filtersCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            ui->filtersCheckBox->repaint();
            emit dataChanged(checked);
        });
}

ExportMediaSettingsWidget::~ExportMediaSettingsWidget()
{
}

bool ExportMediaSettingsWidget::applyFilters() const
{
    return ui->filtersCheckBox->isChecked();
}

void ExportMediaSettingsWidget::setApplyFilters(bool value, bool suppressSignal)
{
    if (suppressSignal)
    {
        QSignalBlocker blocker(ui->filtersCheckBox);
        ui->filtersCheckBox->setChecked(value);
    }
    else
    {
        ui->filtersCheckBox->setChecked(value);
    }
}

bool ExportMediaSettingsWidget::transcodingAllowed() const
{
    return ui->filtersCheckBox->isEnabledTo(this);
}

void ExportMediaSettingsWidget::setTranscodingAllowed(bool value)
{
    ui->filtersCheckBox->setEnabled(value);
}

} // namespace desktop
} // namespace client
} // namespace nx
